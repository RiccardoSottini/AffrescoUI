/// @file
/// @brief The outlines of Sinopia's shapes as SVG path data.

#include "ShapeGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace sinopia {

namespace {

/// @brief How far a cubic's control points sit to draw a quarter circle: 4/3 (√2 − 1).
constexpr float circleControl = 0.5522848f;

/// @brief A rectangle, its corners rounded with cubic curves.
/// @param width Its width.
/// @param height Its height.
/// @param radius The corner radius.
/// @return The path data.
[[nodiscard]] std::string rectangle(float width, float height, float radius) {
	const float r = std::clamp(radius, 0.0f, std::min(width, height) / 2.0f);
	if(r <= 0.0f) return std::format("M0 0H{}V{}H0Z", width, height);
	const float c = r * (1.0f - circleControl);
	return std::format("M{0} 0H{1}C{2} 0 {3} {4} {3} {0}V{5}C{3} {6} {2} {7} {1} {7}H{0}C{8} {7} 0 {6} 0 {5}V{0}C0 {8} "
	                   "{8} 0 {0} 0Z",
	                   r, width - r, width - c, width, c, height - r, height - c, height, c);
}

/// @brief An ellipse filling the box, as four cubic curves.
/// @param width Its width.
/// @param height Its height.
/// @return The path data.
[[nodiscard]] std::string ellipse(float width, float height) {
	const float rx = width / 2.0f;
	const float ry = height / 2.0f;
	const float cx = rx * circleControl;
	const float cy = ry * circleControl;
	return std::format("M{0} {1}C{0} {2} {3} 0 {4} 0C{5} 0 {6} {7} {6} {1}C{6} {8} {5} {9} {4} {9}C{3} {9} {0} {8} "
	                   "{0} {1}Z",
	                   0.0f, ry, ry - cy, rx - cx, rx, rx + cx, width, ry - cy, ry + cy, height);
}

/// @brief A regular polygon or a star filling the box, its first point at the top.
/// @param width Its width.
/// @param height Its height.
/// @param points The corners (a star's outer points).
/// @param inner A star's inner radius as a fraction (1: a polygon).
/// @return The path data.
[[nodiscard]] std::string polygon(float width, float height, int points, float inner) {
	const int count = std::clamp(points, 3, 64);
	const bool star = inner < 1.0f;
	const int corners = star ? count * 2 : count;
	std::string data;
	for(int i = 0; i < corners; ++i) {
		const float angle =
		    std::numbers::pi_v<float> * (2.0f * static_cast<float>(i) / static_cast<float>(corners) - 0.5f);
		const float scale = star && i % 2 == 1 ? inner : 1.0f;
		const float x = width / 2.0f * (1.0f + scale * std::cos(angle));
		const float y = height / 2.0f * (1.0f + scale * std::sin(angle));
		data += std::format("{}{} {}", i == 0 ? 'M' : 'L', x, y);
	}
	return data + "Z";
}

/// @brief An arrow pointing right: a shaft and a head, filling the box.
/// @param width Its width.
/// @param height Its height.
/// @return The path data.
[[nodiscard]] std::string arrow(float width, float height) {
	const float head = std::min(width / 2.0f, height);
	const float shaftTop = height * 0.3f;
	const float shaftBottom = height * 0.7f;
	return std::format("M0 {0}H{1}V0L{2} {3}L{1} {4}V{5}H0Z", shaftTop, width - head, width, height / 2.0f, height,
	                   shaftBottom);
}

} // namespace

std::string shapePath(const ShapeOutline& shape) {
	const float width = std::max(shape.width, 1.0f);
	const float height = std::max(shape.height, 1.0f);
	if(shape.kind == "ellipse") return ellipse(width, height);
	if(shape.kind == "polygon") return polygon(width, height, shape.sides, 1.0f);
	if(shape.kind == "star") return polygon(width, height, shape.sides, std::clamp(shape.inner, 0.05f, 0.95f));
	if(shape.kind == "line") return std::format("M0 0L{} {}", width, height);
	if(shape.kind == "arrow") return arrow(width, height);
	return rectangle(width, height, shape.radius);
}

bool isOpenShape(const std::string& kind) noexcept { return kind == "line"; }

} // namespace sinopia
