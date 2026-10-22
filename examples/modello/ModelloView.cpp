/// @file
/// @brief Modello's viewport natives (ModelloInternal.hpp): the camera, picking, box select, and the gizmo's handles,
/// drags and typed numbers. They compute; the screen keeps the camera and the selection in its state and applies the
/// transforms' patches as revisions.

#include "ModelloInternal.hpp"

#include <affresco/scene3d/Picking.hpp>
#include <affresco/scene3d/SceneJson.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace modello::detail {

namespace {

using namespace affresco::scene3d;

/// @brief The `camera` argument, or the default view. @param with The arguments. @return It.
/// @throws affresco::DocumentError If it is malformed.
OrbitCamera cameraIn(const json& with) {
	return with.is_object() && with.contains("camera") ? cameraFromJson(with["camera"]) : OrbitCamera{};
}

/// @brief The ray under a point of the viewport. @param call The call. @param with Its `x` and `y`, in window units
/// from the viewport's left and top. @return It.
Ray rayAt(const ViewCall& call, const json& with) {
	return call.settings.camera.rayThrough(numberIn(with, "x", 0.0f), numberIn(with, "y", 0.0f), call.width,
	                                       call.height);
}

/// @brief The gizmo over the selection, shown whatever the viewport says, in a mode if one is given.
/// @param call The call. @param mode The `mode` argument, or empty for the viewport's. @return It, or nothing.
std::optional<Gizmo> gizmoOver(ViewCall& call, std::string_view mode) {
	call.settings.gizmo.visible = true;
	if(const auto named = gizmoModeNamed(mode)) call.settings.gizmo.mode = *named;
	return gizmoFor(call.scene, call.settings, call.height);
}

/// @brief A camera changed as `action` says. @param model The store (for framing). @param with The arguments.
/// @return `{ camera }`.
json moveCamera(const Model& model, const json& with) {
	OrbitCamera camera = cameraIn(with);
	const std::string action = textIn(with, "action");
	const float dx = numberIn(with, "dx", 0.0f), dy = numberIn(with, "dy", 0.0f);
	if(action == "orbit") camera.orbit(dx, dy);
	else if(action == "pan") camera.pan(dx, dy, std::max(numberIn(with, "height", 1.0f), 1.0f));
	else if(action == "zoom") camera.zoom(numberIn(with, "steps", 0.0f));
	else if(action == "projection") camera.lens.orthographic = !camera.lens.orthographic;
	else if(action == "view") {
		const auto view = standardViewNamed(textIn(with, "view"));
		if(!view) throw std::runtime_error("unknown view '" + textIn(with, "view") + "'");
		camera.snap(*view);
	} else if(action == "frame") {
		const Scene scene = sceneFromJson(with.value("scene", json::object()));
		Aabb box = worldBounds(scene, *model.assets, selectionIn(with));
		if(box.empty()) box = worldBounds(scene, *model.assets);
		if(!box.empty()) camera.frame(box);
	} else throw std::runtime_error("unknown camera action '" + action + "'");
	return {{"camera", toJson(camera)}};
}

/// @brief The selection after a click: the node alone, or toggled in with Shift; nothing clears it without Shift.
/// @param selection Before. @param node The node clicked, or empty. @param shift Whether Shift was held.
/// @return After.
std::vector<std::string> clicked(std::vector<std::string> selection, const std::string& node, bool shift) {
	if(!shift) return node.empty() ? std::vector<std::string>{} : std::vector<std::string>{node};
	if(node.empty()) return selection;
	const auto found = std::ranges::find(selection, node);
	if(found != selection.end()) selection.erase(found);
	else selection.push_back(node);
	return selection;
}

/// @brief The node under a click: the viewport's id pass if it saw this click, else a ray through the meshes.
/// @param model The store and the probe. @param with The arguments (`x`, `y`, `shift`).
/// @return `{ selection, node }`.
json pick(Model& model, const json& with) {
	const ViewCall call = readView(with);
	std::string node;
	bool shift = with.value("shift", false);
	const float x = numberIn(with, "x", 0.0f), y = numberIn(with, "y", 0.0f);
	if(const auto& seen = model.probe->pick; seen && std::abs(seen->x - x) < 1.0f && std::abs(seen->y - y) < 1.0f) {
		node = seen->node; // the id pass is exact where the ray tests bounds and triangles alike
		shift = shift || seen->shift;
	} else if(const auto hit = pickNode(call.scene, *model.assets, rayAt(call, with))) node = hit->node;
	model.probe->pick.reset();
	return {{"selection", clicked(selectionIn(with), node, shift)}, {"node", node.empty() ? json() : json(node)}};
}

/// @brief The nodes in a dragged box, added to the selection with Shift. @param model The store.
/// @param with The arguments (`box`: x0, y0, x1, y1; `shift`). @return `{ selection }`.
json selectBox(const Model& model, const json& with) {
	const ViewCall call = readView(with);
	const json box = with.value("box", json::array());
	if(!box.is_array() || box.size() != 4 || !std::ranges::all_of(box, &json::is_number))
		throw std::runtime_error("box is not four numbers");
	const auto at = [&](std::size_t i) { return box[i].get<float>(); };
	std::vector<std::string> inside =
	    boxSelect(call.scene, *model.assets, call.settings.camera, call.width, call.height, at(0), at(1), at(2), at(3));
	if(with.value("shift", false)) {
		std::vector<std::string> selection = selectionIn(with);
		for(std::string& id : inside)
			if(std::ranges::find(selection, id) == selection.end()) selection.push_back(std::move(id));
		inside = std::move(selection);
	}
	return {{"selection", inside}};
}

/// @brief The handle under the pointer. @param with The arguments (`x`, `y`). @return `{ axis }`: a name or "none".
json gizmoHit(const json& with) {
	ViewCall call = readView(with);
	const auto gizmo = gizmoOver(call, textIn(with, "mode"));
	const GizmoAxis axis = gizmo ? hitHandle(*gizmo, rayAt(call, with)) : GizmoAxis::None;
	return {{"axis", gizmoAxisName(axis)}};
}

/// @brief A change applied to the selection's moved nodes. @param with The arguments (the scene's JSON).
/// @param call The call (the scene at its time). @param gizmo The gizmo. @param delta The change.
/// @return `{ preview, patch, moved }`: the scene's JSON with the new transforms, the patch that makes them, the ids.
json applyToSelection(const json& with, ViewCall& call, const Gizmo& gizmo, const GizmoDelta& delta) {
	const std::vector<std::string> moved = movedRoots(call.scene, call.settings.selection);
	const auto worlds = call.scene.worldMatrices();
	json preview = with["scene"];
	for(const std::string& id : moved) {
		Node& node = *call.scene.node(id);
		const auto parent = worlds.find(node.parent);
		node.transform = applyDelta(node.transform, parent == worlds.end() ? Mat4{} : parent->second.first, gizmo, delta);
		json& entry = preview["nodes"][indexOf(call.scene, id)];
		entry["translation"] = toJson(node.transform.translation);
		entry["rotation"] = toJson(node.transform.rotation);
		entry["scale"] = toJson(node.transform.scale);
	}
	return {{"preview", std::move(preview)}, {"patch", transformPatch(call.scene, moved)}, {"moved", moved}};
}

/// @brief Words for a change, for the status bar. @param mode What the gizmo does. @param delta The change.
/// @return E.g. "Move 1.25, 0, 0".
std::string describe(GizmoMode mode, const GizmoDelta& delta) {
	std::array<char, 96> text{};
	if(mode == GizmoMode::Translate)
		std::snprintf(text.data(), text.size(), "Move %.3g, %.3g, %.3g", static_cast<double>(delta.translation.x),
		              static_cast<double>(delta.translation.y), static_cast<double>(delta.translation.z));
	else if(mode == GizmoMode::Scale)
		std::snprintf(text.data(), text.size(), "Scale %.3g, %.3g, %.3g", static_cast<double>(delta.scale.x),
		              static_cast<double>(delta.scale.y), static_cast<double>(delta.scale.z));
	else {
		const float angle = 2.0f * std::acos(std::clamp(std::abs(delta.rotation.w), 0.0f, 1.0f));
		std::snprintf(text.data(), text.size(), "Rotate %.3g°", static_cast<double>(degrees(angle)));
	}
	return text.data();
}

/// @brief A handle dragged from `start` to `x`, `y`. @param with The arguments (`axis`, `start`, `x`, `y`, `snap`
/// `{ translate, rotate (degrees), scale }`). @return See applyToSelection(), and `text`.
json gizmoDrag(const json& with) {
	ViewCall call = readView(with);
	const auto gizmo = gizmoOver(call, textIn(with, "mode"));
	const auto axis = gizmoAxisNamed(textIn(with, "axis"));
	const json start = with.value("start", json::array());
	if(!gizmo || !axis || !start.is_array() || start.size() != 2 || !start[0].is_number() || !start[1].is_number())
		return {{"preview", with["scene"]}, {"patch", json::array()}, {"moved", json::array()}, {"text", ""}};
	const json snapJson = with.value("snap", json::object());
	const GizmoSnap snap{numberIn(snapJson, "translate", 0.0f), radians(numberIn(snapJson, "rotate", 0.0f)),
	                     numberIn(snapJson, "scale", 0.0f)};
	const Ray from = call.settings.camera.rayThrough(start[0].get<float>(), start[1].get<float>(), call.width,
	                                                 call.height);
	const auto delta = dragGizmo(*gizmo, *axis, from, rayAt(call, with), snap);
	json result = applyToSelection(with, call, *gizmo, delta.value_or(GizmoDelta{}));
	result["text"] = delta ? describe(gizmo->mode, *delta) : "";
	return result;
}

/// @brief A typed number applied as G, R or S would. @param with The arguments (`mode`, `axis`, `value`).
/// @return See applyToSelection(), and `text`.
json numeric(const json& with) {
	ViewCall call = readView(with);
	const auto gizmo = gizmoOver(call, textIn(with, "mode"));
	const GizmoAxis axis = gizmoAxisNamed(textIn(with, "axis")).value_or(GizmoAxis::View);
	if(!gizmo) return {{"preview", with["scene"]}, {"patch", json::array()}, {"moved", json::array()}, {"text", ""}};
	const OrbitCamera& camera = call.settings.camera;
	const GizmoDelta delta =
	    numericDelta(*gizmo, axis, numberIn(with, "value", 0.0f), normalize(camera.target - camera.eye()));
	json result = applyToSelection(with, call, *gizmo, delta);
	result["text"] = describe(gizmo->mode, delta);
	return result;
}

} // namespace

void registerViewNatives(affresco::Screen& screen, const std::shared_ptr<Model>& model) {
	define(screen, "modello.camera", [model](const json& with) { return moveCamera(*model, with); });
	define(screen, "modello.pick", [model](const json& with) { return pick(*model, with); });
	define(screen, "modello.boxSelect", [model](const json& with) { return selectBox(*model, with); });
	define(screen, "modello.gizmoHit", gizmoHit);
	define(screen, "modello.gizmoDrag", gizmoDrag);
	define(screen, "modello.numeric", numeric);
}

} // namespace modello::detail
