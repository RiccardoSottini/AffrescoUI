#pragma once

/// @file
/// @brief The general providers' factories, and what they share: a bounded set of worker threads, paths kept under a
/// root, and a UTF-8 check for text that goes into the document.

#include "affresco/services/Provider.hpp"

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace affresco::services {

/// @brief The `fs` provider: a granted folder; read, write and list text files under it.
[[nodiscard]] std::unique_ptr<Provider> makeFileProvider();
/// @brief The `process` provider: a granted program, run with an input; its output lines are streamed.
[[nodiscard]] std::unique_ptr<Provider> makeProcessProvider();
/// @brief The `jsonrpc` provider: a granted program spoken to in JSON-RPC 2.0 over its stdin and stdout.
[[nodiscard]] std::unique_ptr<Provider> makeJsonRpcProvider();
/// @brief The `watch` provider: a granted folder; what changes under it is streamed, debounced.
[[nodiscard]] std::unique_ptr<Provider> makeWatchProvider();
/// @brief The `pty` provider: a granted program on a terminal of its own; its output is streamed, what is typed sent.
[[nodiscard]] std::unique_ptr<Provider> makePtyProvider();

/// @brief Takes the text from terminal output that arrives in pieces (a split sequence waits for the next piece;
/// bytes that aren't UTF-8 become U+FFFD).
/// @param carry Bytes left from the piece before; on return, the incomplete sequence at this one's end.
/// @param bytes The piece.
/// @return The text, valid UTF-8.
[[nodiscard]] std::string takeTerminalText(std::string& carry, std::string_view bytes);

/// @brief Worker threads, at most a few at once, all stopped and joined when it ends. Main thread only.
class Tasks {
public:
	/// @brief Starts an empty set.
	/// @param limit The most that may run at once.
	explicit Tasks(std::size_t limit) noexcept;
	/// @brief Asks every task to stop and waits for them.
	~Tasks();
	Tasks(const Tasks&) = delete;
	Tasks& operator=(const Tasks&) = delete;
	Tasks(Tasks&&) = delete;
	Tasks& operator=(Tasks&&) = delete;

	/// @brief Runs work on a thread of its own.
	/// @param work The work; its stop token says the set is ending.
	/// @return false if @p limit tasks are running already (the work then doesn't run).
	[[nodiscard]] bool run(std::function<void(std::stop_token)> work);

private:
	/// @brief One running task.
	struct Task {
		std::shared_ptr<std::atomic<bool>> done; ///< Set when its work returns.
		std::jthread thread;                     ///< Its thread.
	};
	std::size_t limit_;       ///< See the constructor.
	std::vector<Task> tasks_; ///< Those started, finished ones pruned at each run().
};

/// @brief Resolves a path a screen gave against a root, following links, and keeps it under the root.
/// @param root The root (canonical).
/// @param relative The path, relative to the root ("" or "." for the root itself).
/// @return The resolved path, or std::nullopt if it is absolute or leads outside the root.
[[nodiscard]] std::optional<std::filesystem::path> underRoot(const std::filesystem::path& root,
                                                             std::string_view relative);

/// @brief Whether bytes are valid UTF-8 (no overlong forms, no surrogates), so they may become a JSON string.
/// @param bytes The bytes.
/// @return Whether they are.
[[nodiscard]] bool isUtf8(std::string_view bytes) noexcept;

/// @brief Ends a call when either of two stop tokens asks: the call's own (cancelled, timed out) or its worker's
/// (the service is ending), running an action (e.g. ending a child process) once.
class EitherStop {
public:
	/// @brief Watches both tokens.
	/// @param first One.
	/// @param second The other.
	/// @param action What to run when either asks, on the thread that asked; at most once.
	EitherStop(const std::stop_token& first, const std::stop_token& second, std::function<void()> action);
	/// @brief Whether either asked.
	[[nodiscard]] bool requested() const noexcept;

private:
	/// @brief Runs the action, once.
	void fire() noexcept;

	std::function<void()> action_;                                                 ///< See the constructor.
	std::atomic<bool> fired_{false};                                               ///< Whether it ran.
	std::optional<std::stop_callback<std::function<void()>>> first_, second_;     ///< The watches.
};

} // namespace affresco::services
