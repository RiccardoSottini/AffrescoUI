#pragma once

/// @file
/// @brief Clock: the time a window's timers, tooltips, double clicks and a screen's animations all read.

#include <chrono>

namespace affresco {

/// @brief Where a window reads the time: one clock for its timers, its tooltip delay, double clicks and the
/// animations and transitions of a screen built into it, so they all agree, and so a test can control time.
///
/// An Application gives its windows a SteadyClock. Implementations are read on the main thread only.
class Clock {
public:
	virtual ~Clock() = default;

	/// @brief Seconds since an arbitrary start. Never goes back.
	[[nodiscard]] virtual double seconds() const noexcept = 0;
};

/// @brief The system's steady clock: real time, unaffected by changes to the wall clock.
class SteadyClock final : public Clock {
public:
	[[nodiscard]] double seconds() const noexcept override {
		return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
};

} // namespace affresco
