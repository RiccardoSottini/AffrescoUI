#pragma once

/// @file
/// @brief The unit scale: how many pixels a window unit is, on windowing systems whose own coordinates are pixels
/// (Win32, X11), and the conversions between units and pixels at that scale. Pure arithmetic, so it is tested without
/// a window (tests/unit/unit_scale.cpp); only platform/GlfwPlatform.cpp uses it.
///
/// Where the windowing system already speaks in units (Cocoa, Wayland, the web) or there is no display (headless), the
/// unit scale is exactly 1, and every function here returns its argument unchanged, bit for bit.

#include <algorithm>
#include <cmath>

namespace affresco::platform {

/// @brief The unit scale for a display's content scale: rounded to the nearest quarter and kept between 1 and 4.
/// Quarters, because the engine rasterises text and pictures at quarter scales (1.25, 1.5, 1.75 ...); 1 at least, so
/// nothing is drawn smaller than one pixel per unit; 4 at most, the largest scale displays offer.
/// @param contentScale The display's content scale (1.5 for a display set to 150%); not a number counts as 1.
/// @return Pixels per window unit.
[[nodiscard]] inline float unitScale(float contentScale) noexcept {
	if(std::isnan(contentScale)) return 1.0f;
	return std::clamp(std::round(contentScale * 4.0f) / 4.0f, 1.0f, 4.0f);
}

/// @brief A length in window units as pixels, rounded to the nearest pixel.
/// @param units The length in window units.
/// @param scale The unit scale (unitScale()).
/// @return The length in pixels; units itself at a scale of 1.
[[nodiscard]] inline int unitsToPixels(int units, float scale) noexcept {
	if(scale == 1.0f) return units;
	return static_cast<int>(std::lround(static_cast<double>(units) * static_cast<double>(scale)));
}

/// @brief A length in pixels as window units, rounded to the nearest unit (a window the user dragged to an odd pixel
/// count is the nearest whole number of units, and stays that over any number of round trips).
/// @param pixels The length in pixels.
/// @param scale The unit scale (unitScale()).
/// @return The length in window units; pixels itself at a scale of 1.
[[nodiscard]] inline int pixelsToUnits(int pixels, float scale) noexcept {
	if(scale == 1.0f) return pixels;
	return static_cast<int>(std::lround(static_cast<double>(pixels) / static_cast<double>(scale)));
}

/// @brief A position in pixels (the pointer's, from the windowing system) in window units, not rounded.
/// @param pixels The coordinate in pixels.
/// @param scale The unit scale (unitScale()).
/// @return The coordinate in window units; at a scale of 1, the same float the coordinate always gave.
[[nodiscard]] inline float pointToUnits(double pixels, float scale) noexcept {
	if(scale == 1.0f) return static_cast<float>(pixels);
	return static_cast<float>(pixels / static_cast<double>(scale));
}

} // namespace affresco::platform
