/// @file
/// @brief Gizmo maths (Gizmo.hpp).

#include "affresco/scene3d/Gizmo.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace affresco::scene3d {

namespace {

/// @brief The modes by name.
constexpr std::array<std::pair<GizmoMode, const char*>, 3> modeNames{
    {{GizmoMode::Translate, "translate"}, {GizmoMode::Rotate, "rotate"}, {GizmoMode::Scale, "scale"}}};

/// @brief The axes by name.
constexpr std::array<std::pair<GizmoAxis, const char*>, 8> axisNames{{{GizmoAxis::None, "none"},
                                                                      {GizmoAxis::X, "x"},
                                                                      {GizmoAxis::Y, "y"},
                                                                      {GizmoAxis::Z, "z"},
                                                                      {GizmoAxis::XY, "xy"},
                                                                      {GizmoAxis::YZ, "yz"},
                                                                      {GizmoAxis::ZX, "zx"},
                                                                      {GizmoAxis::View, "view"}}};

/// @brief An axis's index. @param axis X, Y or Z. @return 0, 1 or 2, or -1 for the others.
[[nodiscard]] int axisIndex(GizmoAxis axis) noexcept {
	return axis == GizmoAxis::X ? 0 : axis == GizmoAxis::Y ? 1 : axis == GizmoAxis::Z ? 2 : -1;
}

/// @brief A plane's two axes. @param axis XY, YZ or ZX. @return Their indices, or {-1, -1}.
[[nodiscard]] std::pair<int, int> planeAxes(GizmoAxis axis) noexcept {
	if(axis == GizmoAxis::XY) return {0, 1};
	if(axis == GizmoAxis::YZ) return {1, 2};
	if(axis == GizmoAxis::ZX) return {2, 0};
	return {-1, -1};
}

/// @brief Rounds to a step. @param value It. @param step 0 for none. @return The value.
[[nodiscard]] float snapped(float value, float step) noexcept {
	return step > 0.0f ? std::round(value / step) * step : value;
}

/// @brief Where a ray crosses the plane through the gizmo's origin. @param origin The point. @param normal The
/// plane's normal. @param ray The ray. @return The point, or nothing.
[[nodiscard]] std::optional<Vec3> onPlane(Vec3 origin, Vec3 normal, const Ray& ray) noexcept {
	const Vec3 n = normalize(normal);
	const auto t = intersect(ray, Plane{n, -dot(n, origin)});
	if(!t) return std::nullopt;
	return ray.at(*t);
}

/// @brief How far along a ray it passes near an axis line, if near enough. @param gizmo The gizmo. @param index The
/// axis. @param ray The ray. @param tolerance How near. @return The distance along the ray, or nothing.
[[nodiscard]] std::optional<float> nearAxis(const Gizmo& gizmo, int index, const Ray& ray, float tolerance) noexcept {
	const Vec3 a = gizmo.axis(index);
	const auto t = closestOnLine(ray, gizmo.origin, a);
	if(!t || *t < 0.0f || *t > gizmo.size * 1.05f) return std::nullopt;
	const Vec3 p = gizmo.origin + a * *t;
	const float s = dot(p - ray.origin, ray.direction);
	if(s < 0.0f || length(ray.at(s) - p) > tolerance) return std::nullopt;
	return s;
}

/// @brief How far along a ray it crosses a plane handle's square, if it does. @param gizmo The gizmo.
/// @param axis The plane. @param ray The ray. @return The distance along the ray, or nothing.
[[nodiscard]] std::optional<float> onPlaneHandle(const Gizmo& gizmo, GizmoAxis axis, const Ray& ray) noexcept {
	const auto [i, j] = planeAxes(axis);
	const auto hit = onPlane(gizmo.origin, cross(gizmo.axis(i), gizmo.axis(j)), ray);
	if(!hit) return std::nullopt;
	const float u = dot(*hit - gizmo.origin, gizmo.axis(i)) / gizmo.size;
	const float v = dot(*hit - gizmo.origin, gizmo.axis(j)) / gizmo.size;
	if(u < 0.2f || u > 0.45f || v < 0.2f || v > 0.45f) return std::nullopt;
	return length(*hit - ray.origin);
}

/// @brief How far along a ray it crosses a rotation ring, if it does. @param gizmo The gizmo. @param index The axis.
/// @param ray The ray. @param tolerance How near. @return The distance along the ray, or nothing.
[[nodiscard]] std::optional<float> onRing(const Gizmo& gizmo, int index, const Ray& ray, float tolerance) noexcept {
	const auto hit = onPlane(gizmo.origin, gizmo.axis(index), ray);
	if(!hit || std::abs(length(*hit - gizmo.origin) - gizmo.size) > tolerance) return std::nullopt;
	return length(*hit - ray.origin);
}

/// @brief The handles worth testing for a mode. @param mode It. @return Them.
[[nodiscard]] std::array<GizmoAxis, 7> handlesOf(GizmoMode mode) noexcept {
	if(mode == GizmoMode::Rotate)
		return {GizmoAxis::X,    GizmoAxis::Y,    GizmoAxis::Z,   GizmoAxis::None,
		        GizmoAxis::None, GizmoAxis::None, GizmoAxis::None};
	return {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z, GizmoAxis::XY, GizmoAxis::YZ, GizmoAxis::ZX, GizmoAxis::View};
}

/// @brief How far along a ray it meets one handle. @param gizmo The gizmo. @param handle It. @param ray The ray.
/// @return The distance, or nothing if it misses.
[[nodiscard]] std::optional<float> handleDistance(const Gizmo& gizmo, GizmoAxis handle, const Ray& ray) noexcept {
	const float tolerance = gizmo.size * 0.1f;
	if(handle == GizmoAxis::None) return std::nullopt;
	if(handle == GizmoAxis::View) {
		const float s = dot(gizmo.origin - ray.origin, ray.direction);
		if(s < 0.0f || length(ray.at(s) - gizmo.origin) > gizmo.size * 0.12f) return std::nullopt;
		return s;
	}
	if(const int index = axisIndex(handle); index >= 0)
		return gizmo.mode == GizmoMode::Rotate ? onRing(gizmo, index, ray, tolerance)
		                                       : nearAxis(gizmo, index, ray, tolerance);
	return onPlaneHandle(gizmo, handle, ray);
}

/// @brief A translation drag. @param gizmo The gizmo. @param axis The constraint. @param start The press's ray.
/// @param current The ray now. @param step The snap. @return The offset, or nothing.
[[nodiscard]] std::optional<Vec3> translation(const Gizmo& gizmo, GizmoAxis axis, const Ray& start, const Ray& current,
                                              float step) noexcept {
	if(const int index = axisIndex(axis); index >= 0) {
		const Vec3 a = gizmo.axis(index);
		const auto t0 = closestOnLine(start, gizmo.origin, a), t1 = closestOnLine(current, gizmo.origin, a);
		if(!t0 || !t1) return std::nullopt;
		return a * snapped(*t1 - *t0, step);
	}
	const auto [i, j] = planeAxes(axis);
	const Vec3 normal = i >= 0 ? cross(gizmo.axis(i), gizmo.axis(j)) : start.direction;
	const auto h0 = onPlane(gizmo.origin, normal, start), h1 = onPlane(gizmo.origin, normal, current);
	if(!h0 || !h1) return std::nullopt;
	const Vec3 d = *h1 - *h0;
	Vec3 result;
	for(int k = 0; k < 3; ++k) {
		const Vec3 a = i >= 0 ? gizmo.axis(k) : Vec3{k == 0 ? 1.0f : 0.0f, k == 1 ? 1.0f : 0.0f, k == 2 ? 1.0f : 0.0f};
		if(i < 0 || k == i || k == j) result = result + a * snapped(dot(d, a), step);
	}
	return result;
}

/// @brief A rotation drag. @param gizmo The gizmo. @param axis X, Y, Z or View. @param start The press's ray.
/// @param current The ray now. @param step The snap. @return The rotation, or nothing.
[[nodiscard]] std::optional<Quat> rotation(const Gizmo& gizmo, GizmoAxis axis, const Ray& start, const Ray& current,
                                           float step) noexcept {
	const int index = axisIndex(axis);
	const Vec3 a = index >= 0 ? gizmo.axis(index) : normalize(-start.direction);
	const auto h0 = onPlane(gizmo.origin, a, start), h1 = onPlane(gizmo.origin, a, current);
	if(!h0 || !h1) return std::nullopt;
	const Vec3 v0 = *h0 - gizmo.origin, v1 = *h1 - gizmo.origin;
	const float angle = std::atan2(dot(cross(v0, v1), a), dot(v0, v1));
	return Quat::fromAxisAngle(a, snapped(angle, step));
}

/// @brief A scale drag. @param gizmo The gizmo. @param axis The constraint. @param start The press's ray.
/// @param current The ray now. @param step The snap. @return The factors, or nothing.
[[nodiscard]] std::optional<Vec3> scaling(const Gizmo& gizmo, GizmoAxis axis, const Ray& start, const Ray& current,
                                          float step) noexcept {
	float factor = 1.0f;
	const int index = axisIndex(axis);
	if(index >= 0) {
		const Vec3 a = gizmo.axis(index);
		const auto t0 = closestOnLine(start, gizmo.origin, a), t1 = closestOnLine(current, gizmo.origin, a);
		if(!t0 || !t1 || std::abs(*t0) < 1e-6f) return std::nullopt;
		factor = *t1 / *t0;
	} else {
		const auto [i, j] = planeAxes(axis);
		const Vec3 normal = i >= 0 ? cross(gizmo.axis(i), gizmo.axis(j)) : start.direction;
		const auto h0 = onPlane(gizmo.origin, normal, start), h1 = onPlane(gizmo.origin, normal, current);
		if(!h0 || !h1 || length(*h0 - gizmo.origin) < 1e-6f) return std::nullopt;
		factor = length(*h1 - gizmo.origin) / length(*h0 - gizmo.origin);
	}
	factor = snapped(factor, step);
	if(std::abs(factor) < 1e-3f) factor = std::copysign(1e-3f, factor);
	return numericDelta(gizmo, axis, factor, start.direction).scale;
}

} // namespace

const char* gizmoModeName(GizmoMode mode) noexcept {
	for(const auto& [each, name] : modeNames)
		if(each == mode) return name;
	return "translate";
}

std::optional<GizmoMode> gizmoModeNamed(std::string_view name) noexcept {
	for(const auto& [each, eachName] : modeNames)
		if(name == eachName) return each;
	return std::nullopt;
}

const char* gizmoAxisName(GizmoAxis axis) noexcept {
	for(const auto& [each, name] : axisNames)
		if(each == axis) return name;
	return "none";
}

std::optional<GizmoAxis> gizmoAxisNamed(std::string_view name) noexcept {
	for(const auto& [each, eachName] : axisNames)
		if(name == eachName) return each;
	return std::nullopt;
}

Vec3 Gizmo::axis(int index) const noexcept {
	return orientation.rotate({index == 0 ? 1.0f : 0.0f, index == 1 ? 1.0f : 0.0f, index == 2 ? 1.0f : 0.0f});
}

float worldSizeForPixels(const OrbitCamera& camera, Vec3 point, float viewportHeight, float pixels) noexcept {
	if(viewportHeight <= 0.0f) return 1.0f;
	float halfHeight = camera.halfHeightAtTarget();
	if(!camera.lens.orthographic) {
		const Vec3 forward = normalize(camera.target - camera.eye());
		const float depth = std::max(dot(point - camera.eye(), forward), camera.lens.nearPlane);
		halfHeight = depth * std::tan(camera.lens.fieldOfView * 0.5f);
	}
	return pixels * 2.0f * halfHeight / viewportHeight;
}

GizmoAxis hitHandle(const Gizmo& gizmo, const Ray& ray) noexcept {
	if(gizmo.mode != GizmoMode::Rotate && handleDistance(gizmo, GizmoAxis::View, ray)) return GizmoAxis::View;
	GizmoAxis best = GizmoAxis::None;
	float bestDistance = 0.0f;
	for(const GizmoAxis handle : handlesOf(gizmo.mode)) {
		const auto distance = handleDistance(gizmo, handle, ray);
		if(distance && (best == GizmoAxis::None || *distance < bestDistance)) {
			best = handle;
			bestDistance = *distance;
		}
	}
	return best;
}

std::optional<GizmoDelta> dragGizmo(const Gizmo& gizmo, GizmoAxis axis, const Ray& start, const Ray& current,
                                    const GizmoSnap& snap) noexcept {
	if(axis == GizmoAxis::None) return std::nullopt;
	GizmoDelta delta;
	if(gizmo.mode == GizmoMode::Translate) {
		const auto offset = translation(gizmo, axis, start, current, snap.translate);
		if(!offset) return std::nullopt;
		delta.translation = *offset;
	} else if(gizmo.mode == GizmoMode::Rotate) {
		const auto turn = rotation(gizmo, axis, start, current, snap.rotate);
		if(!turn) return std::nullopt;
		delta.rotation = *turn;
	} else {
		const auto factors = scaling(gizmo, axis, start, current, snap.scale);
		if(!factors) return std::nullopt;
		delta.scale = *factors;
	}
	return delta;
}

GizmoDelta numericDelta(const Gizmo& gizmo, GizmoAxis axis, float value, Vec3 viewDirection) noexcept {
	GizmoDelta delta;
	const int index = axisIndex(axis);
	const auto [i, j] = planeAxes(axis);
	if(gizmo.mode == GizmoMode::Translate) {
		if(index >= 0) delta.translation = gizmo.axis(index) * value;
	} else if(gizmo.mode == GizmoMode::Rotate) {
		const Vec3 a = index >= 0 ? gizmo.axis(index) : normalize(-viewDirection);
		if(index >= 0 || axis == GizmoAxis::View) delta.rotation = Quat::fromAxisAngle(a, radians(value));
	} else {
		for(int k = 0; k < 3; ++k)
			if(k == index || k == i || k == j || axis == GizmoAxis::View) delta.scale[k] = value;
	}
	return delta;
}

Transform applyDelta(const Transform& local, const Mat4& parentWorld, const Gizmo& gizmo,
                     const GizmoDelta& delta) noexcept {
	const Mat4 world = parentWorld * local.matrix();
	const Mat4 axes = Mat4::rotation(gizmo.orientation);
	const Mat4 change = Mat4::translation(delta.translation + gizmo.origin) * Mat4::rotation(delta.rotation) * axes *
	                    Mat4::scaling(delta.scale) * axes.transposed() * Mat4::translation(-gizmo.origin);
	const Mat4 parentInverse = parentWorld.inverse().value_or(Mat4{});
	if(auto result = Transform::decompose(parentInverse * change * world)) return *result;
	// A sheared result (scaling along axes the node isn't aligned with) keeps the node's own scale axes.
	Transform fallback = local;
	fallback.scale = multiply(local.scale, delta.scale);
	return fallback;
}

} // namespace affresco::scene3d
