/// @file
/// @brief Forge's debugger natives and adapter grants (Debug.hpp).

#include "features/debug/Debug.hpp"
#include "features/lsp/Lsp.hpp" // findProgram

#include <affresco/core/Json.hpp>
#include <affresco/core/Services.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace forge {

namespace {

using affresco::json;
namespace fs = std::filesystem;

constexpr std::uintmax_t largestLaunchFile = std::uintmax_t{1} << 20; ///< launch.json, at most 1 MiB.
constexpr std::size_t mostConfigurations = 100;                       ///< Configurations read from it.
constexpr std::size_t mostWords = 1000;                               ///< Arguments or variables in a list.
constexpr std::size_t longestWord = 4096;                             ///< One argument, path or value, in bytes.
constexpr std::size_t longestLine = 60000; ///< A terminal's command line: under the pty's 64 KiB write.

/// @brief Where a JSON comment that starts at `i` ends.
/// @param text The text.
/// @param i Where to look.
/// @return The index of its last character, or npos if no comment starts there.
std::size_t commentEnd(std::string_view text, std::size_t i) {
	if(text[i] != '/' || i + 1 >= text.size()) return std::string_view::npos;
	if(text[i + 1] == '/') return std::min(text.find('\n', i), text.size() - 1);
	if(text[i + 1] != '*') return std::string_view::npos;
	const std::size_t end = text.find("*/", i + 2);
	return end == std::string_view::npos ? text.size() - 1 : end + 1;
}

/// @brief Where a JSON string that starts at `i` ends.
/// @param text The text.
/// @param i Its opening quote.
/// @return The index of its closing quote (or the last character, if it isn't closed).
std::size_t stringEnd(std::string_view text, std::size_t i) {
	for(std::size_t j = i + 1; j < text.size(); ++j) {
		if(text[j] == '\\') ++j;
		else if(text[j] == '"') return j;
	}
	return text.size() - 1;
}

/// @brief VS Code's JSON with comments as plain JSON: its comments and trailing commas left out.
/// @param text The text.
/// @return Plain JSON (or text that still isn't JSON).
std::string plainJson(std::string_view text) {
	std::string out;
	out.reserve(text.size());
	std::size_t pendingComma = std::string::npos; // where a comma waits until the next token says if it trails
	for(std::size_t i = 0; i < text.size(); ++i) {
		const char c = text[i];
		if(c == '"') {
			const std::size_t end = stringEnd(text, i);
			out.append(text.substr(i, end - i + 1));
			i = end;
			pendingComma = std::string::npos;
			continue;
		}
		if(const std::size_t end = commentEnd(text, i); end != std::string_view::npos) {
			i = end;
			continue;
		}
		if((c == '}' || c == ']') && pendingComma != std::string::npos) out[pendingComma] = ' ';
		if(c == ',') pendingComma = out.size();
		else if(c != ' ' && c != '\t' && c != '\n' && c != '\r') pendingComma = std::string::npos;
		out.push_back(c);
	}
	return out;
}

/// @brief Whether a text is a usable word: at most longestWord bytes, with no control character.
/// @param text The text.
/// @return Whether it is.
bool plainWord(std::string_view text) {
	return text.size() <= longestWord && std::ranges::none_of(text, [](char c) {
		       return static_cast<unsigned char>(c) < 0x20 || c == 0x7f;
	       });
}

/// @brief The plain words of a list, up to mostWords of them; the others are left out.
/// @param list A JSON array.
/// @return The words.
json plainWords(const json& list) {
	json words = json::array();
	for(const json& word : list)
		if(word.is_string() && plainWord(word.get_ref<const std::string&>()) && words.size() < mostWords)
			words.push_back(word);
	return words;
}

/// @brief The variables of a map whose names and values are plain words, up to mostWords of them.
/// @param map A JSON object.
/// @return The variables.
json plainVariables(const json& map) {
	json variables = json::object();
	for(const auto& [name, value] : map.items())
		if(value.is_string() && plainWord(name) && plainWord(value.get_ref<const std::string&>()) &&
		   variables.size() < mostWords)
			variables[name] = value;
	return variables;
}

/// @brief Copies the plain keys Forge knows from a configuration to an adapter's arguments: text (`program`, `cwd`),
/// a flag (`stopOnEntry`), the text list `args` and the text map `env`. Others, and those of the wrong kind, are left
/// out.
/// @param configuration The configuration.
/// @param arguments The arguments, to add to.
void copyKnownKeys(const json& configuration, json& arguments) {
	for(const char* key : {"program", "cwd"})
		if(const auto it = configuration.find(key);
		   it != configuration.end() && it->is_string() && plainWord(it->get_ref<const std::string&>()))
			arguments[key] = *it;
	if(const auto it = configuration.find("stopOnEntry"); it != configuration.end() && it->is_boolean())
		arguments["stopOnEntry"] = *it;
	if(const auto it = configuration.find("args"); it != configuration.end() && it->is_array())
		arguments["args"] = plainWords(*it);
	if(const auto it = configuration.find("env"); it != configuration.end() && it->is_object())
		arguments["env"] = plainVariables(*it);
}

/// @brief The port a debugpy attach names: `port`, or `connect.port`.
/// @param configuration The configuration.
/// @return The port, or 0 if there is none in 1..65535.
int attachPort(const json& configuration) {
	const json* port = nullptr;
	if(configuration.contains("port")) port = &configuration["port"];
	else if(configuration.contains("connect") && configuration["connect"].is_object() &&
	        configuration["connect"].contains("port"))
		port = &configuration["connect"]["port"];
	if(port == nullptr || !port->is_number_integer()) return 0;
	const auto value = port->get<long long>();
	return value >= 1 && value <= 65535 ? static_cast<int>(value) : 0;
}

/// @brief A text key of a configuration.
/// @param configuration The configuration.
/// @param key The key.
/// @return Its text, or "" if it is missing or not text.
std::string textOf(const json& configuration, const char* key) {
	const auto it = configuration.find(key);
	return it != configuration.end() && it->is_string() ? it->get<std::string>() : std::string();
}

/// @brief The adapter Forge runs for a configuration's `type`.
/// @param type The type.
/// @return "lldb", "python", or null for a type Forge doesn't run.
const char* adapterFor(std::string_view type) {
	if(type == "lldb" || type == "lldb-dap") return "lldb";
	if(type == "python" || type == "debugpy") return "python";
	return nullptr;
}

/// @brief Copies where a configuration's program runs: debugpy takes `console` itself, lldb-dap a `runInTerminal`
/// flag for either terminal.
/// @param configuration The configuration.
/// @param python Whether the adapter is debugpy.
/// @param arguments The arguments, to add to.
void copyConsole(const json& configuration, bool python, json& arguments) {
	const auto it = configuration.find("console");
	if(it == configuration.end() || !it->is_string()) return;
	const bool terminal = *it == "integratedTerminal" || *it == "externalTerminal";
	if(python) arguments["console"] = terminal ? "integratedTerminal" : "internalConsole";
	else if(terminal) arguments["runInTerminal"] = true;
}

/// @brief Adds what a configuration runs to an adapter's arguments: the local port debugpy attaches to, the process
/// lldb attaches to, or (on launch) nothing, since copyKnownKeys copied the program.
/// @param configuration The configuration.
/// @param python Whether the adapter is debugpy.
/// @param request "launch" or "attach".
/// @param arguments The arguments, to add to.
/// @return Whether the configuration names something to run.
bool addTarget(const json& configuration, bool python, std::string_view request, json& arguments) {
	if(request != "attach") return arguments.contains("program");
	if(python) {
		const int port = attachPort(configuration);
		if(port == 0) return false;
		arguments["connect"] = {{"host", "127.0.0.1"}, {"port", port}};
		return true;
	}
	const auto it = configuration.find("pid");
	if(it == configuration.end() || !it->is_number_integer() || it->get<long long>() <= 0) return false;
	arguments["pid"] = *it;
	return true;
}

/// @brief One configuration as Forge runs it.
/// @param configuration A launch.json configuration.
/// @return `{ "name", "type", "request", "arguments" }`, or null if Forge can't run it.
json configurationFor(const json& configuration) {
	if(!configuration.is_object()) return nullptr;
	const std::string request = textOf(configuration, "request");
	const char* adapter = adapterFor(textOf(configuration, "type"));
	if(adapter == nullptr || (request != "launch" && request != "attach")) return nullptr;
	const bool python = std::string_view(adapter) == "python";
	json arguments = json::object();
	copyKnownKeys(configuration, arguments);
	copyConsole(configuration, python, arguments);
	if(!addTarget(configuration, python, request, arguments)) return nullptr;
	std::string name = textOf(configuration, "name");
	name = plainWord(name) ? name.substr(0, 200) : std::string();
	if(name.empty()) name = std::string(adapter) + " " + request;
	return {{"name", name}, {"type", adapter}, {"request", request}, {"arguments", std::move(arguments)}};
}

/// @brief A word quoted for a POSIX shell: in single quotes, a quote as `'\''`.
/// @param word The word.
/// @return It, quoted.
std::string shellWord(std::string_view word) {
	std::string out = "'";
	for(const char c : word) {
		if(c == '\'') out += "'\\''";
		else out.push_back(c);
	}
	return out + "'";
}

/// @brief A word of a `runInTerminal` request.
/// @param value The value.
/// @param what What it is, for the error.
/// @return It.
/// @throws std::runtime_error If it isn't plain text.
const std::string& wordOf(const json& value, std::string_view what) {
	if(!value.is_string() || !plainWord(value.get_ref<const std::string&>()))
		throw std::runtime_error("the adapter's " + std::string(what) + " is not plain text");
	return value.get_ref<const std::string&>();
}

/// @brief The folder a `runInTerminal` request runs in.
/// @param root The folder the debugger runs in.
/// @param request The request's arguments.
/// @return Its canonical path, inside root.
/// @throws std::runtime_error If it leaves root.
fs::path workingFolder(const fs::path& root, const json& request) {
	std::error_code error;
	fs::path base = fs::weakly_canonical(root, error);
	if(error || base.empty()) throw std::runtime_error("the debugger has no folder");
	const auto it = request.is_object() ? request.find("cwd") : request.end();
	// NOLINTNEXTLINE(readability-container-size-empty): a JSON string is never empty() to nlohmann::json
	if(!request.is_object() || it == request.end() || it->is_null() || *it == "") return base;
	const fs::path folder = fs::weakly_canonical(base / fs::path(wordOf(*it, "folder")), error);
	const fs::path relative = folder.lexically_relative(base);
	if(error || relative.empty() || *relative.begin() == "..")
		throw std::runtime_error("the adapter asked to run a program outside the folder");
	return folder;
}

/// @brief Whether a text is a name an environment variable may have: a letter or `_`, then letters, digits or `_`.
/// @param name The text.
/// @return Whether it is.
bool variableName(std::string_view name) {
	const auto word = [](char c) { return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
	return !name.empty() && name.size() <= 200 && word(name.front()) &&
	       std::ranges::all_of(name, [&word](char c) { return word(c) || (c >= '0' && c <= '9'); });
}

/// @brief Reads a file of a folder's `.vscode`, comments allowed.
/// @param root The open folder, or empty.
/// @param name The file's name, such as `launch.json`.
/// @return Its JSON, or null when there is no folder or no file.
/// @throws std::runtime_error If the file isn't JSON or is too large.
json folderSettingsFile(const fs::path& root, const std::string& name) {
	if(root.empty()) return nullptr;
	const fs::path file = root / ".vscode" / name;
	std::error_code error;
	if(!fs::is_regular_file(file, error)) return nullptr;
	if(fs::file_size(file, error) > largestLaunchFile || error)
		throw std::runtime_error(".vscode/" + name + " is too large");
	std::ifstream in(file, std::ios::binary);
	const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	try {
		return affresco::parseUntrusted(plainJson(text));
	} catch(const std::exception&) {
		throw std::runtime_error(".vscode/" + name + " isn't JSON");
	}
}

/// @brief A word of a task with `${workspaceFolder}` made `.` (its terminal starts in the folder).
/// @param value The word.
/// @return It, or nullopt if it isn't plain text or names another variable.
std::optional<std::string> taskWord(const json& value) {
	if(!value.is_string() || !plainWord(value.get_ref<const std::string&>())) return std::nullopt;
	std::string word = value.get<std::string>();
	constexpr std::string_view folderVariable = "${workspaceFolder}";
	for(std::size_t at = word.find(folderVariable); at != std::string::npos; at = word.find(folderVariable, at + 1))
		word.replace(at, folderVariable.size(), ".");
	if(word.contains("${")) return std::nullopt;
	return word;
}

/// @brief The group a task is in: `build`, `test` or "".
/// @param task The task.
/// @return The group.
std::string taskGroup(const json& task) {
	const auto it = task.find("group");
	if(it == task.end()) return "";
	const json& kind = it->is_object() ? it->value("kind", json()) : *it;
	return kind == "build" || kind == "test" ? kind.get<std::string>() : "";
}

/// @brief The line a task's terminal runs.
/// @param task The task.
/// @param command Its command, its variables replaced.
/// @return The line, or nullopt if an argument isn't plain text or the line is too long.
std::optional<std::string> taskLine(const json& task, const std::string& command) {
	// a shell task's command is a line for the shell; a process task's is one program
	std::string line = task.value("type", json("shell")) == "shell" ? command : shellWord(command);
	if(const auto args = task.find("args"); args != task.end()) {
		if(!args->is_array() || args->size() > mostWords) return std::nullopt;
		for(const json& arg : *args) {
			const std::optional<std::string> word = taskWord(arg);
			if(!word) return std::nullopt;
			line += " " + shellWord(*word);
		}
	}
	if(line.size() > longestLine) return std::nullopt;
	return line;
}

/// @brief One task of a tasks.json, in the subset Forge runs: `label`, `type` (`shell` or `process`), `command`,
/// `args` and `group`.
/// @param task The task.
/// @return `{ "label", "line", "group" }`, or null if Forge can't run it.
json taskFor(const json& task) {
	if(!task.is_object() || !task.contains("command")) return nullptr;
	const json type = task.value("type", json("shell"));
	if(type != "shell" && type != "process") return nullptr;
	const std::optional<std::string> command = taskWord(task["command"]);
	if(!command || command->empty()) return nullptr;
	const std::optional<std::string> line = taskLine(task, *command);
	if(!line) return nullptr;
	const json label = task.value("label", json());
	const bool named = label.is_string() && plainWord(label.get_ref<const std::string&>()) &&
	                   !label.get_ref<const std::string&>().empty();
	return {{"label", named ? label.get<std::string>() : *command}, {"line", *line}, {"group", taskGroup(task)}};
}

} // namespace

json launchConfigurations(const fs::path& root) {
	json result = {{"configurations", json::array()}, {"skipped", 0}};
	const json launch = folderSettingsFile(root, "launch.json");
	if(!launch.is_object() || !launch.contains("configurations") || !launch["configurations"].is_array())
		return result;
	int skipped = 0;
	for(const json& configuration : launch["configurations"]) {
		json known = result["configurations"].size() < mostConfigurations ? configurationFor(configuration) : nullptr;
		if(known.is_null()) ++skipped;
		else result["configurations"].push_back(std::move(known));
	}
	result["skipped"] = skipped;
	return result;
}

json taskList(const fs::path& root) {
	json result = {{"tasks", json::array()}, {"skipped", 0}};
	const json file = folderSettingsFile(root, "tasks.json");
	if(!file.is_object() || !file.contains("tasks") || !file["tasks"].is_array()) return result;
	int skipped = 0;
	for(const json& task : file["tasks"]) {
		json known = result["tasks"].size() < mostConfigurations ? taskFor(task) : nullptr;
		if(known.is_null()) ++skipped;
		else result["tasks"].push_back(std::move(known));
	}
	result["skipped"] = skipped;
	return result;
}

std::string terminalCommandLine(const fs::path& root, const json& request) {
	std::string line = "cd " + shellWord(workingFolder(root, request).string()) + " && exec env";
	if(const auto it = request.find("env"); it != request.end() && it->is_object()) {
		for(const auto& [name, value] : it->items()) {
			if(value.is_null()) continue;
			if(!variableName(name)) throw std::runtime_error("the adapter named an environment variable badly");
			line += " " + shellWord(name + "=" + wordOf(value, "environment"));
		}
	}
	const auto args = request.find("args");
	if(args == request.end() || !args->is_array() || args->empty() || args->size() > mostWords)
		throw std::runtime_error("the adapter named no program to run");
	for(const json& word : *args) line += " " + shellWord(wordOf(word, "argument"));
	if(line.size() > longestLine) throw std::runtime_error("the adapter's command line is too long");
	return line;
}

void registerDebug(affresco::Screen& screen, const std::shared_ptr<Workspace>& workspace, bool grantAdapters) {
	if(const fs::path lldb = findProgram("lldb-dap"); grantAdapters && !lldb.empty())
		screen.services().allowProcess("lldb-dap", lldb);
	if(const fs::path python = findProgram("python3"); grantAdapters && !python.empty())
		screen.services().allowProcess("debugpy", python, {"-m", "debugpy.adapter"});
	// a grant's root is fixed when the screen loads, so the folder open then, like the terminal's
	const fs::path folder = workspace->root.empty() ? fs::current_path() : workspace->root;
	screen.services().addRoot("debug", folder);
	screen.onCall("forge.debug.launchConfigurations",
	              [workspace](const std::string&) { return launchConfigurations(workspace->root).dump(); });
	screen.onCall("forge.tasks.list", [workspace](const std::string&) { return taskList(workspace->root).dump(); });
	screen.onCall("forge.debug.commandLine", [folder = folder](const std::string& arguments) {
		const json request = affresco::parseUntrusted(arguments);
		if(!request.is_object()) throw std::runtime_error("the adapter's request is not an object");
		return json{{"line", terminalCommandLine(folder, request)}}.dump();
	});
}

} // namespace forge
