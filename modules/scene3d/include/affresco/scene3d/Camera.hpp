#pragma once

/// @file
/// @brief The orbit camera of a 3D viewport: it turns around a target, pans, zooms, frames a box and snaps to the
/// front, side and top views, in perspective or orthographic. It is state, so it reads and writes JSON.

#include "affresco/core/Json.hpp"
#include "affresco/scene3d/Bounds.hpp"
#include "affresco/scene3d/Scene.hpp"

#include <optional>
#include <string_view>
#include <utility>

namespace affresco::scene3d {

/// @brief The views a camera snaps to.
enum class StandardView { Front, Back, Left, Right, Top, Bottom };

/// @brief A view's name ("front", "back", "left", "right", "top", "bottom"). @param view It. @return The name.
[[nodiscard]] const char* standardViewName(StandardView view) noexcept;
/// @brief The view of a name. @param name It. @return The view, or nothing if unknown.
[[nodiscard]] std::optional<StandardView> standardViewNamed(std::string_view name) noexcept;

/// @brief A camera turning around a target point, y up.
struct OrbitCamera {
	Vec3 target;           ///< What it looks at and turns around.
	float yaw = 0.6f;      ///< Around y from +z, radians.
	float pitch = 0.45f;   ///< Above the horizon, radians, within (-pi/2, pi/2).
	float distance = 8.0f; ///< From the target.
	Lens lens;             ///< Perspective or orthographic; an orthographic height follows the distance.

	/// @brief Where the eye is. @return The point.
	[[nodiscard]] Vec3 eye() const noexcept;
	/// @brief World to camera. @return The matrix.
	[[nodiscard]] Mat4 view() const noexcept;
	/// @brief Camera to clip space. @param aspect Width over height. @return The matrix.
	[[nodiscard]] Mat4 projection(float aspect) const noexcept;
	/// @brief World to clip space. @param aspect Width over height. @return projection(aspect) * view().
	[[nodiscard]] Mat4 viewProjection(float aspect) const noexcept;
	/// @brief Half the visible height at the target (what orthographic shows). @return It.
	[[nodiscard]] float halfHeightAtTarget() const noexcept;

	/// @brief Turns around the target. @param dx Pixels right. @param dy Pixels down.
	/// @param radiansPerPixel How fast.
	void orbit(float dx, float dy, float radiansPerPixel = 0.008f) noexcept;
	/// @brief Moves the target with the pointer, so what is under it stays there.
	/// @param dx Pixels right. @param dy Pixels down. @param viewportHeight The viewport's height in pixels.
	void pan(float dx, float dy, float viewportHeight) noexcept;
	/// @brief Moves closer or further. @param steps Positive to come closer; each step is 10 %.
	void zoom(float steps) noexcept;
	/// @brief Looks at a box from the current direction so all of it shows. @param box It; empty boxes are ignored.
	void frame(const Aabb& box) noexcept;
	/// @brief Snaps the direction to a standard view. @param view Which.
	void snap(StandardView view) noexcept;

	/// @brief The ray under a pixel. @param x From the left. @param y From the top.
	/// @param width The viewport's width. @param height The viewport's height. @return The ray, origin on the near
	/// plane.
	[[nodiscard]] Ray rayThrough(float x, float y, float width, float height) const noexcept;
	/// @brief Where a point lands in the viewport. @param point In the world. @param width The viewport's width.
	/// @param height The viewport's height. @return Pixels from the top left, or nothing if behind the eye.
	[[nodiscard]] std::optional<std::pair<float, float>> project(Vec3 point, float width, float height) const noexcept;
};

/// @brief A camera as JSON: target, yaw, pitch, distance, orthographic, fieldOfView. @param camera It. @return The
/// object.
[[nodiscard]] json toJson(const OrbitCamera& camera);
/// @brief A camera from JSON; missing members keep their defaults and values are clamped to usable ranges.
/// @param value The object. @return The camera.
/// @throws DocumentError If it isn't an object or a member has the wrong type.
[[nodiscard]] OrbitCamera cameraFromJson(const json& value);

} // namespace affresco::scene3d
