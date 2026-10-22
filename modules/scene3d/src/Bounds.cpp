/// @file
/// @brief Bounding volumes, rays and the view frustum (Bounds.hpp).

#include "affresco/scene3d/Bounds.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace affresco::scene3d {

void Aabb::extend(Vec3 point) noexcept {
	min = minimum(min, point);
	max = maximum(max, point);
}

void Aabb::extend(const Aabb& box) noexcept {
	if(box.empty()) return;
	extend(box.min);
	extend(box.max);
}

bool Aabb::contains(Vec3 point) const noexcept {
	return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y && point.z >= min.z &&
	       point.z <= max.z;
}

Aabb Aabb::transformed(const Mat4& matrix) const noexcept {
	Aabb result;
	if(empty()) return result;
	for(int corner = 0; corner < 8; ++corner)
		result.extend(matrix.transformPoint(
		    {(corner & 1) ? max.x : min.x, (corner & 2) ? max.y : min.y, (corner & 4) ? max.z : min.z}));
	return result;
}

Frustum Frustum::fromMatrix(const Mat4& m) noexcept {
	Frustum frustum;
	// Row 3 plus or minus rows 0 (left, right), 1 (bottom, top) and 2 (near, far)
	for(std::size_t i = 0; i < 6; ++i) {
		const int row = static_cast<int>(i / 2);
		const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
		Vec3 normal{m.at(3, 0) + sign * m.at(row, 0), m.at(3, 1) + sign * m.at(row, 1),
		            m.at(3, 2) + sign * m.at(row, 2)};
		float distance = m.at(3, 3) + sign * m.at(row, 3);
		const float size = length(normal);
		if(size > 0.0f) {
			normal = normal / size;
			distance /= size;
		}
		frustum.planes[i] = {normal, distance};
	}
	return frustum;
}

bool Frustum::intersects(const Aabb& box) const noexcept {
	if(box.empty()) return false;
	for(const Plane& plane : planes) {
		// The corner furthest along the normal
		const Vec3 positive{plane.normal.x >= 0 ? box.max.x : box.min.x, plane.normal.y >= 0 ? box.max.y : box.min.y,
		                    plane.normal.z >= 0 ? box.max.z : box.min.z};
		if(plane.signedDistance(positive) < 0.0f) return false;
	}
	return true;
}

std::optional<float> intersect(const Ray& ray, const Aabb& box) noexcept {
	if(box.empty()) return std::nullopt;
	float nearest = 0.0f, furthest = std::numeric_limits<float>::max();
	for(int axis = 0; axis < 3; ++axis) {
		const float origin = ray.origin[axis], direction = ray.direction[axis];
		if(std::abs(direction) < 1e-12f) {
			if(origin < box.min[axis] || origin > box.max[axis]) return std::nullopt;
			continue;
		}
		float enter = (box.min[axis] - origin) / direction;
		float leave = (box.max[axis] - origin) / direction;
		if(enter > leave) std::swap(enter, leave);
		nearest = std::max(nearest, enter);
		furthest = std::min(furthest, leave);
		if(nearest > furthest) return std::nullopt;
	}
	return nearest;
}

std::optional<float> intersect(const Ray& ray, Vec3 a, Vec3 b, Vec3 c) noexcept {
	const Vec3 edge1 = b - a, edge2 = c - a;
	const Vec3 p = cross(ray.direction, edge2);
	const float determinant = dot(edge1, p);
	if(std::abs(determinant) < 1e-12f) return std::nullopt;
	const float inverse = 1.0f / determinant;
	const Vec3 t = ray.origin - a;
	const float u = dot(t, p) * inverse;
	if(u < 0.0f || u > 1.0f) return std::nullopt;
	const Vec3 q = cross(t, edge1);
	const float v = dot(ray.direction, q) * inverse;
	if(v < 0.0f || u + v > 1.0f) return std::nullopt;
	const float distance = dot(edge2, q) * inverse;
	if(distance < 0.0f) return std::nullopt;
	return distance;
}

std::optional<float> intersect(const Ray& ray, const Plane& plane) noexcept {
	const float facing = dot(plane.normal, ray.direction);
	if(std::abs(facing) < 1e-9f) return std::nullopt;
	const float distance = -plane.signedDistance(ray.origin) / facing;
	if(distance < 0.0f) return std::nullopt;
	return distance;
}

std::optional<float> closestOnLine(const Ray& ray, Vec3 point, Vec3 direction) noexcept {
	// Minimise |ray.at(s) - (point + direction * t)| over s and t
	const Vec3 w = point - ray.origin;
	const float b = dot(ray.direction, direction);
	const float denominator = 1.0f - b * b;
	if(std::abs(denominator) < 1e-6f) return std::nullopt;
	const float d = dot(ray.direction, w), e = dot(direction, w);
	return (b * d - e) / denominator;
}

} // namespace affresco::scene3d
