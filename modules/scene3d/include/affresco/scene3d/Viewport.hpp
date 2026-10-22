#pragma once

/// @file
/// @brief What a viewport shows, read from a surface's parameters: the camera, the selection, the shading, the grid,
/// the gizmo and the box being dragged; and the scene at a time of its animations. Pure, so the renderer and the
/// natives that pick and drag agree on what is on screen.

#include "affresco/core/Json.hpp"
#include "affresco/scene3d/Animation.hpp"
#include "affresco/scene3d/Camera.hpp"
#include "affresco/scene3d/Gizmo.hpp"
#include "affresco/scene3d/Scene.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace affresco::scene3d {

/// @brief How surfaces are shaded.
enum class Shading { Material, Solid, Wireframe };

/// @brief A shading's name ("material", "solid", "wireframe"). @param shading It. @return The name.
[[nodiscard]] const char* shadingName(Shading shading) noexcept;
/// @brief The shading of a name. @param name It. @return The shading, or nothing.
[[nodiscard]] std::optional<Shading> shadingNamed(std::string_view name) noexcept;

/// @brief The gizmo a viewport shows over its selection.
struct GizmoSettings {
	bool visible = false;                  ///< Shown at all.
	GizmoMode mode = GizmoMode::Translate; ///< What it does.
	bool local = false;                    ///< Along the active node's axes rather than the world's.
	GizmoAxis active = GizmoAxis::None;    ///< The handle being dragged or constrained to, drawn highlighted.
	float pixels = 90.0f;                  ///< Its handles' length on screen, in window units.
};

/// @brief Everything a viewport's parameters say, except the scene.
struct ViewportSettings {
	OrbitCamera camera;                      ///< The view.
	std::vector<std::string> selection;      ///< Selected node ids; the last is the active one.
	Shading shading = Shading::Material;     ///< How surfaces are shaded.
	bool grid = true;                        ///< The floor grid.
	bool axes = true;                        ///< The x and z axes on the floor.
	Vec3 background{0.16f, 0.165f, 0.18f};   ///< sRGB, 0..1.
	Vec3 gridColor{0.42f, 0.42f, 0.45f};     ///< sRGB, 0..1.
	Vec3 outline{1.0f, 0.55f, 0.12f};        ///< The selection outline's colour, sRGB, 0..1.
	float exposure = 1.0f;                   ///< Multiplies the light before tone mapping.
	float time = 0.0f;                       ///< Seconds into the animations.
	GizmoSettings gizmo;                     ///< The gizmo.
	std::optional<std::array<float, 4>> box; ///< A box being dragged: x0, y0, x1, y1 in window units.
};

/// @brief Reads a viewport's parameters, keeping the default for anything missing or malformed (never throws).
///
/// The keys are `camera` (see cameraFromJson()), `selection` (ids), `shading`, `grid`, `axes`, `background`,
/// `gridColor` and `outline` (`"#rrggbb"` or three numbers 0..1), `exposure`, `time`, `gizmo`
/// (`{ "visible", "mode", "space": "world" | "local", "axis" }`) and `box` (four numbers).
/// @param params The parameters.
/// @return The settings.
[[nodiscard]] ViewportSettings viewportSettingsFromJson(const json& params) noexcept;

/// @brief A colour from `"#rrggbb"` or three numbers 0..1. @param value It. @param fallback If it isn't one.
/// @return The colour, sRGB 0..1.
[[nodiscard]] Vec3 colorFromJson(const json& value, Vec3 fallback) noexcept;

/// @brief The scene a viewport shows: its `scene` parameter with its `animations` applied at a time.
/// @param scene The scene's JSON. @param animations Clips' JSON (see clipsFromJson()), or null.
/// @param time Seconds. @param limits What the scene may hold.
/// @return The scene.
/// @throws std::exception If the scene or the clips are malformed.
[[nodiscard]] Scene sceneAtTime(const json& scene, const json& animations, float time, const SceneLimits& limits = {});

/// @brief The gizmo over a selection: at the mean of the selected nodes' world positions, along the world's axes or
/// the active (last) node's, sized to a number of pixels.
/// @param scene The scene. @param settings The viewport's settings (selection, camera, gizmo).
/// @param viewportHeight In window units.
/// @return The gizmo, or nothing if it isn't shown or nothing selected is in the scene.
[[nodiscard]] std::optional<Gizmo> gizmoFor(const Scene& scene, const ViewportSettings& settings,
                                            float viewportHeight) noexcept;

} // namespace affresco::scene3d
