#pragma once

/// @file
/// @brief The Executor interface: run work on the UI thread.

#include <functional>

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

} // namespace affresco
