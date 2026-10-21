#pragma once

/// @file
/// @brief The CallProgress interface: what a slow native function sees of its call while it runs on a worker thread.

#include "affresco/core/Json.hpp"

namespace affresco {

/// @brief A native call in progress, as the function running it on a worker thread sees it.
///
/// Every function is safe to call from that thread. What it reports reaches the screen through the event loop: the
/// latest progress and the latest partial result are shown at the next frame, earlier ones in between are dropped.
class CallProgress {
public:
	virtual ~CallProgress() = default;

	/// @brief Whether the call was cancelled (a "cancel" step, its "timeout", the screen going away): the function
	/// should stop soon, as what it returns is dropped.
	/// @return true once cancelled.
	[[nodiscard]] virtual bool cancelled() const noexcept = 0;
	/// @brief Reports how far the call is, into the step's "progress".
	/// @param fraction From 0 to 1.
	virtual void progress(double fraction) = 0;
	/// @brief Streams a partial result into the step's "into", until the final one replaces it.
	/// @param result The result so far.
	virtual void partial(json result) = 0;
};

} // namespace affresco
