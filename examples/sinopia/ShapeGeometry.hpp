#pragma once

/// @file
/// @brief The outlines of Sinopia's shapes as SVG path data, at a size: rectangles with rounded corners, ellipses,
/// polygons, stars, lines and arrows.

#include <string>

namespace sinopia {

/// @brief What a shape is: its kind and the values that change its outline.
struct ShapeOutline {
	std::string kind = "rect"; ///< rect, ellipse, polygon, star, line or arrow (another kind: a rectangle).
	float width = 100.0f;      ///< Its width.
	float height = 100.0f;     ///< Its height.
	float radius = 0.0f;       ///< A rectangle's corner radius (at most half its shorter side).
	int sides = 5;             ///< A polygon's sides (3 to 64), a star's points.
	float inner = 0.5f;        ///< A star's inner radius, as a fraction of the outer.
};

/// @brief A shape's outline as SVG path data, from (0, 0) to (width, height). A line is open (it has only a stroke);
/// the others are closed.
/// @param shape The shape.
/// @return The path data.
[[nodiscard]] std::string shapePath(const ShapeOutline& shape);

/// @brief Whether a kind is drawn as an open line (stroked, never filled).
/// @param kind The kind.
/// @return Whether.
[[nodiscard]] bool isOpenShape(const std::string& kind) noexcept;

} // namespace sinopia
