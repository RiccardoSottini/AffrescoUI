/// @file
/// @brief vector::Canvas: triangles with interpolated coverage, blended into premultiplied pixels.

#include "MeshRasterizer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace affresco::vector {

namespace {

/// @brief Twice the signed area of the triangle a, b, p: positive when p is left of a→b (in pixel axes, y down).
[[nodiscard]] float edgeAt(Vec2 a, Vec2 b, Vec2 p) noexcept { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); }

/// @brief Whether a pixel exactly on an edge belongs to the triangle: only on its top and left edges, so a pixel two
/// triangles share is drawn once.
/// @param a The edge's start.
/// @param b The edge's end, the triangle turning counter-clockwise (in pixel axes).
[[nodiscard]] bool ownsEdge(Vec2 a, Vec2 b) noexcept { return a.y > b.y || (a.y == b.y && a.x < b.x); }

/// @brief Converts a channel in [0, 1] to a byte.
[[nodiscard]] std::uint8_t toByte(float value) noexcept {
	return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

} // namespace

Canvas::Canvas(int width, int height, std::size_t workLimit)
    : width_(std::max(width, 1)), height_(std::max(height, 1)), workLeft_(workLimit),
      pixels_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4, 0) {}

void Canvas::fill(std::span<const MeshVertex> quads, Color color) {
	const std::array<float, 4> premultiplied{color.r * color.a, color.g * color.a, color.b * color.a, color.a};
	if(!(color.a > 0.0f)) return;
	for(std::size_t first = 0; first + 4 <= quads.size(); first += 4) {
		triangle({quads[first], quads[first + 1], quads[first + 2]}, premultiplied);
		triangle({quads[first + 2], quads[first + 3], quads[first]}, premultiplied);
	}
}

// Pixel centres inside by edge functions; the coverage is interpolated with the same barycentric weights
void Canvas::triangle(const std::array<MeshVertex, 3>& corners, const std::array<float, 4>& color) {
	std::array<Vec2, 3> p{corners[0].position, corners[1].position, corners[2].position};
	std::array<float, 3> coverage{corners[0].coverage, corners[1].coverage, corners[2].coverage};
	float area = edgeAt(p[0], p[1], p[2]);
	if(!(std::abs(area) > 1.0e-6f)) return; // also drops NaN
	if(area < 0.0f) { // one winding, so the edge rule holds
		std::swap(p[1], p[2]);
		std::swap(coverage[1], coverage[2]);
		area = -area;
	}
	const float left = std::max(std::floor(std::min({p[0].x, p[1].x, p[2].x})), 0.0f);
	const float top = std::max(std::floor(std::min({p[0].y, p[1].y, p[2].y})), 0.0f);
	const float right = std::min(std::ceil(std::max({p[0].x, p[1].x, p[2].x})), static_cast<float>(width_));
	const float bottom = std::min(std::ceil(std::max({p[0].y, p[1].y, p[2].y})), static_cast<float>(height_));
	if(!(right > left) || !(bottom > top)) return;
	const auto visits = static_cast<std::size_t>((right - left) * (bottom - top));
	if(visits > workLeft_) {
		workLeft_ = 0;
		return;
	}
	workLeft_ -= visits;
	const std::array<bool, 3> owns{ownsEdge(p[1], p[2]), ownsEdge(p[2], p[0]), ownsEdge(p[0], p[1])};
	for(auto y = static_cast<int>(top); y < static_cast<int>(bottom); ++y)
		for(auto x = static_cast<int>(left); x < static_cast<int>(right); ++x) {
			const Vec2 centre{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
			const std::array<float, 3> weight{edgeAt(p[1], p[2], centre), edgeAt(p[2], p[0], centre),
			                                  edgeAt(p[0], p[1], centre)};
			bool inside = true;
			for(std::size_t edge = 0; edge < 3; ++edge)
				inside = inside && (weight[edge] > 0.0f || (weight[edge] == 0.0f && owns[edge]));
			if(!inside) continue;
			const float amount = (weight[0] * coverage[0] + weight[1] * coverage[1] + weight[2] * coverage[2]) / area;
			blend((static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)) * 4,
			      color, amount);
		}
}

void Canvas::blend(std::size_t index, const std::array<float, 4>& color, float coverage) noexcept {
	const float amount = std::clamp(coverage, 0.0f, 1.0f);
	const float keep = 1.0f - color[3] * amount;
	for(std::size_t channel = 0; channel < 4; ++channel) {
		const float below = static_cast<float>(pixels_[index + channel]) / 255.0f;
		pixels_[index + channel] = toByte(color[channel] * amount + below * keep);
	}
}

std::vector<std::uint8_t> Canvas::straightPixels() const {
	std::vector<std::uint8_t> straight(pixels_.size());
	for(std::size_t index = 0; index < pixels_.size(); index += 4) {
		const std::uint8_t alpha = pixels_[index + 3];
		for(std::size_t channel = 0; channel < 3; ++channel)
			straight[index + channel] =
			    alpha == 0 ? 0 : static_cast<std::uint8_t>(std::min(pixels_[index + channel] * 255 / alpha, 255));
		straight[index + 3] = alpha;
	}
	return straight;
}

} // namespace affresco::vector
