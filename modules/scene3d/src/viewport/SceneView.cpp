/// @file
/// @brief The panel behind the `scene` node (SceneView.hpp): its gestures, picking, box select and gizmo drags.

#include "affresco/scene3d/SceneView.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/scene3d/Picking.hpp"
#include "affresco/scene3d/SceneJson.hpp"
#include "affresco/scene3d/Viewport.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string_view>

namespace affresco::scene3d {

namespace {

constexpr float dragPixels = 3.0f; ///< How far a press moves before it is a drag rather than a click.
constexpr std::string_view rendererName = "scene"; ///< The renderer's name in the window.

/// @brief What the pointer is doing.
enum class Gesture { None, Orbit, Pan, Pending, Moved, Box, Gizmo };

/// @brief A node a gizmo drag moves, as it was when the drag started.
struct DragStart {
	std::string id;      ///< The node.
	Transform transform; ///< Its local transform.
	Mat4 parentWorld;    ///< Its parent's world matrix.
};

/// @brief A loaded glTF file and where it came from.
struct LoadedFile {
	std::filesystem::path path;                ///< The file.
	std::filesystem::file_time_type modified;  ///< When it was last written, when it was loaded.
};

/// @brief Whether a binding starts on a press. @param binding It. @param button The button pressed.
/// @param mods The modifiers held. @return Whether it does.
bool starts(const PointerBinding& binding, MouseButton button, Mods mods) {
	return binding.enabled && binding.button == button && (mods.bits & 0x7) == binding.mods;
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

/// @brief The selected nodes a drag moves: those without a selected ancestor, which moves them already.
/// @param scene The scene. @param selection The selected ids. @return The ids, in selection order.
std::vector<std::string> movedRoots(const Scene& scene, const std::vector<std::string>& selection) {
	std::vector<std::string> roots;
	for(const std::string& id : selection) {
		bool underAnother = false;
		const Node* node = scene.node(id);
		for(std::size_t depth = 0; node && !node->parent.empty() && depth < scene.nodes.size(); ++depth) {
			if(std::ranges::find(selection, node->parent) != selection.end()) underAnother = true;
			node = scene.node(node->parent);
		}
		if(!underAnother && scene.node(id) && std::ranges::find(roots, id) == roots.end()) roots.push_back(id);
	}
	return roots;
}

/// @brief Words for a gizmo's change, for a status bar. @param mode What the gizmo does. @param delta The change.
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

/// @brief A scene's JSON with some nodes' transforms replaced. @param scene The JSON. @param nodes The nodes.
/// @return The JSON.
json withTransforms(json scene, const std::vector<MovedNode>& nodes) {
	if(!scene.is_object() || !scene.contains("nodes") || !scene["nodes"].is_array()) return scene;
	for(json& entry : scene["nodes"]) {
		if(!entry.is_object() || !entry.contains("id") || !entry["id"].is_string()) continue;
		const auto moved = std::ranges::find(nodes, entry["id"].get_ref<const std::string&>(), &MovedNode::id);
		if(moved == nodes.end()) continue;
		entry.erase("rotationDegrees"); // it would win over the rotation
		entry["translation"] = toJson(moved->transform.translation);
		entry["rotation"] = toJson(moved->transform.rotation);
		entry["scale"] = toJson(moved->transform.scale);
	}
	return scene;
}

/// @brief A number in an object, or a fallback. @param object It. @param key The member. @param fallback If none.
/// @return The number.
float numberIn(const json& object, const char* key, float fallback) {
	if(!object.is_object()) return fallback;
	const auto found = object.find(key);
	return found != object.end() && found->is_number() ? found->get<float>() : fallback;
}

} // namespace

std::optional<PointerBinding> pointerBindingNamed(std::string_view text) {
	if(text == "none") return PointerBinding{MouseButton::Left, 0, false};
	PointerBinding binding;
	constexpr std::array<std::pair<std::string_view, int>, 3> modifiers{
	    {{"shift+", 0x1}, {"control+", 0x2}, {"alt+", 0x4}}};
	for(bool found = true; found;) {
		found = false;
		for(const auto& [prefix, bit] : modifiers)
			if(text.starts_with(prefix) && (binding.mods & bit) == 0) {
				binding.mods |= bit;
				text.remove_prefix(prefix.size());
				found = true;
			}
	}
	if(text == "left") binding.button = MouseButton::Left;
	else if(text == "middle") binding.button = MouseButton::Middle;
	else if(text == "right") binding.button = MouseButton::Right;
	else return std::nullopt;
	return binding;
}

/// @brief The view's state: what it shows, the store, and the gesture under way.
struct SceneView::Internals {
	std::shared_ptr<Assets> assets = std::make_shared<Assets>();                ///< Primitives and loaded files.
	std::shared_ptr<ViewportProbe> probe = std::make_shared<ViewportProbe>();   ///< The renderer's stats and picks.
	AssetFiles files;                                                           ///< The loaded files, by name.
	std::map<std::string, LoadedFile, std::less<>> sources;                     ///< Where each came from.
	json sceneJson = json::object();                                            ///< The scene as given.
	json animations;                                                            ///< The clips as given.
	float time = 0.0f;                                                          ///< Seconds into them.
	PlacedScene placed{json::object(), {}, {}};                                 ///< The scene with its files placed.
	Scene scene;                                                                ///< It at its time.
	json display = json::object();                                              ///< How it looks.
	SceneControls controls;                                                     ///< The gestures it answers.
	OrbitCamera camera;                                                         ///< The view.
	std::vector<std::string> selection;                                         ///< The selected ids.
	Gesture gesture = Gesture::None;                                            ///< What the pointer does.
	MouseButton button = MouseButton::Left;                                     ///< The button that started it.
	bool shift = false;                                                         ///< Whether Shift was held then.
	Vec2 start;                                                                 ///< Where it started.
	Vec2 last;                                                                  ///< Where the pointer was last.
	std::optional<Gizmo> gizmo;                                                 ///< The gizmo being dragged.
	GizmoAxis axis = GizmoAxis::None;                                           ///< Its handle.
	std::vector<DragStart> dragged;                                             ///< The nodes it moves.
	SceneTransform transform;                                                   ///< The drag's last step.
	std::optional<json> preview;                                                ///< The scene with it applied.
	std::optional<std::array<float, 4>> box;                                    ///< The box being dragged.
};

SceneView::SceneView() : internals_(std::make_unique<Internals>()) {
	for(const Primitive primitive : {Primitive::Cube, Primitive::Sphere, Primitive::Cylinder, Primitive::Cone,
	                                 Primitive::Torus, Primitive::Plane})
		internals_->assets->setMesh(primitiveName(primitive), makePrimitive(primitive));
	std::shared_ptr<const Assets> assets = internals_->assets;
	setSurface(std::string(rendererName),
	           [assets, probe = internals_->probe] { return makeSceneRenderer(assets, probe); }, {.depth = true});
	refresh();
}

SceneView::~SceneView() = default;

void SceneView::setScene(const json& scene, const json& animations, float time) {
	const Internals& in = *internals_;
	if(time == in.time && scene == in.sceneJson && animations == in.animations) return;
	place(scene, animations, time);
}

void SceneView::place(const json& scene, const json& animations, float time) {
	PlacedScene placed = placeAssets(scene, internals_->files);
	Scene shown = sceneAtTime(placed.scene, animations, time);
	internals_->sceneJson = scene;
	internals_->animations = animations;
	internals_->time = time;
	internals_->placed = std::move(placed);
	internals_->scene = std::move(shown);
	if(internals_->gesture != Gesture::Gizmo) internals_->preview.reset(); // the document caught up with the drag
	refresh();
}

void SceneView::setDisplay(const json& display) {
	internals_->display = display.is_object() ? display : json::object();
	refresh();
}

void SceneView::setControls(const SceneControls& controls) { internals_->controls = controls; }

std::vector<std::string> SceneView::setAssets(const std::map<std::string, std::filesystem::path, std::less<>>& files,
                                              const GltfLimits& limits) {
	std::vector<std::string> errors;
	bool changed = std::erase_if(internals_->files, [&](const auto& file) { return !files.contains(file.first); }) > 0;
	std::erase_if(internals_->sources, [&](const auto& source) { return !files.contains(source.first); });
	for(const auto& [name, path] : files) {
		std::error_code error;
		const auto modified = std::filesystem::last_write_time(path, error);
		const auto source = internals_->sources.find(name);
		if(source != internals_->sources.end() && source->second.path == path && source->second.modified == modified)
			continue;
		changed = true;
		internals_->files.erase(name);
		internals_->sources.erase(name);
		try {
			GltfImport import = loadGltfFile(path, limits, name + ":");
			internals_->assets->merge(import.assets);
			internals_->files.emplace(name, std::move(import));
			internals_->sources.emplace(name, LoadedFile{path, modified});
		} catch(const std::exception& failure) {
			errors.push_back(name + ": " + failure.what());
			log(LogLevel::Warning, "scene: the model \"" + name + "\" wasn't loaded: " + failure.what());
		}
	}
	if(changed) try {
			place(internals_->sceneJson, internals_->animations, internals_->time);
		} catch(const std::exception& failure) {
			log(LogLevel::Warning, std::string("scene: ") + failure.what());
		}
	return errors;
}

void SceneView::setCamera(const OrbitCamera& camera) {
	internals_->camera = camera;
	refresh();
}

const OrbitCamera& SceneView::camera() const noexcept { return internals_->camera; }

void SceneView::setSelection(std::vector<std::string> ids) {
	internals_->selection = std::move(ids);
	refresh();
}

const std::vector<std::string>& SceneView::selection() const noexcept { return internals_->selection; }

void SceneView::showView(StandardView view) {
	internals_->camera.snap(view);
	cameraMoved();
}

void SceneView::frameNodes(const std::vector<std::string>& ids) {
	std::vector<std::string> under;
	for(const std::string& id : ids)
		for(std::string& node : internals_->scene.subtree(id)) under.push_back(std::move(node));
	Aabb box = ids.empty() ? Aabb{} : worldBounds(internals_->scene, *internals_->assets, under);
	if(box.empty()) box = worldBounds(internals_->scene, *internals_->assets);
	if(box.empty()) return;
	internals_->camera.frame(box);
	cameraMoved();
}

void SceneView::setOrthographic(bool orthographic) {
	internals_->camera.lens.orthographic = orthographic;
	cameraMoved();
}

void SceneView::zoom(float steps) {
	internals_->camera.zoom(steps);
	cameraMoved();
}

void SceneView::orbit(float yaw, float pitch) {
	internals_->camera.orbit(yaw, pitch, radians(1.0f));
	cameraMoved();
}

const Scene& SceneView::scene() const noexcept { return internals_->scene; }

const Assets& SceneView::assets() const noexcept { return *internals_->assets; }

const RenderStats& SceneView::stats() const noexcept { return internals_->probe->stats; }

void SceneView::mouseMoveEvent(const MouseMoveEvent& event) {
	SurfaceView::mouseMoveEvent(event);
	Internals& in = *internals_;
	const Vec2 at{event.position.x - frame().left, event.position.y - frame().top};
	const float dx = at.x - in.last.x, dy = at.y - in.last.y;
	in.last = at;
	switch(in.gesture) {
	case Gesture::Orbit:
		in.camera.orbit(dx, dy);
		cameraMoved();
		break;
	case Gesture::Pan:
		in.camera.pan(dx, dy, std::max(frame().height(), 1.0f));
		cameraMoved();
		break;
	case Gesture::Pending:
		if(std::hypot(at.x - in.start.x, at.y - in.start.y) > dragPixels)
			in.gesture = in.button == in.controls.select.button ? Gesture::Box : Gesture::Moved;
		if(in.gesture == Gesture::Box) {
			in.box = std::array<float, 4>{in.start.x, in.start.y, at.x, at.y};
			refresh();
		}
		break;
	case Gesture::Box:
		in.box = std::array<float, 4>{in.start.x, in.start.y, at.x, at.y};
		refresh();
		break;
	case Gesture::Gizmo:
		dragTo(at);
		break;
	case Gesture::None:
	case Gesture::Moved:
		break;
	}
}

void SceneView::mouseButtonEvent(const MouseButtonEvent& event) {
	SurfaceView::mouseButtonEvent(event);
	const Vec2 at{event.position.x - frame().left, event.position.y - frame().top};
	if(event.action == Action::Press) press(event.button, event.mods, at);
	else if(event.action == Action::Release) release(event.button, at);
}

void SceneView::scrollEvent(const ScrollEvent& event) {
	SurfaceView::scrollEvent(event);
	if(!internals_->controls.zoom || event.offset.y == 0.0f) return;
	internals_->camera.zoom(event.offset.y);
	cameraMoved();
}

void SceneView::press(MouseButton button, Mods mods, Vec2 at) {
	Internals& in = *internals_;
	if(in.gesture == Gesture::Gizmo && button != in.button) return endDrag(TransformPhase::Cancel);
	if(in.gesture != Gesture::None) return;
	in.preview.reset();
	in.button = button;
	in.shift = mods.shift();
	in.start = in.last = at;
	if(starts(in.controls.pan, button, mods)) in.gesture = Gesture::Pan;
	else if(starts(in.controls.orbit, button, mods)) in.gesture = Gesture::Orbit;
	else in.gesture = Gesture::Pending;
	if(in.gesture != Gesture::Pending || !in.controls.select.enabled || button != in.controls.select.button) return;
	ViewportSettings settings = viewportSettingsFromJson(params());
	settings.selection = in.selection;
	const float width = std::max(frame().width(), 1.0f), height = std::max(frame().height(), 1.0f);
	in.gizmo = gizmoFor(in.scene, settings, height);
	in.axis = in.gizmo ? hitHandle(*in.gizmo, in.camera.rayThrough(at.x, at.y, width, height)) : GizmoAxis::None;
	if(in.axis == GizmoAxis::None) return;
	in.gesture = Gesture::Gizmo;
	in.dragged.clear();
	const auto worlds = in.scene.worldMatrices();
	for(const std::string& id : movedRoots(in.scene, in.selection)) {
		const auto parent = worlds.find(in.scene.node(id)->parent);
		in.dragged.push_back({id, in.scene.node(id)->transform, parent == worlds.end() ? Mat4{} : parent->second.first});
	}
	in.transform = {TransformPhase::Preview, in.gizmo->mode, in.axis, {}, {}};
	for(const DragStart& node : in.dragged) in.transform.nodes.push_back({node.id, node.transform});
	refresh();
}

void SceneView::release(MouseButton button, Vec2 at) {
	Internals& in = *internals_;
	if(in.gesture == Gesture::None || button != in.button) return;
	const Gesture gesture = in.gesture;
	in.gesture = Gesture::None;
	if(gesture == Gesture::Pending) click(at, button, in.shift);
	else if(gesture == Gesture::Box) selectBox(in.shift);
	else if(gesture == Gesture::Gizmo) {
		in.gesture = Gesture::Gizmo; // endDrag() ends it
		endDrag(TransformPhase::End);
	}
}

void SceneView::click(Vec2 at, MouseButton button, bool shift) {
	Internals& in = *internals_;
	const float width = std::max(frame().width(), 1.0f), height = std::max(frame().height(), 1.0f);
	const Ray ray = in.camera.rayThrough(at.x, at.y, width, height);
	ScenePick pick{{}, button, shift, {at.x, at.y}, std::nullopt};
	const auto hit = pickNode(in.scene, *in.assets, ray);
	if(hit) pick.point = hit->point;
	else if(const auto floor = intersect(ray, Plane{{0.0f, 1.0f, 0.0f}, 0.0f}); floor && *floor > 0.0f)
		pick.point = ray.at(*floor);
	const auto& seen = in.probe->pick;
	if(seen && std::abs(seen->x - at.x) < 1.0f && std::abs(seen->y - at.y) < 1.0f)
		pick.node = seen->node; // the id pass is exact where the ray tests bounds and triangles alike
	else if(hit) pick.node = hit->node;
	in.probe->pick.reset();
	pick.node = ownerOf(in.placed, pick.node);
	if(in.controls.selects && in.controls.select.enabled && button == in.controls.select.button) {
		in.selection = clicked(in.selection, pick.node, shift);
		refresh();
		onViewChanged.emit();
	}
	onPick.emit(pick);
}

void SceneView::selectBox(bool shift) {
	Internals& in = *internals_;
	if(!in.box) return;
	const std::array<float, 4> box = *in.box;
	in.box.reset();
	SceneBoxSelect selected{{}, shift, box};
	const float width = std::max(frame().width(), 1.0f), height = std::max(frame().height(), 1.0f);
	for(const std::string& id :
	    boxSelect(in.scene, *in.assets, in.camera, width, height, box[0], box[1], box[2], box[3])) {
		std::string owner = ownerOf(in.placed, id);
		if(std::ranges::find(selected.nodes, owner) == selected.nodes.end()) selected.nodes.push_back(std::move(owner));
	}
	if(in.controls.selects) {
		std::vector<std::string> selection = shift ? in.selection : std::vector<std::string>{};
		for(const std::string& id : selected.nodes)
			if(std::ranges::find(selection, id) == selection.end()) selection.push_back(id);
		in.selection = std::move(selection);
	}
	refresh();
	if(in.controls.selects) onViewChanged.emit();
	onBoxSelect.emit(selected);
}

void SceneView::dragTo(Vec2 at) {
	Internals& in = *internals_;
	if(!in.gizmo) return;
	const float width = std::max(frame().width(), 1.0f), height = std::max(frame().height(), 1.0f);
	const json snapJson = in.display.contains("gizmo") ? in.display["gizmo"].value("snap", json::object()) : json();
	const GizmoSnap snap{numberIn(snapJson, "translate", 0.0f), radians(numberIn(snapJson, "rotate", 0.0f)),
	                     numberIn(snapJson, "scale", 0.0f)};
	const auto delta = scene3d::dragGizmo(*in.gizmo, in.axis, in.camera.rayThrough(in.start.x, in.start.y, width, height),
	                                      in.camera.rayThrough(at.x, at.y, width, height), snap);
	in.transform.nodes.clear();
	for(const DragStart& node : in.dragged)
		in.transform.nodes.push_back(
		    {node.id, applyDelta(node.transform, node.parentWorld, *in.gizmo, delta.value_or(GizmoDelta{}))});
	in.transform.text = delta ? describe(in.gizmo->mode, *delta) : std::string();
	in.preview = withTransforms(in.placed.scene, in.transform.nodes);
	refresh();
	onTransform.emit(in.transform);
}

void SceneView::endDrag(TransformPhase phase) {
	Internals& in = *internals_;
	if(in.gesture != Gesture::Gizmo) return;
	in.gesture = Gesture::None;
	in.transform.phase = phase;
	if(phase == TransformPhase::Cancel) {
		in.transform.nodes.clear();
		for(const DragStart& node : in.dragged) in.transform.nodes.push_back({node.id, node.transform});
		in.transform.text.clear();
		in.preview.reset();
	}
	in.gizmo.reset();
	in.axis = GizmoAxis::None;
	refresh();
	onTransform.emit(in.transform);
}

void SceneView::cameraMoved() {
	OrbitCamera& camera = internals_->camera;
	camera.distance = std::clamp(camera.distance, internals_->controls.minimumDistance,
	                             std::max(internals_->controls.maximumDistance, internals_->controls.minimumDistance));
	refresh();
	onViewChanged.emit();
}

void SceneView::refresh() {
	const Internals& in = *internals_;
	json params = in.display;
	params["scene"] = in.preview ? *in.preview : in.placed.scene;
	if(!in.animations.is_null()) params["animations"] = in.animations;
	params["time"] = in.time;
	params["camera"] = toJson(in.camera);
	params["selection"] = in.selection;
	json outlined = json::array();
	if(!in.placed.owners.empty()) {
		const std::set<std::string, std::less<>> selected(in.selection.begin(), in.selection.end());
		for(const auto& [id, owner] : in.placed.owners)
			if(selected.contains(owner)) outlined.push_back(id);
	}
	params["outlined"] = std::move(outlined);
	if(in.gesture == Gesture::Gizmo && params.contains("gizmo") && params["gizmo"].is_object())
		params["gizmo"]["axis"] = gizmoAxisName(in.axis);
	if(in.box) params["box"] = *in.box;
	setParams(params);
}

} // namespace affresco::scene3d
