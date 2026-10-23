#pragma once

/// @file
/// @brief The panel behind the format's `scene` node: a 3D view of a scene it is given, drawn by the PBR renderer
/// (Renderer.hpp), that people orbit, pan, zoom, click, box select and drag the gizmo of. It owns the camera and the
/// selection while people change them and says so through its signals; the scene itself is the document's, so a
/// gizmo drag is shown as a preview and reported, and the document applies it.

#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"
#include "affresco/render/GpuSurface.hpp"
#include "affresco/render/Input.hpp"
#include "affresco/scene3d/Camera.hpp"
#include "affresco/scene3d/Gizmo.hpp"
#include "affresco/scene3d/Gltf.hpp"
#include "affresco/scene3d/Renderer.hpp"
#include "affresco/scene3d/SceneAssets.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::scene3d {

/// @brief A mouse button and the modifiers held with it that start a gesture.
struct PointerBinding {
	MouseButton button = MouseButton::Middle; ///< Which button.
	int mods = 0;                             ///< Shift (1), Control (2) and Alt (4): exactly these are held.
	bool enabled = true;                      ///< Whether the gesture is on at all.
};

/// @brief A binding from words: a button (`left`, `middle`, `right`) after any of `shift+`, `control+` and `alt+`,
/// or `none`. @param text The words. @return The binding, or nothing if they name none.
[[nodiscard]] std::optional<PointerBinding> pointerBindingNamed(std::string_view text);

/// @brief Which gestures a scene view answers.
struct SceneControls {
	PointerBinding orbit{MouseButton::Middle, 0};   ///< Turns the camera around its target.
	PointerBinding pan{MouseButton::Middle, 1};     ///< Moves the target with the pointer.
	PointerBinding select{MouseButton::Left, 0};    ///< Clicks pick, drags box select (Shift adds; any mods).
	bool zoom = true;                               ///< The wheel zooms.
	bool selects = true;                            ///< Picks change the selection, or are only reported.
	float minimumDistance = 0.05f;                  ///< The closest the camera comes to its target.
	float maximumDistance = 20000.0f;               ///< The furthest.
};

/// @brief A click: the node under it, or none.
struct ScenePick {
	std::string node;                       ///< The node under it (a placed model's own node), or empty for none.
	MouseButton button = MouseButton::Left; ///< Which button; only the select binding's changes the selection.
	bool shift = false;                     ///< Whether Shift was held.
	std::array<float, 2> at{};              ///< Where, in window units from the view's top left.
	std::optional<Vec3> point;              ///< Where in the world: on the surface hit, else on the floor (y = 0).
};

/// @brief A box dragged over the view.
struct SceneBoxSelect {
	std::vector<std::string> nodes; ///< The nodes in it.
	bool shift = false;             ///< Whether Shift was held: they were added to the selection.
	std::array<float, 4> box{};     ///< x0, y0, x1, y1 in window units from the view's top left.
};

/// @brief Where a gizmo drag is.
enum class TransformPhase { Preview, End, Cancel };

/// @brief A node moved by a gizmo drag.
struct MovedNode {
	std::string id;      ///< The node.
	Transform transform; ///< Its local transform now.
};

/// @brief A gizmo drag.
struct SceneTransform {
	TransformPhase phase = TransformPhase::Preview; ///< Under way, done, or given up (nodes back where they were).
	GizmoMode mode = GizmoMode::Translate;          ///< What the gizmo does.
	GizmoAxis axis = GizmoAxis::None;               ///< The handle.
	std::vector<MovedNode> nodes;                   ///< The nodes moved, and where.
	std::string text;                               ///< Words for it, e.g. "Move 1.25, 0, 0".
};

/// @brief A 3D view of a scene.
class SceneView : public SurfaceView {
public:
	/// @brief Shows an empty scene, with the six primitives in its store (`cube`, `sphere`, ...).
	SceneView();
	/// @brief Releases the store.
	~SceneView() override;
	SceneView(const SceneView&) = delete;
	SceneView& operator=(const SceneView&) = delete;

	/// @brief Shows a scene.
	/// @param scene Its JSON (SceneJson.hpp), whose nodes may place assets (SceneAssets.hpp).
	/// @param animations Clips' JSON, or null. @param time Seconds into them.
	/// @throws std::exception If the scene or the clips are malformed or over the limits; the view keeps the last.
	/// The same scene, clips and time again change nothing.
	void setScene(const json& scene, const json& animations, float time);
	/// @brief How it looks: `shading`, `grid`, `axes`, `background`, `gridColor`, `outline`, `exposure` and `gizmo`
	/// as viewportSettingsFromJson() reads them, with `gizmo.snap` `{translate, rotate (degrees), scale}`.
	/// @param display The object.
	void setDisplay(const json& display);
	/// @brief Which gestures it answers. @param controls Them.
	void setControls(const SceneControls& controls);
	/// @brief The glTF files the scene's nodes may place, loaded once per file until it changes on the disk.
	/// @param files By name, files the caller has checked it may read. @param limits What each may hold.
	/// @return What went wrong, one line per file that couldn't be loaded (it places nothing).
	std::vector<std::string> setAssets(const std::map<std::string, std::filesystem::path, std::less<>>& files,
	                                   const GltfLimits& limits);

	/// @brief Moves the camera (without a signal). @param camera It.
	void setCamera(const OrbitCamera& camera);
	/// @brief The camera. @return It.
	[[nodiscard]] const OrbitCamera& camera() const noexcept;
	/// @brief Selects nodes (without a signal). @param ids Them; the last is the active one.
	void setSelection(std::vector<std::string> ids);
	/// @brief The selection. @return The ids.
	[[nodiscard]] const std::vector<std::string>& selection() const noexcept;

	/// @brief Snaps the camera to a view, and signals it. @param view Which.
	void showView(StandardView view);
	/// @brief Frames nodes, and signals it. @param ids Them, or none for the whole scene.
	void frameNodes(const std::vector<std::string>& ids);
	/// @brief Switches between perspective and orthographic, and signals it. @param orthographic Which.
	void setOrthographic(bool orthographic);
	/// @brief Zooms as the wheel does, and signals it. @param steps Positive to come closer.
	void zoom(float steps);
	/// @brief Turns the camera as a drag right and down does, and signals it. @param yaw Degrees.
	/// @param pitch Degrees.
	void orbit(float yaw, float pitch);
	/// @brief Starts moving, turning or scaling the selection with the pointer, without a button held (a modal
	/// transform, as a key starts one): moves preview it, a press of the select button keeps it, any other cancels.
	/// @param mode What it does. @param axis Its constraint: View for free, or an axis or a plane.
	/// @return Whether it started: not while another gesture runs or with nothing selected.
	bool startTransform(GizmoMode mode, GizmoAxis axis = GizmoAxis::View);
	/// @brief Changes a modal transform's constraint, from where it started. @param axis The new one.
	void setTransformAxis(GizmoAxis axis);
	/// @brief Gives a modal transform a typed amount instead of the pointer's: units along its axis (X when it is
	/// free), degrees about it (the view's when free), or a factor along it (all three when free).
	/// @param value The amount, or nothing to follow the pointer again.
	void setTransformValue(std::optional<float> value);
	/// @brief Ends a gizmo drag or modal transform. @param confirm Whether to keep it, or give it up.
	void endTransform(bool confirm);
	/// @brief Whether a gizmo drag or modal transform runs. @return It.
	[[nodiscard]] bool transforming() const noexcept;

	/// @brief The scene shown, at its time, with its assets placed. @return It.
	[[nodiscard]] const Scene& scene() const noexcept;
	/// @brief The meshes and textures. @return The store.
	[[nodiscard]] const Assets& assets() const noexcept;
	/// @brief What the last frame drew. @return The stats.
	[[nodiscard]] const RenderStats& stats() const noexcept;

	Signal<> onViewChanged;                       ///< People moved the camera or changed the selection.
	Signal<const ScenePick&> onPick;              ///< A click.
	Signal<const SceneBoxSelect&> onBoxSelect;    ///< A box dragged.
	Signal<const SceneTransform&> onTransform;    ///< A gizmo drag moved, ended or was given up.

protected:
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void scrollEvent(const ScrollEvent& event) override;

private:
	struct Internals;

	/// @brief A press. @param button Which. @param mods The modifiers. @param at In window units from the top left.
	void press(MouseButton button, Mods mods, Vec2 at);
	/// @brief A release. @param button Which. @param at In window units from the top left.
	void release(MouseButton button, Vec2 at);
	/// @brief A click: a press and release without moving. @param at Where. @param button Which.
	/// @param shift Whether Shift was held.
	void click(Vec2 at, MouseButton button, bool shift);
	/// @brief A box dragged with the select binding. @param shift Whether Shift was held.
	void selectBox(bool shift);
	/// @brief Starts a gizmo drag of the selection's roots, with the gizmo and its axis set.
	void beginDrag();
	/// @brief The gizmo dragged to a point. @param at Where.
	void dragTo(Vec2 at);
	/// @brief Ends or gives up a gizmo drag. @param phase Which.
	void endDrag(TransformPhase phase);
	/// @brief Clamps the camera, shows it and signals it.
	void cameraMoved();
	/// @brief Places the scene's models and poses it at its time, and shows it.
	/// @param scene Its JSON. @param animations Clips' JSON, or null. @param time Seconds into them.
	/// @throws std::exception If the scene or the clips are malformed or over the limits; the view keeps the last.
	void place(const json& scene, const json& animations, float time);
	/// @brief Sends the renderer what to draw now.
	void refresh();

	std::unique_ptr<Internals> internals_; ///< The scene, the store, the camera and the gesture under way.
};

} // namespace affresco::scene3d
