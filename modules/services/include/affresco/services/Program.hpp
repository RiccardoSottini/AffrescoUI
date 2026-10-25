#pragma once

/// @file
/// @brief Running a program once from a host: an argument vector (no shell), its own process group (a job object on
/// Windows), its input given whole on stdin, its output read up to a bound, a time limit, and a way to stop it.

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief What to start: the program, its arguments, where and with what environment.
struct Launch {
	std::filesystem::path program;                       ///< An absolute path; never looked up in PATH.
	std::vector<std::string> arguments;                  ///< After the program's name.
	std::filesystem::path directory;                     ///< The working directory, or empty for the host's.
	std::optional<std::vector<std::string>> environment; ///< "NAME=value" entries, or std::nullopt for the host's.
};

/// @brief How a run of runProgram() ended.
struct ProgramRun {
	/// @brief Why it ended.
	enum class End {
		Exited,     ///< By itself: status holds its exit code.
		NotStarted, ///< It couldn't be started.
		TimedOut,   ///< At the time limit, ended with everything it started.
		Stopped,    ///< Asked to stop, ended with everything it started.
		TooLarge    ///< It printed more than the bound, ended with everything it started.
	};
	End end = End::NotStarted; ///< Why it ended.
	int status = -1;           ///< Its exit code (-1 if ended by a signal or never started).
	std::string output;        ///< What it printed on stdout, up to the bound.
};

/// @brief Runs a program once: writes @p input to its stdin and closes it, reads its stdout until it closes it (at
/// most @p maxOutput bytes), and waits for it. A watchdog ends it, and everything it started, at the time limit or
/// when @p stopped says so; a program it started that keeps stdout open never holds the read past that.
/// @param launch What to start.
/// @param input Its whole input (so nothing of it shows in the list of processes).
/// @param timeout The longest it may take.
/// @param stopped Asked every 100 ms; true ends it. May be empty.
/// @param maxOutput The most bytes read from its stdout.
/// @return How it ended, its exit code and what it printed.
[[nodiscard]] ProgramRun runProgram(const Launch& launch, std::string_view input, std::chrono::milliseconds timeout,
                                    const std::function<bool()>& stopped, std::size_t maxOutput);

} // namespace affresco::services
