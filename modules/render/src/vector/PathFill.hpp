#pragma once

/// @file
/// @brief vector::appendFill: the public fill within a share of the work, for drawings of many paths (SVG icons) that
/// bound their work as a whole.

#include "affresco/render/VectorPath.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace affresco::vector {

/// @brief appendFill() within a share of the work: each of its passes, the inside and the fringe, ends past
/// @p workLimit edge visits (appendFill() gives each meshWorkLimit).
/// @param contours The contours, in window units; each is closed.
/// @param rule Which points are inside.
/// @param mesh Where the quads go, 4 vertices each.
/// @param feather How wide the fringe is; 0 for none.
/// @param workLimit The most edge visits of each pass.
/// @return The edge visits made, both passes together.
[[nodiscard]] std::size_t appendFill(std::span<const VectorPath::Contour> contours, FillRule rule,
                                     std::vector<MeshVertex>& mesh, float feather, std::size_t workLimit);

} // namespace affresco::vector
