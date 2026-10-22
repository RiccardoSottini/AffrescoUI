/// @file
/// @brief The orbit camera (Camera.hpp).

#include "affresco/scene3d/Camera.hpp"
#include "affresco/scene3d/SceneJson.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace affresco::scene3d {

namespace {

constexpr float halfPi = 1.5707963267948966f; ///< A quarter turn.
constexpr float pi = 3.141592653589793f;      ///< Half a turn.

/// @brief The views by name.
constexpr std::array<std::pair<StandardView, const char*>, 6> viewNames{{{StandardView::Front, "front"},
                                                                         {StandardView::Back, "back"},
                                                                         {StandardView::Left, "left"},
                                                                         {StandardView::Right, "right"},
                                                                         {StandardView::Top, "top"},
                                                                         {StandardView::Bottom, "bottom"}}};

/// @brief A number member, if present. @param value The object. @param key Its name. @param fallback If absent.
/// @return The value. @throws DocumentError If present but not a finite number.
[[nodiscard]] float numberMember(const json& value, const char* key, float fallback) {
	const auto found = value.find(key);
	if(found == value.end()) return fallback;
	if(!found->is_number() || !std::isfinite(found->get<double>()))
		throw DocumentError(std::string("camera.") + key + ": expected a number");
	return found->get<float>();
}

} // namespace

const char* standardViewName(StandardView view) noexcept {
	for(const auto& [each, name] : viewNames)
		if(each == view) return name;
	return "front";
}

std::optional<StandardView> standardViewNamed(std::string_view name) noexcept {
	for(const auto& [each, eachName] : viewNames)
		if(name == eachName) return each;
	return std::nullopt;
}

Vec3 OrbitCamera::eye() const noexcept {
	const float c = std::cos(pitch);
	return target + Vec3{c * std::sin(yaw), std::sin(pitch), c * std::cos(yaw)} * distance;
}

Mat4 OrbitCamera::view() const noexcept {
	// The up vector is the derivative of the eye's direction along the pitch, so it stays exact looking straight down.
	const float s = std::sin(pitch);
	const Vec3 up{-s * std::sin(yaw), std::cos(pitch), -s * std::cos(yaw)};
	return Mat4::lookAt(eye(), target, up);
}

float OrbitCamera::halfHeightAtTarget() const noexcept { return distance * std::tan(lens.fieldOfView * 0.5f); }

Mat4 OrbitCamera::projection(float aspect) const noexcept {
	const float safeAspect = aspect > 0.0f && std::isfinite(aspect) ? aspect : 1.0f;
	if(lens.orthographic) {
		const float h = halfHeightAtTarget();
		return Mat4::orthographic(h * safeAspect, h, -lens.farPlane, lens.farPlane);
	}
	return Mat4::perspective(lens.fieldOfView, safeAspect, lens.nearPlane, lens.farPlane);
}

Mat4 OrbitCamera::viewProjection(float aspect) const noexcept { return projection(aspect) * view(); }

void OrbitCamera::orbit(float dx, float dy, float radiansPerPixel) noexcept {
	yaw = std::remainder(yaw - dx * radiansPerPixel, 2.0f * pi);
	pitch = std::clamp(pitch + dy * radiansPerPixel, -halfPi, halfPi);
}

void OrbitCamera::pan(float dx, float dy, float viewportHeight) noexcept {
	if(viewportHeight <= 0.0f) return;
	const float unitsPerPixel = 2.0f * halfHeightAtTarget() / viewportHeight;
	const float s = std::sin(pitch);
	const Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};
	const Vec3 up{-s * std::sin(yaw), std::cos(pitch), -s * std::cos(yaw)};
	target = target - right * (dx * unitsPerPixel) + up * (dy * unitsPerPixel);
}

void OrbitCamera::zoom(float steps) noexcept {
	distance = std::clamp(distance * std::pow(0.9f, steps), 0.01f, 100000.0f);
}

void OrbitCamera::frame(const Aabb& box) noexcept {
	if(box.empty()) return;
	const float radius = std::max(length(box.halfExtent()), 0.01f);
	target = box.center();
	distance = std::clamp(radius * 1.1f / std::sin(lens.fieldOfView * 0.5f), 0.01f, 100000.0f);
}

void OrbitCamera::snap(StandardView view) noexcept {
	switch(view) {
	case StandardView::Front:
		yaw = 0.0f, pitch = 0.0f;
		break;
	case StandardView::Back:
		yaw = pi, pitch = 0.0f;
		break;
	case StandardView::Left:
		yaw = -halfPi, pitch = 0.0f;
		break;
	case StandardView::Right:
		yaw = halfPi, pitch = 0.0f;
		break;
	case StandardView::Top:
		yaw = 0.0f, pitch = halfPi;
		break;
	case StandardView::Bottom:
		yaw = 0.0f, pitch = -halfPi;
		break;
	}
}

Ray OrbitCamera::rayThrough(float x, float y, float width, float height) const noexcept {
	const auto inverse = viewProjection(width / height).inverse().value_or(Mat4{});
	const float ndcX = 2.0f * x / width - 1.0f;
	const float ndcY = 1.0f - 2.0f * y / height;
	const auto unproject = [&](float z) {
		const Vec4 p = inverse.transform({ndcX, ndcY, z, 1.0f});
		return Vec3{p.x, p.y, p.z} / p.w;
	};
	const Vec3 nearPoint = unproject(-1.0f);
	return {nearPoint, normalize(unproject(1.0f) - nearPoint)};
}

std::optional<std::pair<float, float>> OrbitCamera::project(Vec3 point, float width, float height) const noexcept {
	const Vec4 clip = viewProjection(width / height).transform({point.x, point.y, point.z, 1.0f});
	if(clip.w <= 1e-6f) return std::nullopt;
	return std::pair{(clip.x / clip.w * 0.5f + 0.5f) * width, (0.5f - clip.y / clip.w * 0.5f) * height};
}

json toJson(const OrbitCamera& camera) {
	return {{"target", toJson(camera.target)},
	        {"yaw", camera.yaw},
	        {"pitch", camera.pitch},
	        {"distance", camera.distance},
	        {"orthographic", camera.lens.orthographic},
	        {"fieldOfView", camera.lens.fieldOfView}};
}

OrbitCamera cameraFromJson(const json& value) {
	if(!value.is_object()) throw DocumentError("camera: expected an object");
	OrbitCamera camera;
	if(value.contains("target")) camera.target = vec3FromJson(value["target"], "camera.target");
	camera.yaw = std::remainder(numberMember(value, "yaw", camera.yaw), 2.0f * pi);
	camera.pitch = std::clamp(numberMember(value, "pitch", camera.pitch), -halfPi, halfPi);
	camera.distance = std::clamp(numberMember(value, "distance", camera.distance), 0.01f, 100000.0f);
	camera.lens.fieldOfView = std::clamp(numberMember(value, "fieldOfView", camera.lens.fieldOfView), 0.05f, 3.0f);
	if(const auto ortho = value.find("orthographic"); ortho != value.end()) {
		if(!ortho->is_boolean()) throw DocumentError("camera.orthographic: expected true or false");
		camera.lens.orthographic = ortho->get<bool>();
	}
	return camera;
}

} // namespace affresco::scene3d
