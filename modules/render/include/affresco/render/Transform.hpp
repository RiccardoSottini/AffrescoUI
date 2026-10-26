#pragma once

/// @file
/// @brief Affine2: a 2D affine transform (rotation, scale, skew, translation), how panels are transformed when painted.

#include "affresco/render/Geometry.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

namespace affresco {

/// @brief A 2D affine transform: x' = a x + c y + tx, y' = b x + d y + ty (y pointing down, as window units).
///
/// The columns (a, b) and (c, d) are where the unit vectors go; (tx, ty) is where the origin goes. Composition reads
/// right to left, as matrices do: (outer * inner) applies inner first.
struct Affine2 {
	float a = 1.0f;  ///< x scale (and rotation).
	float b = 0.0f;  ///< y shear by x (and rotation).
	float c = 0.0f;  ///< x shear by y (and rotation).
	float d = 1.0f;  ///< y scale (and rotation).
	float tx = 0.0f; ///< x translation.
	float ty = 0.0f; ///< y translation.

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const Affine2&) const noexcept = default;

	/// @brief The transform that applies other first, then this.
	/// @param other The inner transform.
	/// @return this * other.
	[[nodiscard]] constexpr Affine2 operator*(const Affine2& other) const noexcept {
		return {a * other.a + c * other.b,         b * other.a + d * other.b,         a * other.c + c * other.d,
		        b * other.c + d * other.d,         a * other.tx + c * other.ty + tx, b * other.tx + d * other.ty + ty};
	}

	/// @brief Where a point goes.
	/// @param point The point.
	/// @return The transformed point.
	[[nodiscard]] constexpr Vec2 apply(Vec2 point) const noexcept {
		return {a * point.x + c * point.y + tx, b * point.x + d * point.y + ty};
	}

	/// @brief Whether it is the identity, exactly.
	/// @return true if it changes nothing.
	[[nodiscard]] constexpr bool isIdentity() const noexcept { return *this == Affine2{}; }

	/// @brief Whether it keeps rectangles axis-aligned rectangles: a scale (maybe negative) and a translation, or a
	/// quarter-turn of them.
	/// @return true if so.
	[[nodiscard]] constexpr bool isAxisAligned() const noexcept {
		return (b == 0.0f && c == 0.0f) || (a == 0.0f && d == 0.0f);
	}

	/// @brief The determinant of its linear part: how it scales areas (negative when it mirrors).
	/// @return a d - b c.
	[[nodiscard]] constexpr float determinant() const noexcept { return a * d - b * c; }

	/// @brief The transform that undoes it, if there is one.
	/// @return The inverse; nullopt when it flattens the plane (a scale of 0), or is not finite.
	[[nodiscard]] std::optional<Affine2> inverse() const noexcept;

	/// @brief The smallest axis-aligned rectangle holding a transformed rectangle.
	/// @param rect The rectangle.
	/// @return The bounding box of its 4 transformed corners.
	[[nodiscard]] constexpr Rect boundingBox(const Rect& rect) const noexcept {
		const Vec2 corners[] = {apply({rect.left, rect.top}), apply({rect.right, rect.top}),
		                        apply({rect.right, rect.bottom}), apply({rect.left, rect.bottom})};
		Rect box{corners[0].x, corners[0].y, corners[0].x, corners[0].y};
		for(const Vec2& corner : corners) {
			box.left = std::min(box.left, corner.x);
			box.top = std::min(box.top, corner.y);
			box.right = std::max(box.right, corner.x);
			box.bottom = std::max(box.bottom, corner.y);
		}
		return box;
	}

	/// @brief A translation.
	/// @param offset By how much.
	/// @return The transform.
	[[nodiscard]] static constexpr Affine2 translation(Vec2 offset) noexcept {
		return {1.0f, 0.0f, 0.0f, 1.0f, offset.x, offset.y};
	}

	/// @brief A scale about the origin.
	/// @param factor Per axis.
	/// @return The transform.
	[[nodiscard]] static constexpr Affine2 scaling(Vec2 factor) noexcept { return {factor.x, 0.0f, 0.0f, factor.y}; }

	/// @brief A rotation about the origin, clockwise on screen (y points down), as CSS rotate().
	/// @param degrees The angle.
	/// @return The transform; exact for multiples of 90 degrees.
	[[nodiscard]] static Affine2 rotation(float degrees) noexcept {
		const float turns = std::fmod(degrees, 360.0f);
		if(turns == 0.0f) return {};
		if(std::abs(turns) == 90.0f || std::abs(turns) == 180.0f || std::abs(turns) == 270.0f) {
			const int quarter = (static_cast<int>(turns) / 90 + 4) % 4; // 1, 2, 3
			constexpr float cosines[] = {1.0f, 0.0f, -1.0f, 0.0f}, sines[] = {0.0f, 1.0f, 0.0f, -1.0f};
			return {cosines[quarter], sines[quarter], -sines[quarter], cosines[quarter]};
		}
		const float radians = degrees * std::numbers::pi_v<float> / 180.0f;
		const float cosine = std::cos(radians), sine = std::sin(radians);
		return {cosine, sine, -sine, cosine};
	}

	/// @brief A skew about the origin, as CSS skew(x, y).
	/// @param degrees The angles: x leans vertical lines, y leans horizontal ones.
	/// @return The transform.
	[[nodiscard]] static Affine2 skewing(Vec2 degrees) noexcept {
		const float toRadians = std::numbers::pi_v<float> / 180.0f;
		return {1.0f, std::tan(degrees.y * toRadians), std::tan(degrees.x * toRadians), 1.0f};
	}

	/// @brief The transform a panel's keys describe, as CSS composes them: translate, rotate, scale, then skew
	/// (the rightmost applied first), about the origin.
	/// @param translate After layout, in window units.
	/// @param degrees Rotation.
	/// @param scale Per axis.
	/// @param skew Per axis, in degrees.
	/// @return The transform.
	[[nodiscard]] static Affine2 compose(Vec2 translate, float degrees, Vec2 scale, Vec2 skew) noexcept {
		return translation(translate) * rotation(degrees) * scaling(scale) * skewing(skew);
	}

	/// @brief The same transform applied about another point (the transform origin) instead of the origin.
	/// @param pivot The point it turns and scales about.
	/// @return T(pivot) * this * T(-pivot).
	[[nodiscard]] constexpr Affine2 about(Vec2 pivot) const noexcept {
		return translation(pivot) * *this * translation({-pivot.x, -pivot.y});
	}
};

} // namespace affresco
