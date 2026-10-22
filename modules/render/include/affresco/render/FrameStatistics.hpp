#pragma once

/// @file
/// @brief FrameStatistics: what a window's last frame cost, for an inspector.

#include <cstddef>
#include <cstdint>

namespace affresco {

/// @brief What a window's last frame cost: read with Window::frameStatistics().
struct FrameStatistics {
	std::uint64_t frames = 0;     ///< Frames laid out (and drawn, unless headless) since the window opened.
	double frameMilliseconds = 0; ///< How long the last one took to lay out, draw and hand to the display.
	std::size_t drawCalls = 0;    ///< How many draw calls it made (0 for a headless window).
};

} // namespace affresco
