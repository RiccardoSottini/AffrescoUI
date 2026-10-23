#pragma once

/// @file
/// @brief appendGradientFill: a fill mesh cut into bands of one colour each, which paint a gradient across a box.

#include "affresco/render/Color.hpp"
#include "affresco/render/Geometry.hpp"
#include "affresco/render/GradientEditor.hpp"
#include "affresco/render/Painter.hpp"

#include <span>
#include <vector>

namespace affresco {

/// @brief A part of a fill in one colour of its gradient.
struct GradientBand {
	Color color;                   ///< Its colour.
	std::vector<MeshVertex> quads; ///< Its quads, 4 vertices each, in the fill's units.
};

/// @brief Cuts a fill mesh (appendFill()) into bands that paint a gradient across a box: a linear one in 128 strips
/// across its direction, a radial one in 96 rings around the box's middle, each band in the gradient's colour at its
/// middle. The pieces keep the fill's coverage, so its edges stay anti-aliased, and they don't overlap, so translucent
/// stops draw evenly. Past 4 * meshQuadLimit quads, the rest of the fill goes uncut, each triangle in the colour at its
/// middle.
/// @param fill The fill's quads, 4 vertices each.
/// @param gradient The gradient (normalised, see normalizeGradient()).
/// @param box The box it spans, in the fill's units.
/// @param bands Where the bands go, one per colour that has quads, in no particular order.
void appendGradientFill(std::span<const MeshVertex> fill, const Gradient& gradient, Rect box,
                        std::vector<GradientBand>& bands);

} // namespace affresco
