#pragma once

/// @file
/// @brief A glyph's outline from its font file, as closed contours of cubic segments (Create Work Path).

#include "affresco/imaging/Vector.hpp"

#include <span>
#include <vector>

namespace affresco::imaging::detail {

/// @brief A glyph's contours, each closed, in pixels from its pen position on the baseline (y down).
/// @param file The font file's bytes (a collection's first font).
/// @param glyph The glyph's index in it.
/// @param scale Pixels per font unit.
/// @return The contours; empty for a glyph with no outline (a space) or a file HarfBuzz can't read.
[[nodiscard]] std::vector<std::vector<PathPoint>> glyphContours(std::span<const unsigned char> file, unsigned glyph,
                                                                float scale);

} // namespace affresco::imaging::detail
