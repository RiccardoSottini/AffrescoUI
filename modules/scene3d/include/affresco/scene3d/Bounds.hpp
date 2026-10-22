#pragma once

/// @file
/// @brief Bounding volumes, rays and the view frustum: what culling and picking test against.

#include "affresco/scene3d/Math.hpp"

#include <array>
#include <limits>
#include <optional>

namespace affresco::scene3d {

/// @brief An axis-aligned box; empty (min above max) until a point is added.
struct Aabb {
	Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
	Vec3 max{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
	         -std::numeric_limits<float>::max()}; ///< The largest corner.

	/// @brief Whether nothing was added. @return true if empty.
	[[nodiscard]] constexpr bool empty() const noexcept { return min.x > max.x || min.y > max.y || min.z > max.z; }
	/// @brief Grows to hold a point. @param point The point.
	void extend(Vec3 point) noexcept;
	/// @brief Grows to hold another box. @param box The box; an empty one changes nothing.
	void extend(const Aabb& box) noexcept;
	/// @brief The middle. @return It.
	[[nodiscard]] constexpr Vec3 center() const noexcept { return (min + max) * 0.5f; }
	/// @brief Half the size per axis. @return It.
	[[nodiscard]] constexpr Vec3 halfExtent() const noexcept { return (max - min) * 0.5f; }
	/// @brief Whether a point is inside or on the surface. @param point It. @return true if so.
	[[nodiscard]] bool contains(Vec3 point) const noexcept;
	/// @brief The box around this one's eight corners, transformed. @param matrix The transform. @return It.
	[[nodiscard]] Aabb transformed(const Mat4& matrix) const noexcept;
};

/// @brief A plane: the points p where dot(normal, p) + distance is 0; positive on the side the normal points to.
struct Plane {
	Vec3 normal;           ///< Of length 1.
	float distance = 0.0f; ///< The signed offset.

	/// @brief The signed distance of a point. @param point It. @return Positive in front.
	[[nodiscard]] constexpr float signedDistance(Vec3 point) const noexcept { return dot(normal, point) + distance; }
};

/// @brief The space a camera sees: six planes facing inwards.
struct Frustum {
	std::array<Plane, 6> planes{}; ///< Left, right, bottom, top, near, far.

	/// @brief The frustum of a projection times a view (Gribb and Hartmann's method).
	/// @param viewProjection The matrix.
	/// @return The frustum, in the space the matrix takes from.
	[[nodiscard]] static Frustum fromMatrix(const Mat4& viewProjection) noexcept;
	/// @brief Whether a box may be visible: false only when it is wholly outside one plane.
	/// @param box The box, in the frustum's space.
	/// @return false if it surely isn't.
	[[nodiscard]] bool intersects(const Aabb& box) const noexcept;
};

/// @brief A half-line, for picking.
struct Ray {
	Vec3 origin;    ///< Where it starts.
	Vec3 direction; ///< Where it goes, of length 1.

	/// @brief A point on it. @param distance How far along. @return The point.
	[[nodiscard]] constexpr Vec3 at(float distance) const noexcept { return origin + direction * distance; }
};

/// @brief Where a ray enters a box (slab test).
/// @param ray The ray.
/// @param box The box.
/// @return The distance along the ray (0 if it starts inside), or nothing if it misses.
[[nodiscard]] std::optional<float> intersect(const Ray& ray, const Aabb& box) noexcept;

/// @brief Where a ray meets a triangle, either side (Möller and Trumbore).
/// @param ray The ray.
/// @param a A corner. @param b Another. @param c The last.
/// @return The distance along the ray, or nothing if it misses.
[[nodiscard]] std::optional<float> intersect(const Ray& ray, Vec3 a, Vec3 b, Vec3 c) noexcept;

/// @brief Where a ray meets a plane.
/// @param ray The ray.
/// @param plane The plane.
/// @return The distance along the ray (at least 0), or nothing if parallel or behind.
[[nodiscard]] std::optional<float> intersect(const Ray& ray, const Plane& plane) noexcept;

/// @brief The closest points of a ray and a line, e.g. to drag along an axis.
/// @param ray The ray.
/// @param point A point on the line.
/// @param direction The line's direction, of length 1.
/// @return How far along the line its closest point to the ray is, or nothing if they are parallel.
[[nodiscard]] std::optional<float> closestOnLine(const Ray& ray, Vec3 point, Vec3 direction) noexcept;

} // namespace affresco::scene3d
