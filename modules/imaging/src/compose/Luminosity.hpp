#pragma once

/// @file
/// @brief The luminosity the blend modes and the adjustments share, so the two can't drift apart. Private to the
/// imaging module.

#include "affresco/imaging/Pixels.hpp"

namespace affresco::imaging::detail {

/// @brief The luminosity of a colour, with the W3C blend modes' weights (the GLSL repeats them).
/// @param pixel The colour.
/// @return Its luminosity, 0 to 1 for a colour in range.
[[nodiscard]] constexpr float luminosityOf(const Pixel& pixel) noexcept {
	return 0.3f * pixel.red + 0.59f * pixel.green + 0.11f * pixel.blue;
}

} // namespace affresco::imaging::detail
