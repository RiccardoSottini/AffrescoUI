/// @file
/// @brief The copilot provider (Copilot.hpp), run by runCommandLine() (CommandLine.hpp).
///
/// The command line has no flag that turns its tools off, so a run lists the only tools the model may see
/// (`--available-tools`) with a name no tool has, which leaves it none, and denies every kind of tool besides. Its
/// prompt is read from stdin when `-p` isn't given. It expands an "@" followed by a path into the path and the size
/// of that file or folder, so copilotInput() breaks those; in prompt mode a "/" command or a "!" shell line is only
/// text (tried with the command line 1.0.114), and the input starts with its instructions' heading anyway.

#include "affresco/intelligence/Copilot.hpp"

#include <affresco/core/Json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace affresco::intelligence {

using affresco::json;

namespace {

/// The one tool the model may see: no tool has this name, so it sees none.
constexpr const char* noTool = "affrescoui-no-tools";

/// The longest error of the command line's kept in an answer.
constexpr std::size_t maxError = 300;

/// @brief A text cut to a length, marked when cut.
/// @param text The text.
/// @param length The most bytes kept.
/// @return It.
std::string cut(const std::string& text, std::size_t length) {
	return text.size() <= length ? text : text.substr(0, length) + "…";
}

/// @brief Whether a text says, in any case, one of some words.
/// @param text The text.
/// @param words The words, in lower case.
/// @return Whether it does.
bool mentions(std::string text, std::initializer_list<std::string_view> words) {
	std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return std::ranges::any_of(words, [&text](std::string_view word) { return text.contains(word); });
}

/// @brief The answer when signing in is what's missing.
/// @return It.
CommandLineAnswer notSignedIn() {
	return {false, "GitHub Copilot isn't signed in: run `copilot login` once in a terminal to sign in."};
}

/// @brief What the command line's JSON lines said.
struct Events {
	std::string content; ///< The last assistant message's content: the answer.
	std::string error;   ///< The session's error.
	bool any = false;    ///< Whether any line was an event.
	int status = 0;      ///< The result's exit code.

	/// @brief Takes one event.
	/// @param event The event, an object with a text "type".
	void take(const json& event) {
		any = true;
		const std::string& type = event["type"].get_ref<const std::string&>();
		const json data = event.contains("data") && event["data"].is_object() ? event["data"] : json::object();
		if(type == "assistant.message" && data.contains("content") && data["content"].is_string())
			content = data["content"].get<std::string>(); // the last one is the answer
		else if(type == "session.error" && data.contains("message") && data["message"].is_string())
			error = data["message"].get<std::string>();
		else if(type == "result" && event.contains("exitCode") && event["exitCode"].is_number_integer())
			status = event["exitCode"].get<int>();
	}
};

/// @brief Reads the command line's JSON lines; a line that isn't a JSON object with a text "type", or that nests too
/// deep, is skipped.
/// @param output What it printed.
/// @return What they said.
Events eventsOf(const std::string& output) {
	Events events;
	for(std::size_t at = 0; at < output.size();) {
		const std::size_t end = std::min(output.find('\n', at), output.size());
		const std::string_view line = std::string_view(output).substr(at, end - at);
		at = end + 1;
		json event;
		try {
			event = affresco::parseUntrusted(line);
		} catch(...) {
			continue;
		}
		if(event.is_object() && event.contains("type") && event["type"].is_string()) events.take(event);
	}
	return events;
}

} // namespace

std::optional<std::filesystem::path> findCopilot() {
	static constexpr std::array places{".local/bin"};
	return findCommandLine("copilot", places);
}

std::vector<std::string> copilotArguments(const std::string& model) {
	return {"--output-format",
	        "json",
	        "--stream",
	        "off",
	        "--model=" + model,
	        std::string("--available-tools=") + noTool, // the model sees no tool: no shell, files, web or MCP
	        "--deny-tool=shell,write,read,url,memory",  // and each kind is denied, should one come back
	        "--disable-builtin-mcps",
	        "--no-custom-instructions", // no AGENTS.md or instructions files
	        "--no-ask-user",
	        "--disallow-temp-dir", // no access to the temporary folder beyond its own empty one
	        "--sync-session=off",  // nothing of the screen goes to the account's history
	        "--no-auto-update",    // it never downloads a newer version itself
	        "--disable-client-telemetry",
	        "--log-level",
	        "none", // no log of the run
	        "--no-color"};
}

std::vector<std::string> copilotEnvironment() {
	// The session bus (DBUS_SESSION_BUS_ADDRESS, XDG_RUNTIME_DIR) is how it reads its sign-in from the keyring on Linux
#ifdef _WIN32
	static constexpr std::array names{
	    "PATH",         "USERPROFILE",        "APPDATA",         "LOCALAPPDATA", "SystemRoot",   "TEMP", "TMP",
	    "COPILOT_HOME", "COPILOT_CACHE_HOME", "COPILOT_GH_HOST", "GH_HOST",      "GH_CONFIG_DIR"};
#else
	static constexpr std::array names{
	    "HOME",
	    "PATH",
	    "USER",
	    "LOGNAME",
	    "TMPDIR",
	    "LANG",
	    "LC_ALL",
	    "LC_CTYPE",
	    "LC_MESSAGES",
	    "LC_COLLATE",
	    "LC_NUMERIC",
	    "LC_TIME",
	    "LC_MONETARY",
	    "COPILOT_HOME",
	    "COPILOT_CACHE_HOME",
	    "XDG_CACHE_HOME",
	    "COPILOT_GH_HOST",
	    "GH_HOST",
	    "GH_CONFIG_DIR",
	    "XDG_RUNTIME_DIR",
	    "DBUS_SESSION_BUS_ADDRESS",
	};
#endif
	std::vector<std::string> entries = keptEnvironment(names);
	entries.emplace_back("COPILOT_AUTO_UPDATE=false");
	entries.emplace_back("COPILOT_DISABLE_LOGIN_SHELL_ENV=1"); // it never runs the person's login shell
	entries.emplace_back("GITHUB_COPILOT_PROMPT_MODE_EXTENSIONS=false");
	entries.emplace_back("GITHUB_COPILOT_PROMPT_MODE_REPO_HOOKS=false");
	entries.emplace_back("GITHUB_COPILOT_PROMPT_MODE_WORKSPACE_MCP=false");
	return entries;
}

std::string copilotInput(const std::string& system, const std::string& input) {
	const std::string text = "# Instructions\n\n" + system + "\n\n# Request\n\n" + input;
	std::string safe;
	safe.reserve(text.size() + text.size() / 64);
	for(std::size_t at = 0; at < text.size(); ++at) {
		safe += text[at];
		if(text[at] != '@' || at + 1 == text.size()) continue;
		const char next = text[at + 1];
		const bool drive =
		    at + 2 < text.size() && std::isalpha(static_cast<unsigned char>(next)) && text[at + 2] == ':';
		if(next == '/' || next == '~' || next == '.' || next == '\\' || drive) safe += "⁠"; // a word joiner
	}
	return safe;
}

CommandLineAnswer parseCopilotOutput(const std::string& output) {
	const Events events = eventsOf(output);
	if(!events.content.empty()) return {true, events.content}; // the agent reads its JSON, in a fence or not
	if(!events.error.empty())
		return mentions(events.error, {"login", "log in", "sign in", "authenticat"})
		           ? notSignedIn()
				   : CommandLineAnswer{false, "GitHub Copilot failed: " + cut(events.error, maxError)};
	if(!events.any) {
		if(mentions(output, {"login", "authenticat"})) return notSignedIn();
		return {false, output.empty() ? "GitHub Copilot gave no answer." : "GitHub Copilot's answer isn't JSON."};
	}
	// Signed out, it says so on stderr only, and its result's exit code is 1
	if(events.status != 0)
		return {false, "GitHub Copilot didn't answer: run `copilot` once in a terminal to check that it is signed in."};
	return {false, "GitHub Copilot gave no answer."};
}

CommandLineAnswer runCopilot(const std::filesystem::path& program, const std::string& model, const std::string& system,
                             const std::string& input, std::chrono::milliseconds timeout,
                             const std::function<bool()>& stopped) {
	const CommandLineRun run{"GitHub Copilot",     "copilot",         program, copilotArguments(model),
	                         copilotEnvironment(), parseCopilotOutput};
	return runCommandLine(run, copilotInput(system, input), timeout, stopped);
}

} // namespace affresco::intelligence
