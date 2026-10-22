#pragma once

/// @file
/// @brief Gizmo maths: which handle a ray points at, and what dragging it (or typing a number) does to a transform.
/// Translate, rotate and scale; along an axis, in a plane or in the view; in world or local space; with snapping.

#include "affresco/scene3d/Bounds.hpp"
#include "affresco/scene3d/Camera.hpp"

#include <optional>
#include <string_view>

namespace affresco::scene3d {

/// @brief What a gizmo does.
enum class GizmoMode { Translate, Rotate, Scale };

/// @brief Which handle, or constraint: an axis, a plane (by the two axes in it), or the view (free).
enum class GizmoAxis { None, X, Y, Z, XY, YZ, ZX, View };

/// @brief A mode's name ("translate", "rotate", "scale"). @param mode It. @return The name.
[[nodiscard]] const char* gizmoModeName(GizmoMode mode) noexcept;
/// @brief The mode of a name. @param name It. @return The mode, or nothing.
[[nodiscard]] std::optional<GizmoMode> gizmoModeNamed(std::string_view name) noexcept;
/// @brief An axis's name ("none", "x", "y", "z", "xy", "yz", "zx", "view"). @param axis It. @return The name.
[[nodiscard]] const char* gizmoAxisName(GizmoAxis axis) noexcept;
/// @brief The axis of a name. @param name It. @return The axis, or nothing.
[[nodiscard]] std::optional<GizmoAxis> gizmoAxisNamed(std::string_view name) noexcept;

/// @brief Snapping steps; 0 turns one off.
struct GizmoSnap {
	float translate = 0.0f; ///< World units.
	float rotate = 0.0f;    ///< Radians.
	float scale = 0.0f;     ///< Of the factor.
};

/// @brief A gizmo placed in the world.
struct Gizmo {
	GizmoMode mode = GizmoMode::Translate; ///< What it does.
	Vec3 origin;                           ///< Where (the selection's pivot).
	Quat orientation;                      ///< Its axes: identity in world space, the node's rotation in local space.
	float size = 1.0f;                     ///< The handles' length in world units.

	/// @brief One of its axes in the world. @param index 0, 1 or 2. @return The unit vector.
	[[nodiscard]] Vec3 axis(int index) const noexcept;
};

/// @brief The world size that shows a fixed number of pixels at a point.
/// @param camera The view. @param point Where. @param viewportHeight In pixels. @param pixels How many.
/// @return World units.
[[nodiscard]] float worldSizeForPixels(const OrbitCamera& camera, Vec3 point, float viewportHeight,
                                       float pixels) noexcept;

/// @brief The handle a ray points at: the centre (free) handle first, else the nearest. @param gizmo The gizmo.
/// @param ray The ray.
/// @return It, or GizmoAxis::None.
[[nodiscard]] GizmoAxis hitHandle(const Gizmo& gizmo, const Ray& ray) noexcept;

/// @brief A change in world terms: moved by, turned by (about the gizmo's origin), scaled by (along its axes).
struct GizmoDelta {
	Vec3 translation;             ///< World offset.
	Quat rotation;                ///< World rotation.
	Vec3 scale{1.0f, 1.0f, 1.0f}; ///< Factors along the gizmo's axes.
};

/// @brief What a drag from one ray to another does.
/// @param gizmo The gizmo when the drag began. @param axis The handle or constraint.
/// @param start The ray at the press. @param current The ray now. @param snap The steps.
/// @return The change, or nothing if the rays miss the handle's line or plane.
[[nodiscard]] std::optional<GizmoDelta> dragGizmo(const Gizmo& gizmo, GizmoAxis axis, const Ray& start,
                                                  const Ray& current, const GizmoSnap& snap) noexcept;

/// @brief What a typed number does: units along an axis, degrees around it, or a factor along it.
/// @param gizmo The gizmo. @param axis An axis (X, Y or Z); a plane scales along its two axes; View scales along all
/// three and rotates about viewDirection (it doesn't translate). @param value The number.
/// @param viewDirection Where the camera looks.
/// @return The change.
[[nodiscard]] GizmoDelta numericDelta(const Gizmo& gizmo, GizmoAxis axis, float value, Vec3 viewDirection) noexcept;

/// @brief A node's local transform after a world change.
/// @param local Its transform at the start. @param parentWorld Its parent's world matrix (identity at the root).
/// @param gizmo The gizmo (its origin is the pivot, its axes the scale axes). @param delta The change.
/// @return The new local transform.
[[nodiscard]] Transform applyDelta(const Transform& local, const Mat4& parentWorld, const Gizmo& gizmo,
                                   const GizmoDelta& delta) noexcept;

} // namespace affresco::scene3d
