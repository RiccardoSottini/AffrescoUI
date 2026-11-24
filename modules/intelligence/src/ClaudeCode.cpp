/// @file
/// @brief The claude-code provider (ClaudeCode.hpp), run by runCommandLine() (CommandLine.hpp).

#include "affresco/intelligence/ClaudeCode.hpp"

#include <affresco/core/Json.hpp>

#include <array>

namespace affresco::intelligence {

using affresco::json;

std::vector<std::string> claudeEnvironment() {
	static constexpr std::array names{
#ifdef _WIN32
	    "PATH", "USERPROFILE", "APPDATA", "LOCALAPPDATA", "SystemRoot", "TEMP", "TMP", "CLAUDE_CONFIG_DIR",
#else
	    "HOME",   "PATH",     "USER",        "LOGNAME",    "TMPDIR",     "CLAUDE_CONFIG_DIR", "LANG",
	    "LC_ALL", "LC_CTYPE", "LC_MESSAGES", "LC_COLLATE", "LC_NUMERIC", "LC_TIME",           "LC_MONETARY",
#endif
	};
	std::vector<std::string> entries = keptEnvironment(names);
	entries.emplace_back("DISABLE_AUTOUPDATER=1");
	entries.emplace_back("ENABLE_CLAUDEAI_MCP_SERVERS=false");
	return entries;
}

std::optional<std::filesystem::path> findClaude() {
	static constexpr std::array places{".local/bin", ".claude/local"};
	return findCommandLine("claude", places);
}

std::vector<std::string> claudeArguments(const std::string& model, const std::string& system) {
	return {"-p",
	        "--output-format",
	        "json",
	        "--model",
	        model,
	        "--tools",
	        "",                    // no tools at all: it can't run commands, read or write files
	        "--restricted",        // and none come back from settings; their files are ignored
	        "--safe-mode",         // no hooks, plugins, skills, CLAUDE.md, MCP servers of the person's
	        "--strict-mcp-config", // no MCP servers but those given (none)
	        "--disable-slash-commands",
	        "--no-session-persistence", // nothing of the screen is kept
	        "--permission-mode",
	        "dontAsk", // anything that would need a permission is refused
	        "--system-prompt",
	        system};
}

ClaudeAnswer parseClaudeOutput(const std::string& output) {
	const json parsed = json::parse(output, nullptr, false);
	if(!parsed.is_object()) {
		const bool loggedOut = output.find("login") != std::string::npos || output.find("Login") != std::string::npos;
		return {false, loggedOut ? "Claude Code isn't signed in: run `claude` once in a terminal to sign in."
		                         : "Claude Code's answer isn't JSON."};
	}
	const std::string result =
	    parsed.contains("result") && parsed["result"].is_string() ? parsed["result"].get<std::string>() : "";
	const bool failed =
	    parsed.contains("is_error") && parsed["is_error"].is_boolean() && parsed["is_error"].get<bool>();
	if(failed) {
		const bool subtype = parsed.contains("subtype") && parsed["subtype"].is_string();
		std::string said = !result.empty() ? result : subtype ? parsed["subtype"].get<std::string>() : "an error";
		if(said.size() > 300) said = said.substr(0, 300) + "…";
		return {false, "Claude Code failed: " + said};
	}
	return {true, result};
}

ClaudeAnswer runClaude(const std::filesystem::path& program, const std::string& model, const std::string& system,
                       const std::string& input, std::chrono::milliseconds timeout,
                       const std::function<bool()>& stopped) {
	const CommandLineRun run{"Claude Code", "claude", program, claudeArguments(model, system), claudeEnvironment(),
	                         parseClaudeOutput};
	return runCommandLine(run, input, timeout, stopped);
}

} // namespace affresco::intelligence
