/// @file
/// @brief The steps that work a scene node: camera (with view, frame, projection, orbit and zoom), and transform (with
/// mode, axis and end), the modal move, turn or scale a key starts.

#include "Engine.hpp"

#include "affresco/scene3d/SceneView.hpp"

#include <vector>

namespace affresco::format {

namespace {

/// @brief What a camera step does, read and checked when it runs.
struct CameraMove {
	std::optional<scene3d::StandardView> view;      ///< A view to snap to.
	std::optional<std::vector<std::string>> frame;  ///< Ids to frame (none: the whole scene).
	bool frameSelection = false;                    ///< Frames the selection, as the view has it then.
	std::optional<std::string> projection;          ///< "perspective", "orthographic" or "toggle".
	std::optional<std::array<float, 2>> orbit;      ///< Degrees to turn, as a drag right and down.
	std::optional<float> zoom;                      ///< Wheel steps, positive to come closer.
};

/// @brief Reads "frame": "selection", "all", or a list of ids. @param s The step. @param move Where it goes.
/// @throws DocumentError If it is none of them.
void readFrame(const Running& s, CameraMove& move) {
	const json frame = s.value("frame");
	if(frame.is_string() && frame == "selection") move.frameSelection = true;
	else if(frame.is_string() && frame == "all") move.frame.emplace();
	else if(frame.is_array() && frame.size() <= 10'000) {
		move.frame.emplace();
		for(const json& id : frame) move.frame->push_back(text(id));
	} else fail(s.where + ".frame", R"(expected "selection", "all" or a list of ids)");
}

/// @brief Reads a camera step's members. @param s The step. @return What it does.
/// @throws DocumentError If a member is malformed.
CameraMove readCameraMove(const Running& s) {
	CameraMove move;
	if(s.step.contains("view")) {
		move.view = scene3d::standardViewNamed(text(s.value("view")));
		if(!move.view) fail(s.where + ".view", R"(expected "front", "back", "left", "right", "top" or "bottom")");
	}
	if(s.step.contains("frame")) readFrame(s, move);
	if(s.step.contains("projection")) {
		move.projection = text(s.value("projection"));
		if(*move.projection != "perspective" && *move.projection != "orthographic" && *move.projection != "toggle")
			fail(s.where + ".projection", R"(expected "perspective", "orthographic" or "toggle")");
	}
	if(s.step.contains("orbit")) {
		const json orbit = s.value("orbit");
		if(!orbit.is_array() || orbit.size() != 2 || !orbit[0].is_number() || !orbit[1].is_number())
			fail(s.where + ".orbit", "expected [degrees right, degrees down]");
		move.orbit = std::array<float, 2>{std::clamp(orbit[0].get<float>(), -360.0f, 360.0f),
		                                  std::clamp(orbit[1].get<float>(), -360.0f, 360.0f)};
	}
	if(s.step.contains("zoom")) move.zoom = std::clamp(number(s.value("zoom"), s.where + ".zoom"), -100.0f, 100.0f);
	return move;
}

/// @brief Moves a scene view's camera as a step says: projection, then view, frame, orbit and zoom.
/// @param scene The view. @param move What to do.
void moveCamera(scene3d::SceneView& scene, const CameraMove& move) {
	if(move.projection)
		scene.setOrthographic(*move.projection == "toggle" ? !scene.camera().lens.orthographic
		                                                   : *move.projection == "orthographic");
	if(move.view) scene.showView(*move.view);
	if(move.frameSelection) scene.frameNodes(scene.selection());
	else if(move.frame) scene.frameNodes(*move.frame);
	if(move.orbit) scene.orbit((*move.orbit)[0], (*move.orbit)[1]);
	if(move.zoom) scene.zoom(*move.zoom);
}

/// @brief Runs a camera step: on the scene node of that id, after the update and its layout, so it exists and has
/// its place and selection.
/// @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runSceneStep(const Running& s) {
	if(!s.step.contains("camera")) return std::nullopt;
	std::string id = text(s.value(s.step["camera"], s.where + ".camera"));
	if(id.starts_with('#')) id.erase(0, 1);
	const CameraMove move = readCameraMove(s);
	Engine& engine = s.steps.engine();
	if(!engine.window) return true;
	engine.scheduler.once(0.0, [&engine, id, move] {
		if(auto* scene = dynamic_cast<scene3d::SceneView*>(engine.window->root().findById(id))) moveCamera(*scene, move);
	});
	return true;
}

/// @brief What a transform step does, read and checked when it runs.
struct TransformMove {
	std::optional<scene3d::GizmoMode> mode; ///< Starts one doing this.
	std::optional<scene3d::GizmoAxis> axis; ///< Its constraint, or the running one's new one.
	std::optional<bool> confirm;            ///< Ends the running one: keeps it, or gives it up.
};

/// @brief Reads a transform step's members. @param s The step. @return What it does.
/// @throws DocumentError If a member is malformed.
TransformMove readTransformMove(const Running& s) {
	TransformMove move;
	if(s.step.contains("mode")) {
		move.mode = scene3d::gizmoModeNamed(text(s.value("mode")));
		if(!move.mode) fail(s.where + ".mode", R"(expected "translate", "rotate" or "scale")");
	}
	if(s.step.contains("axis")) {
		move.axis = scene3d::gizmoAxisNamed(text(s.value("axis")));
		if(!move.axis || *move.axis == scene3d::GizmoAxis::None)
			fail(s.where + ".axis", R"(expected "x", "y", "z", "xy", "yz", "zx" or "view")");
	}
	if(s.step.contains("end")) {
		const std::string end = text(s.value("end"));
		if(end != "confirm" && end != "cancel") fail(s.where + ".end", R"(expected "confirm" or "cancel")");
		move.confirm = end == "confirm";
	}
	if(move.confirm && move.mode) fail(s.where, "a transform step either starts one (mode) or ends one (end)");
	return move;
}

/// @brief Works a scene view's modal transform as a step says. @param scene The view. @param move What to do.
void moveTransform(scene3d::SceneView& scene, const TransformMove& move) {
	if(move.confirm) scene.endTransform(*move.confirm);
	else if(move.mode) {
		if(scene.transforming()) scene.endTransform(false); // a new key starts afresh, as G after R does
		(void)scene.startTransform(*move.mode, move.axis.value_or(scene3d::GizmoAxis::View));
	} else if(move.axis) scene.setTransformAxis(*move.axis);
}

/// @brief Runs a transform step, deferred as a camera step is. @param s The step.
/// @return std::nullopt if it isn't one, else true.
std::optional<bool> runTransformStep(const Running& s) {
	if(!s.step.contains("transform")) return std::nullopt;
	std::string id = text(s.value(s.step["transform"], s.where + ".transform"));
	if(id.starts_with('#')) id.erase(0, 1);
	const TransformMove move = readTransformMove(s);
	Engine& engine = s.steps.engine();
	if(!engine.window) return true;
	engine.scheduler.once(0.0, [&engine, id, move] {
		if(auto* scene = dynamic_cast<scene3d::SceneView*>(engine.window->root().findById(id)))
			moveTransform(*scene, move);
	});
	return true;
}

} // namespace

void registerSceneSteps(StepRegistry& registry) {
	registry.add({"scene", {{"camera", {"view", "frame", "projection", "orbit", "zoom"}}}, runSceneStep});
	registry.add({"scene", {{"transform", {"mode", "axis", "end"}}}, runTransformStep});
}

} // namespace affresco::format
