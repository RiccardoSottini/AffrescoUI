#pragma once

/// @file
/// @brief Snapping on a canvas: a moved or resized rectangle pulled to the grid, to other items' edges and centres,
/// and to guides, with the smart guides to draw. Pure functions, in world units.

#include <affresco/render/Geometry.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace affresco {

/// @brief What a rectangle snaps to.
struct SnapTargets {
	float grid = 0.0f;                   ///< The grid's spacing; 0 snaps to no grid.
	std::span<const float> vertical{};   ///< Vertical guides: their x.
	std::span<const float> horizontal{}; ///< Horizontal guides: their y.
	std::span<const Rect> others{};      ///< Other items' rectangles: their edges and centres.
};

/// @brief The edges a resize moves; a move moves them all.
struct SnapEdges {
	bool left = true;   ///< The left edge moves.
	bool top = true;    ///< The top edge moves.
	bool right = true;  ///< The right edge moves.
	bool bottom = true; ///< The bottom edge moves.
};

/// @brief A smart guide to draw: a line where the rectangle met something.
struct SnapLine {
	bool vertical = true;  ///< A vertical line (at an x) or a horizontal one (at a y).
	float position = 0.0f; ///< Its x (vertical) or y (horizontal).
	float from = 0.0f;     ///< Where it starts along the other axis.
	float to = 0.0f;       ///< Where it ends along the other axis.
};

/// @brief A snapped rectangle and the lines to draw, one per axis at most.
struct SnapResult {
	Rect rect{};                                    ///< The rectangle, moved or resized to what it snapped to.
	std::array<std::optional<SnapLine>, 2> lines{}; ///< The vertical line, then the horizontal one; none for the grid.
};

/// @brief Snaps a rectangle that moves whole: each axis moves by the smallest pull within the tolerance that brings
/// one of its edges or its centre onto a target.
/// @param moving The rectangle where the pointer put it.
/// @param targets What it snaps to.
/// @param tolerance World units (screen pixels over the zoom); 0 or less snaps to nothing.
/// @return The rectangle and the guides.
[[nodiscard]] SnapResult snapMove(Rect moving, const SnapTargets& targets, float tolerance) noexcept;

/// @brief Snaps the edges a resize moves, each to the nearest target within the tolerance.
/// @param resized The rectangle where the pointer put it.
/// @param edges The edges that move.
/// @param targets What they snap to.
/// @param tolerance World units; 0 or less snaps to nothing.
/// @return The rectangle and the guides.
[[nodiscard]] SnapResult snapResize(Rect resized, SnapEdges edges, const SnapTargets& targets,
                                    float tolerance) noexcept;

} // namespace affresco
