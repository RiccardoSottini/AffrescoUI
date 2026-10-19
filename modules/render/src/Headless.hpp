#pragma once

/// @file
/// @brief detail::createHeadlessWindow and detail::ManualClock: a Window without a display or OpenGL, on a clock a
/// test moves by hand.
///
/// Internal for now (the unit tests reach it through the private include path). It is meant to become public API once
/// the maintainer agrees on its shape: see the PR of roadmap item 7.3.

#include "affresco/render/Clock.hpp"
#include "affresco/render/Window.hpp"

#include <memory>
#include <string>

namespace affresco::detail {

/// @brief A clock that moves only when told to, so tests decide when timers, tooltips, double clicks and
/// animations are due. Starts at 0.
class ManualClock final : public Clock {
public:
	[[nodiscard]] double seconds() const noexcept override { return seconds_; }

	/// @brief Moves the time forward.
	/// @param milliseconds How far; negative values are ignored, as the clock never goes back.
	void advance(double milliseconds) noexcept {
		if(milliseconds > 0.0) seconds_ += milliseconds / 1000.0;
	}

private:
	double seconds_ = 0.0; ///< See seconds().
};

/// @brief Creates a window with no display and no OpenGL: it routes input, keeps the focus, hover and capture, runs
/// timers and deferred tasks and lays out, but never draws (render() lays out, saveScreenshot() fails). Its clipboard
/// is its own, and it always counts as open. Needs no Application.
/// @param width Width in window units.
/// @param height Height in window units.
/// @param clock The clock its timers read; must outlive the window.
/// @param title The title.
/// @return The window, owned by the caller.
[[nodiscard]] std::unique_ptr<Window> createHeadlessWindow(int width, int height, const Clock& clock,
                                                           std::string title = "AffrescoUI");

} // namespace affresco::detail
