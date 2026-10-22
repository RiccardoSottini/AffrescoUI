#pragma once

/// @file
/// @brief PseudoTerminal: a program started from an argument vector, with no shell, on a terminal of its own (forkpty,
/// or a pseudo console on Windows), which can be resized and signalled.

#include "process/Process.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace affresco::services {

/// @brief A terminal's size, in cells.
struct TerminalSize {
	std::uint16_t columns = 80; ///< Columns.
	std::uint16_t rows = 24;    ///< Rows.
};

/// @brief What a terminal's signal asks of its program.
enum class TerminalSignal {
	Interrupt, ///< Ctrl+C: SIGINT to the foreground process group (on Windows, a Ctrl+C written to it).
	Terminate, ///< SIGTERM to the foreground process group (on Windows, the program and everything it started end).
	Hangup,    ///< SIGHUP to the whole session, as closing a terminal sends (on Windows, as Terminate).
	Kill,      ///< SIGKILL to the whole session (on Windows, as Terminate).
};

/// @brief A running program on a terminal. read() blocks, so it belongs on a thread of its own; end() may be called
/// from any thread and makes a blocked read return. write() blocks while the program doesn't read.
class PseudoTerminal {
public:
	/// @brief Starts a program on a new terminal. TERM is set to xterm-256color unless the environment sets it.
	/// @param launch What to start.
	/// @param size The terminal's size.
	/// @return The terminal.
	/// @throws std::runtime_error If it can't be started (the message names the program, never its environment).
	[[nodiscard]] static std::unique_ptr<PseudoTerminal> start(const Launch& launch, TerminalSize size);
	/// @brief Ends the program (end()) and closes the terminal.
	~PseudoTerminal();

	PseudoTerminal(const PseudoTerminal&) = delete;
	PseudoTerminal& operator=(const PseudoTerminal&) = delete;
	PseudoTerminal(PseudoTerminal&&) = delete;
	PseudoTerminal& operator=(PseudoTerminal&&) = delete;

	/// @brief Writes bytes to the terminal, as typed, all of them.
	/// @param bytes The bytes.
	/// @return false if the terminal closed.
	[[nodiscard]] bool write(std::string_view bytes);
	/// @brief Reads what the program wrote to the terminal.
	/// @param buffer Where the bytes go.
	/// @param size Its size.
	/// @return The count read; 0 once the terminal closed (or end() was called).
	[[nodiscard]] std::size_t read(char* buffer, std::size_t size);
	/// @brief Resizes the terminal; the program hears SIGWINCH.
	/// @param size The new size.
	void resize(TerminalSize size) noexcept;
	/// @brief Signals the program.
	/// @param signal What to send.
	void signal(TerminalSignal signal) noexcept;
	/// @brief Ends the program and everything on its terminal (a hangup, then a kill), and waits for it. Once only.
	/// @return Its exit code, or -1 if a signal ended it.
	int end() noexcept;
	/// @brief Waits for the program to exit by itself (after read() returned 0).
	/// @return Its exit code, or -1 as for end().
	int wait() noexcept;

private:
	struct Platform;
	/// @brief Takes the platform's handles.
	/// @param platform The handles.
	explicit PseudoTerminal(std::unique_ptr<Platform> platform) noexcept;

	std::unique_ptr<Platform> platform_; ///< The pid or process handle, and the terminal's ends.
};

} // namespace affresco::services
