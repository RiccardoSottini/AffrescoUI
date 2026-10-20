#pragma once

/// @file
/// @brief detail::createHeadlessWindow and detail::ManualClock: a Window without a display or OpenGL, on a clock a
/// test moves by hand.
///
/// Internal for now (the unit tests reach it through the private include path). It is meant to become public API once
/// the maintainer agrees on its shape: see the PR of roadmap item 7.3.

#include "Renderer.hpp"

#include "affresco/render/Clock.hpp"
#include "affresco/render/Window.hpp"

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

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

/// @brief Draws a window's current frame offscreen from nothing, at a fixed scale: every panel recorded afresh, no
/// retained geometry or damage used, nothing kept. What Window::saveScreenshot(path, scale) must give: the rendering
/// check compares the two, so a pixel a frame failed to redraw shows.
/// @param window A window with a renderer; its deferred tasks run and it is laid out first.
/// @param scale Pixels per window unit.
/// @return Tightly packed 8-bit RGB, top row first; empty for a headless window.
[[nodiscard]] std::vector<std::uint8_t> drawFromScratch(Window& window, int scale);

/// @brief Does what Window::render() does, but doesn't present the frame: timers, deferred tasks, layout, and drawing
/// into the window's canvas (copied to its back buffer). What a frame costs the library, without the windowing
/// system's own cost of showing it (benchmarks).
/// @param window The window; a headless one lays out instead.
void renderWithoutPresenting(Window& window);

/// @brief What a window's renderer would be given for its next frame, and the damage handed over with it (the window
/// forgets it, as after a frame). For tests of retained geometry, which need no OpenGL.
/// @param window The window, laid out.
/// @return The frame.
[[nodiscard]] Frame takeFrame(Window& window);

} // namespace affresco::detail
