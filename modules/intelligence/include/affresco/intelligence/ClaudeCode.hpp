#pragma once

/// @file
/// @brief The claude-code provider: the person's own Claude Code command line, with their own login, run headless
/// for one answer, with no tools at all (no commands, no files, no MCP servers, no hooks or settings of theirs), in
/// an empty temporary folder, from an argument vector (no shell), with a time limit.

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace affresco::intelligence {

/// @brief What a run of the command line gave.
struct ClaudeAnswer {
	bool ok = false;  ///< Whether it answered.
	std::string text; ///< Its answer (the "result"), or why it failed, for people.
};

/// @brief Where the command line is.
/// @return `claude` on the PATH, or nullopt if it isn't installed.
[[nodiscard]] std::optional<std::filesystem::path> findClaude();

/// @brief The arguments a run passes after the program: print mode, JSON output, the model, no tools, no
/// customisations, no MCP servers, nothing kept, and the system prompt.
/// @param model An alias ("sonnet", "opus", "haiku") or a model id.
/// @param system The system prompt.
/// @return The arguments.
[[nodiscard]] std::vector<std::string> claudeArguments(const std::string& model, const std::string& system);

/// @brief The environment a run gets: only what the command line needs from the host's (the home and user, PATH,
/// the temporary folder, the locale, CLAUDE_CONFIG_DIR; on Windows the profile folders and SystemRoot), never the
/// rest (no ANTHROPIC_API_KEY or ANTHROPIC_BASE_URL, no AFFRESCOUI_ variables); with its updater off and no claude.ai
/// MCP servers.
/// @return "NAME=value" entries.
[[nodiscard]] std::vector<std::string> claudeEnvironment();

/// @brief Runs the command line once: @p input goes to its stdin (so the screen's content never shows in the list
/// of processes), its JSON answer is read from its stdout (at most 4 MiB), with claudeEnvironment().
/// @param program The command line (findClaude()).
/// @param model The model.
/// @param system The system prompt.
/// @param input The request, with the screen.
/// @param timeout The longest it may take; then it is ended.
/// @param stopped Asked every 100 ms; true ends it.
/// @return The answer.
[[nodiscard]] ClaudeAnswer runClaude(const std::filesystem::path& program, const std::string& model,
                                     const std::string& system, const std::string& input,
                                     std::chrono::milliseconds timeout, const std::function<bool()>& stopped);

/// @brief Reads the command line's JSON output (`--output-format json`).
/// @param output What it printed.
/// @return Its result, or why it failed.
[[nodiscard]] ClaudeAnswer parseClaudeOutput(const std::string& output);

} // namespace affresco::intelligence
