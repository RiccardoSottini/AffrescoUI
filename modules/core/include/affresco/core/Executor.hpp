#pragma once

/// @file
/// @brief The Executor interface: run work on the UI thread.

#include <functional>
#include <vector>

namespace affresco {

/// @brief Runs tasks on the UI (main) thread.
///
/// Implemented by the rendering engine's Application; used by modules that work on other threads, e.g. a channel
/// reading a socket.
class Executor {
public:
	virtual ~Executor() = default;

	/// @brief Queues a task for the main thread. Safe to call from any thread.
	/// @param task The work to run.
	virtual void post(std::function<void()> task) = 0;
};

/// @brief Runs a batch of tasks swapped out of a queue, and empties it, keeping its capacity. If one throws, the batch
/// is emptied all the same: the tasks run so far never run again, and the rest are dropped, as with a local batch.
/// @param tasks The batch.
inline void runTasks(std::vector<std::function<void()>>& tasks) {
	// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions): a scope guard, never copied or moved
	struct Emptied {
		std::vector<std::function<void()>>& tasks; ///< The batch.
		/// @brief Empties it, on return or on a throw.
		~Emptied() { tasks.clear(); }
	} emptied{tasks};
	for(auto& task : tasks) task();
}

} // namespace affresco
