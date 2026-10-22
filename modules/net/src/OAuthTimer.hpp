#pragma once

/// @file
/// @brief OAuthTimer: runs tasks on the main thread after a delay (a device sign-in's polls, a token's refresh before
/// it expires), from one thread of its own.

#include "affresco/core/Executor.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace affresco::net {

/// @brief Tasks due later, posted to an Executor when due. Destroying it cancels every task not yet run.
class OAuthTimer {
public:
	/// @brief A task's handle, for cancel().
	using Id = std::uint64_t;

	/// @brief Starts the thread.
	/// @param executor Runs the tasks on the main thread; outlives this object.
	explicit OAuthTimer(Executor& executor);
	/// @brief Cancels every task and joins the thread.
	~OAuthTimer();
	OAuthTimer(const OAuthTimer&) = delete;
	OAuthTimer& operator=(const OAuthTimer&) = delete;
	OAuthTimer(OAuthTimer&&) = delete;
	OAuthTimer& operator=(OAuthTimer&&) = delete;

	/// @brief Runs a task on the main thread after a delay.
	/// @param delay The delay.
	/// @param task The task.
	/// @return Its handle.
	Id after(std::chrono::milliseconds delay, std::function<void()> task);
	/// @brief Cancels a task (on the main thread): it doesn't run, even if it is already posted.
	/// @param id Its handle; an unknown one does nothing.
	void cancel(Id id);

private:
	/// @brief A task waiting.
	struct Waiting {
		std::chrono::steady_clock::time_point due; ///< When it runs.
		std::function<void()> task;                ///< What runs.
	};

	/// @brief The thread: posts each task when due, until stopped.
	/// @param stop Set when the timer goes away.
	void run(const std::stop_token& stop);

	Executor& executor_;                                         ///< See the constructor.
	std::mutex mutex_;                                           ///< Guards waiting_ and next_.
	std::condition_variable_any changed_;                        ///< Signalled when a task is added.
	std::map<Id, Waiting> waiting_;                              ///< By handle.
	Id next_ = 1;                                                ///< The next handle.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Posted tasks do nothing once it is reset.
	std::jthread thread_;                                        ///< Last: it starts once the rest is ready.
};

} // namespace affresco::net
