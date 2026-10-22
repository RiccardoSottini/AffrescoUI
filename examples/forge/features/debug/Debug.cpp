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
	if(const auto it = configuration.find("args"); it != configuration.end() && it->is_array()) {
		json words = json::array();
		for(const json& word : *it)
			if(word.is_string() && plainWord(word.get_ref<const std::string&>()) && words.size() < mostWords)
				words.push_back(word);
		arguments["args"] = std::move(words);
	}
	if(const auto it = configuration.find("env"); it != configuration.end() && it->is_object()) {
		json variables = json::object();
		for(const auto& [name, value] : it->items())
			if(value.is_string() && plainWord(name) && plainWord(value.get_ref<const std::string&>()) &&
			   variables.size() < mostWords)
				variables[name] = value;
		arguments["env"] = std::move(variables);
	}
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

/// @brief One configuration as Forge runs it.
/// @param configuration A launch.json configuration.
/// @return `{ "name", "type", "request", "arguments" }`, or null if Forge can't run it.
json configurationFor(const json& configuration) {
	if(!configuration.is_object()) return nullptr;
	const std::string type = configuration.value("type", json("")).is_string() ? configuration.value("type", "") : "";
	const std::string request =
	    configuration.value("request", json("")).is_string() ? configuration.value("request", "") : "";
	const char* adapter = type == "lldb" || type == "lldb-dap" ? "lldb"
	                      : type == "python" || type == "debugpy" ? "python"
	                                                              : nullptr;
	if(adapter == nullptr || (request != "launch" && request != "attach")) return nullptr;
	const json& nameValue = configuration.value("name", json(""));
	std::string name = nameValue.is_string() && plainWord(nameValue.get_ref<const std::string&>())
	                       ? nameValue.get<std::string>().substr(0, 200)
	                       : std::string();
	json arguments = json::object();
	copyKnownKeys(configuration, arguments);
	const bool python = std::string_view(adapter) == "python";
	if(const auto it = configuration.find("console"); it != configuration.end() && it->is_string()) {
		const bool terminal = *it == "integratedTerminal" || *it == "externalTerminal";
		if(python) arguments["console"] = terminal ? "integratedTerminal" : "internalConsole";
		else if(terminal) arguments["runInTerminal"] = true;
	}
	if(request == "attach" && python) {
		const int port = attachPort(configuration);
		if(port == 0) return nullptr;
		arguments["connect"] = {{"host", "127.0.0.1"}, {"port", port}};
	} else if(request == "attach") {
		const auto it = configuration.find("pid");
		if(it == configuration.end() || !it->is_number_integer() || it->get<long long>() <= 0) return nullptr;
		arguments["pid"] = *it;
	} else if(!arguments.contains("program")) return nullptr;
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
	const fs::path base = fs::weakly_canonical(root, error);
	if(error || base.empty()) throw std::runtime_error("the debugger has no folder");
	const auto it = request.is_object() ? request.find("cwd") : request.end();
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

} // namespace

json launchConfigurations(const fs::path& root) {
	json result = {{"configurations", json::array()}, {"skipped", 0}};
	if(root.empty()) return result;
	const fs::path file = root / ".vscode" / "launch.json";
	std::error_code error;
	if(!fs::is_regular_file(file, error)) return result;
	if(fs::file_size(file, error) > largestLaunchFile || error)
		throw std::runtime_error(".vscode/launch.json is too large");
	std::ifstream in(file, std::ios::binary);
	const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	json launch;
	try {
		launch = affresco::parseUntrusted(plainJson(text));
	} catch(const std::exception&) {
		throw std::runtime_error(".vscode/launch.json isn't JSON");
	}
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

void registerDebug(affresco::Screen& screen, std::shared_ptr<Workspace> workspace, bool grantAdapters) {
	if(const fs::path lldb = findProgram("lldb-dap"); grantAdapters && !lldb.empty())
		screen.services().allowProcess("lldb-dap", lldb);
	if(const fs::path python = findProgram("python3"); grantAdapters && !python.empty())
		screen.services().allowProcess("debugpy", python, {"-m", "debugpy.adapter"});
	// a grant's root is fixed when the screen loads, so the folder open then, like the terminal's
	const fs::path folder = workspace->root.empty() ? fs::current_path() : workspace->root;
	screen.services().addRoot("debug", folder);
	screen.onCall("forge.debug.launchConfigurations",
	              [workspace](const std::string&) { return launchConfigurations(workspace->root).dump(); });
	screen.onCall("forge.debug.commandLine", [folder](const std::string& arguments) {
		const json request = affresco::parseUntrusted(arguments);
		if(!request.is_object()) throw std::runtime_error("the adapter's request is not an object");
		return json{{"line", terminalCommandLine(folder, request)}}.dump();
	});
}

} // namespace forge
