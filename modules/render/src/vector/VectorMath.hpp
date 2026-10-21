#pragma once

/// @file
/// @brief Small vector arithmetic on Vec2 for the vector path code (Vec2 itself stays a plain point).

#include "affresco/render/Geometry.hpp"
#include "affresco/render/Painter.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace affresco::vector {

/// @brief The sum of two vectors.
[[nodiscard]] constexpr Vec2 plus(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
/// @brief The difference of two vectors.
[[nodiscard]] constexpr Vec2 minus(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
/// @brief A vector scaled.
[[nodiscard]] constexpr Vec2 times(Vec2 a, float factor) noexcept { return {a.x * factor, a.y * factor}; }
/// @brief The dot product.
[[nodiscard]] constexpr float dot(Vec2 a, Vec2 b) noexcept { return a.x * b.x + a.y * b.y; }
/// @brief The z of the cross product: positive when @p b turns from @p a toward its left (leftOf).
[[nodiscard]] constexpr float cross(Vec2 a, Vec2 b) noexcept { return a.x * b.y - a.y * b.x; }
/// @brief The vector turned a quarter toward its left side.
[[nodiscard]] constexpr Vec2 leftOf(Vec2 a) noexcept { return {-a.y, a.x}; }
/// @brief The length of a vector.
[[nodiscard]] inline float length(Vec2 a) noexcept { return std::sqrt(dot(a, a)); }

/// @brief A vector of length 1 in the same direction, or zero for a vector too short to have one.
[[nodiscard]] inline Vec2 normalized(Vec2 a) noexcept {
	const float size = length(a);
	return size > 1.0e-12f ? times(a, 1.0f / size) : Vec2{};
}

/// @brief Tells whether a mesh holds as many quads as it may.
/// @param mesh The mesh.
/// @param quadLimit The most quads.
[[nodiscard]] inline bool meshIsFull(const std::vector<MeshVertex>& mesh, std::size_t quadLimit) noexcept {
	return mesh.size() / 4 >= quadLimit;
}

/// @brief Adds a quad to a mesh.
/// @param mesh The mesh.
/// @param corners Its 4 corners, in order around it.
inline void pushQuad(std::vector<MeshVertex>& mesh, const std::array<MeshVertex, 4>& corners) {
	mesh.insert(mesh.end(), corners.begin(), corners.end());
}

} // namespace affresco::vector
