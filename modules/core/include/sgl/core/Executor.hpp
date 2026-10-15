#pragma once

#include <functional>

namespace sgl {

// Runs tasks on the UI (main) thread. Implemented by the rendering engine's Application; used by modules that do
// work on other threads, e.g. a channel reading a pipe.
class Executor {
public:
	virtual ~Executor() = default;

	// Safe to call from any thread; `task` runs later, on the main thread
	virtual void post(std::function<void()> task) = 0;
};

} // namespace sgl
