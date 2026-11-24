#pragma once

/// @file
/// @brief The copilot provider: the person's own GitHub Copilot command line (`copilot`, npm `@github/copilot`),
/// with their own GitHub sign-in and subscription, run headless for one answer with no tools at all (no shell, no
/// files, no web, none of any MCP server's tools; no built-in or workspace MCP servers, no custom instructions), in an
/// empty temporary folder, from an argument vector (no shell), with a time limit. Its input goes to stdin, never on the
/// command line. The person's own user-level MCP servers, plugins and hooks (in `~/.copilot`) may still start: no flag
/// turns them off, though the model is given none of their tools.

#include "affresco/intelligence/CommandLine.hpp"

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace affresco::intelligence {

/// @brief Where the command line is.
/// @return `copilot` on the PATH (or where its installers put it), or nullopt if it isn't installed.
[[nodiscard]] std::optional<std::filesystem::path> findCopilot();

/// @brief The arguments a run passes after the program: JSON lines out, the model, no tools (an allowlist naming
/// none of them, and every kind of tool denied besides), no built-in MCP servers, no custom instructions, no
/// questions, no temporary folder, nothing synced to the account, no update, no telemetry, no logs.
/// @param model A model id, e.g. "claude-sonnet-5.5" or "auto".
/// @return The arguments.
[[nodiscard]] std::vector<std::string> copilotArguments(const std::string& model);

/// @brief The environment a run gets: only what the command line needs from the host's (the home and user, PATH,
/// the temporary folder, the locale, its own home and cache, the GitHub host, the session bus its keyring is read
/// through; on Windows the profile folders and SystemRoot), never the rest (no GitHub tokens, so the person's own
/// sign-in is used; no COPILOT_ALLOW_ALL, provider, model, instructions or skills variables; no AFFRESCOUI_
/// variables); with its updater, its login shell and the repository's (workspace) extensions, hooks and MCP servers
/// off. The person's own user-level ones in `$COPILOT_HOME` are theirs: no variable turns them off.
/// @return "NAME=value" entries.
[[nodiscard]] std::vector<std::string> copilotEnvironment();

/// @brief What the command line reads on stdin: the system prompt, then the input, with every "@" that would name a
/// file or folder (`@/`, `@~`, `@.`, `@\`, `@C:`) broken by a word joiner (U+2060), so the screen's content can't
/// make it list a file of the person's.
/// @param system The system prompt.
/// @param input The request, with the screen.
/// @return The text.
[[nodiscard]] std::string copilotInput(const std::string& system, const std::string& input);

/// @brief Runs the command line once (runCommandLine()): copilotInput() goes to its stdin (so the screen's content
/// never shows in the list of processes), its JSON lines are read from its stdout (at most 4 MiB), with
/// copilotEnvironment().
/// @param program The command line (findCopilot()).
/// @param model The model.
/// @param system The system prompt.
/// @param input The request, with the screen.
/// @param timeout The longest it may take; then it is ended.
/// @param stopped Asked every 100 ms; true ends it.
/// @return The answer.
[[nodiscard]] CommandLineAnswer runCopilot(const std::filesystem::path& program, const std::string& model,
                                           const std::string& system, const std::string& input,
                                           std::chrono::milliseconds timeout, const std::function<bool()>& stopped);

/// @brief Reads the command line's output (`--output-format json`: one JSON object a line): the last assistant
/// message's content as it is (the agent finds the JSON object in it, in a code fence or not), or the session's
/// error. A line that isn't JSON, or that nests too deep, is skipped.
/// @param output What it printed.
/// @return Its answer, or why it failed.
[[nodiscard]] CommandLineAnswer parseCopilotOutput(const std::string& output);

} // namespace affresco::intelligence
