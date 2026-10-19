#pragma once

/// @file
/// @brief Channels to a child process and over this process's stdin/stdout.

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace affresco::channel {

/// @brief A Channel to a child process: messages go to its stdin, its stdout lines come back.
///
/// The command runs through the system shell: `/bin/sh -c` on macOS and Linux, `cmd.exe /d /s /c` on Windows. Its
/// stderr is inherited, so it can log. The child and everything it starts form one group (a process group on POSIX,
/// a job object on Windows), so closing the channel ends all of them. Owned by the main thread; a thread of its own
/// reads the child's output and hands each line to the Executor.
class ProcessChannel final : public Channel {
public:
	/// @brief Starts the child process and a thread reading its output.
	/// @param executor Where messages are delivered (the main thread). Must outlive the channel.
	/// @param command A shell command.
	/// @throws std::runtime_error If the process can't be started.
	ProcessChannel(Executor& executor, const std::string& command);
	/// @brief Closes the child's stdin, ends the child and everything it started, and waits for them.
	~ProcessChannel() override;

	ProcessChannel(const ProcessChannel&) = delete;
	ProcessChannel& operator=(const ProcessChannel&) = delete;

	/// @brief Writes one message and a line break to the child's stdin; does nothing once the child has gone.
	/// @param message The message.
	void send(std::string_view message) override;
	/// @brief Whether the child's output is still open.
	[[nodiscard]] bool isRunning() const noexcept { return running_; }

private:
	struct Process; ///< The child and the parent's ends of its pipes; defined for each platform in Channels.cpp.

	Executor& executor_;                                         ///< Delivers messages on the main thread.
	std::unique_ptr<Process> process_;                           ///< The child process.
	std::atomic<bool> running_ = false;                          ///< See isRunning().
	std::thread reader_;                                         ///< Reads its stdout.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

/// @brief A Channel over this process's own stdin/stdout: the app is the other end's child process.
///
/// This is how MCP clients run servers. Nothing else may write to stdout while it is in use. A detached thread reads
/// stdin for the rest of the process's life, because a blocked read of stdin can't be interrupted portably.
class StdioChannel final : public Channel {
public:
	/// @brief Starts a thread reading stdin.
	/// @param executor Where messages are delivered (the main thread). Must outlive the process's use of stdin.
	explicit StdioChannel(Executor& executor);
	/// @brief Stops delivering messages; the reading thread ends with the process.
	~StdioChannel() override;

	StdioChannel(const StdioChannel&) = delete;
	StdioChannel& operator=(const StdioChannel&) = delete;

	/// @brief Writes one message and a line break to stdout.
	/// @param message The message.
	void send(std::string_view message) override;

private:
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

} // namespace affresco::channel
