#pragma once

/// @file
/// @brief What the command-line providers share (claude-code, copilot): the person's own command line, found on the
/// PATH, run once headless with no tools, in an empty temporary folder of its own, from an argument vector (no
/// shell), with only the environment it needs, its input on stdin, its output bounded and a time limit.

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::intelligence {

/// @brief What a run of a command line gave.
struct CommandLineAnswer {
	bool ok = false;  ///< Whether it answered.
	std::string text; ///< Its answer, or why it failed, for people.
};

/// @brief Runs a command-line provider once: (model, system prompt, input, stopped) to its answer.
using CommandLineRunner =
    std::function<CommandLineAnswer(const std::string& model, const std::string& system, const std::string& input,
	                                const std::function<bool()>& stopped)>;

/// @brief One run of a command line.
struct CommandLineRun {
	std::string name;                     ///< For people, e.g. "Claude Code".
	std::string tag;                      ///< A short word for its temporary folder's name, e.g. "claude".
	std::filesystem::path program;        ///< The command line (found with findCommandLine()).
	std::vector<std::string> arguments;   ///< After the program.
	std::vector<std::string> environment; ///< "NAME=value" entries: its whole environment.
	/// Reads what it printed on stdout into its answer.
	std::function<CommandLineAnswer(const std::string& output)> parse;
};

/// @brief The most a command line may print.
inline constexpr std::size_t maxCommandLineOutput = std::size_t{4} << 20;

/// @brief Finds a command line: on the PATH, else in the places installers put it (an app opened from the desktop
/// often has a short PATH).
/// @param name Its name, e.g. "claude" (".exe" is added on Windows).
/// @param homePlaces Folders under the home where it may be, e.g. ".local/bin".
/// @return Where it is, or nullopt if it isn't installed.
[[nodiscard]] std::optional<std::filesystem::path> findCommandLine(std::string_view name,
                                                                   std::span<const char* const> homePlaces);

/// @brief The host's values of some environment variables, the rest left out.
/// @param names The variables to keep (those unset or empty are left out).
/// @return "NAME=value" entries.
[[nodiscard]] std::vector<std::string> keptEnvironment(std::span<const char* const> names);

/// @brief Runs a command line once (services::runProgram): in an empty temporary folder of its own (this user's
/// only, removed after), @p input on its stdin (so it never shows in the list of processes), its stdout read up to
/// maxCommandLineOutput and given to `run.parse`. An answer from a run that exited with another status than 0 is a
/// failure.
/// @param run What to run.
/// @param input Its input.
/// @param timeout The longest it may take; then it is ended, with everything it started.
/// @param stopped Asked every 100 ms; true ends it. May be empty.
/// @return The answer.
[[nodiscard]] CommandLineAnswer runCommandLine(const CommandLineRun& run, const std::string& input,
                                               std::chrono::milliseconds timeout, const std::function<bool()>& stopped);

} // namespace affresco::intelligence
