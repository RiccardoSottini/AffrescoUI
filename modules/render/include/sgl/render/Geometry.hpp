#pragma once

namespace sgl {

struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;

	constexpr bool operator==(const Vec2&) const noexcept = default;
};

// Placement of content inside a larger space, along one axis
enum class Align {
	Start,
	Center,
	End,
};

// Axis-aligned rectangle stored as its 4 edges, in window units (origin top-left, y pointing down)
struct Rect {
	float left = 0.0f;
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;

	[[nodiscard]] constexpr float width() const noexcept { return right - left; }
	[[nodiscard]] constexpr float height() const noexcept { return bottom - top; }

	// Edges are inclusive, so a click on the border belongs to the rectangle
	[[nodiscard]] constexpr bool contains(Vec2 point) const noexcept {
		return point.x >= left && point.x <= right && point.y >= top && point.y <= bottom;
	}

	constexpr bool operator==(const Rect&) const noexcept = default;
};

} // namespace sgl
