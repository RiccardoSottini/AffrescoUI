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
/// The command runs through /bin/sh; its stderr is inherited, so it can log. POSIX only for now.
class ProcessChannel final : public Channel {
public:
	/// @brief Starts the child process and a thread reading its output.
	/// @param executor Where messages are delivered (the main thread).
	/// @param command A shell command.
	/// @throws std::runtime_error If the process can't be started.
	ProcessChannel(Executor& executor, const std::string& command);
	/// @brief Closes the child's stdin and waits for it to exit.
	~ProcessChannel() override;

	ProcessChannel(const ProcessChannel&) = delete;
	ProcessChannel& operator=(const ProcessChannel&) = delete;

	void send(std::string_view message) override;
	/// @brief Whether the child's output is still open.
	[[nodiscard]] bool isRunning() const noexcept { return running_; }

private:
	Executor& executor_;                ///< Delivers messages on the main thread.
	int pid_ = -1;                      ///< The child process.
	int toChild_ = -1;                  ///< Its stdin.
	int fromChild_ = -1;                ///< Its stdout.
	std::atomic<bool> running_ = false; ///< See isRunning().
	std::thread reader_;                ///< Reads its stdout.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

/// @brief A Channel over this process's own stdin/stdout: the app is the other end's child process.
///
/// This is how MCP clients run servers. Nothing else may write to stdout while it is in use. POSIX only for now.
class StdioChannel final : public Channel {
public:
	/// @brief Starts a thread reading stdin.
	/// @param executor Where messages are delivered (the main thread).
	explicit StdioChannel(Executor& executor);
	~StdioChannel() override;

	StdioChannel(const StdioChannel&) = delete;
	StdioChannel& operator=(const StdioChannel&) = delete;

	void send(std::string_view message) override;

private:
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

} // namespace affresco::channel
