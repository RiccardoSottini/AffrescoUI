#pragma once

/// @file
/// @brief Basic geometry: points, rectangles and alignment.

namespace affresco {

/// @brief A point or size in window units (y pointing down).
struct Vec2 {
	float x = 0.0f; ///< Horizontal.
	float y = 0.0f; ///< Vertical.

	constexpr bool operator==(const Vec2&) const noexcept = default;
};

/// @brief Placement of content inside a larger space, along one axis.
enum class Align {
	Start,  ///< Left or top.
	Center, ///< Centred.
	End,    ///< Right or bottom.
};

/// @brief An axis-aligned rectangle stored as its 4 edges, in window units (origin top-left, y pointing down).
struct Rect {
	float left = 0.0f;   ///< Left edge.
	float top = 0.0f;    ///< Top edge.
	float right = 0.0f;  ///< Right edge.
	float bottom = 0.0f; ///< Bottom edge.

	/// @brief right - left.
	[[nodiscard]] constexpr float width() const noexcept { return right - left; }
	/// @brief bottom - top.
	[[nodiscard]] constexpr float height() const noexcept { return bottom - top; }

	/// @brief Tells whether a point is inside; edges are inclusive, so a click on the border counts.
	/// @param point The point.
	/// @return true if inside or on an edge.
	[[nodiscard]] constexpr bool contains(Vec2 point) const noexcept {
		return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
	}

	constexpr bool operator==(const Rect&) const noexcept = default;
};

} // namespace affresco
