/// @file
/// @brief The "scene" node type: a 3D scene the document gives as JSON (scene3d's SceneJson.hpp), with glTF models
/// it places by name, drawn by the PBR renderer in a SceneView; its camera and selection are its value, and clicks,
/// boxes and gizmo drags are its events.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/scene3d/SceneJson.hpp"
#include "affresco/scene3d/SceneView.hpp"
#include "affresco/scene3d/Viewport.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

using scene3d::SceneView;

constexpr std::size_t maxAssets = 64;          ///< glTF files a scene node places at most.
constexpr std::size_t maxAssetName = 64;       ///< An asset's name, in bytes, at most.
constexpr std::size_t maxSelection = 10'000;   ///< Ids a view selects at most.
constexpr float maxDistance = 1e7f;            ///< The furthest a camera may be kept from its target.

/// @brief What one glTF file a document places may hold: less than a host's own files.
/// @return The limits.
scene3d::GltfLimits documentLimits() {
	scene3d::GltfLimits limits;
	limits.fileBytes = std::size_t{64} << 20;
	limits.bufferBytes = std::size_t{128} << 20;
	limits.nodes = 4096;
	limits.meshes = 4096;
	limits.materials = 256;
	limits.vertices = std::size_t{2} << 20;
	limits.indices = std::size_t{6} << 20;
	limits.images = 64;
	limits.imageSide = 4096;
	limits.animations = 64;
	limits.keys = std::size_t{1} << 18;
	limits.depth = 32;
	return limits;
}

/// @brief Reads a number within limits.
/// @param value The value. @param at Its place, for errors. @param low The least. @param high The most.
/// @return The number.
/// @throws DocumentError If it isn't a number within them.
float within(const json& value, const std::string& at, float low, float high) {
	const float read = number(value, at);
	if(std::isnan(read) || read < low || read > high)
		fail(at, "expected a number from " + text(json(low)) + " to " + text(json(high)));
	return read;
}

/// @brief Whether a name is one an asset may have: letters, digits, '-', '_' and '.', which its ids are prefixed with.
/// @param name The name. @return Whether it is.
bool assetName(const std::string& name) {
	return !name.empty() && name.size() <= maxAssetName && std::ranges::all_of(name, [](char c) {
		return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_' || c == '.';
	});
}

/// @brief Whether a file is a glTF file by its extension. @param file It. @return Whether it ends in .gltf or .glb.
bool gltfFile(const std::filesystem::path& file) {
	std::string extension = file.extension().string();
	std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return std::tolower(c); });
	return extension == ".gltf" || extension == ".glb";
}

/// @brief Loads the files "assets" names: {name: path}, each a .gltf or .glb file where images may come from.
/// @param scene The view. @param values The node's values. @param reader Resolves the files.
/// @param path The node's place, for errors.
/// @throws DocumentError If "assets" isn't such an object, has too many, or names a file outside those folders.
void applyAssets(SceneView& scene, const json& values, const PropertyReader& reader, const std::string& path) {
	std::map<std::string, std::filesystem::path, std::less<>> files;
	if(values.contains("assets")) {
		const json& given = object(values["assets"], path + ".assets");
		if(given.size() > maxAssets) fail(path + ".assets", "at most 64 models");
		for(const auto& [name, file] : given.items()) {
			const std::string at = concatenate(path, ".assets.", name);
			if(!assetName(name)) fail(at, "a name is up to 64 letters, digits, '-', '_' and '.'");
			std::filesystem::path resolved = reader.imageFile(file, at);
			if(!gltfFile(resolved)) fail(at, "expected a .gltf or .glb file");
			files.emplace(name, std::move(resolved));
		}
	}
	(void)scene.setAssets(files, documentLimits()); // a file that can't be loaded places nothing, and is logged
}

/// @brief Reads a point: three finite numbers, within a million of the origin.
/// @param value The value. @param at Its place, for errors. @return It.
/// @throws DocumentError If it isn't one.
json point(const json& value, const std::string& at) {
	if(!value.is_array() || value.size() != 3) fail(at, "expected [x, y, z]");
	for(std::size_t i = 0; i < 3; ++i) within(value[i], at + "[" + std::to_string(i) + "]", -1e6f, 1e6f);
	return value;
}

/// @brief Reads "gizmo": {visible, mode, space, pixels, pivot, snap: {translate, rotate, scale}}, checked.
/// @param value The value. @param at Its place, for errors. @return It.
/// @throws DocumentError If a member is of the wrong kind or out of range.
json readGizmo(const json& value, const std::string& at) {
	const json& gizmo = object(value, at);
	if(gizmo.contains("visible")) boolean(gizmo["visible"], at + ".visible");
	if(gizmo.contains("mode") && !scene3d::gizmoModeNamed(string(gizmo["mode"], at + ".mode")))
		fail(at + ".mode", R"(expected "translate", "rotate" or "scale")");
	if(gizmo.contains("space")) {
		const std::string space = string(gizmo["space"], at + ".space");
		if(space != "world" && space != "local") fail(at + ".space", R"(expected "world" or "local")");
	}
	if(gizmo.contains("pixels")) within(gizmo["pixels"], at + ".pixels", 10.0f, 400.0f);
	if(gizmo.contains("pivot") && !gizmo["pivot"].is_null()) point(gizmo["pivot"], at + ".pivot");
	if(gizmo.contains("snap")) {
		const json& snap = object(gizmo["snap"], at + ".snap");
		for(const char* key : {"translate", "rotate", "scale"})
			if(snap.contains(key)) within(snap[key], at + ".snap." + key, 0.0f, 1e6f);
	}
	return gizmo;
}

/// @brief How the scene looks, as the view's display reads it (SceneView::setDisplay()).
/// @param values The node's values. @param reader Reads colours. @param path The node's place, for errors.
/// @return The display.
json readDisplay(const json& values, const PropertyReader& reader, const std::string& path) {
	json display = json::object();
	if(values.contains("shading")) {
		const std::string shading = string(values["shading"], path + ".shading");
		if(!scene3d::shadingNamed(shading)) fail(path + ".shading", R"(expected "material", "solid" or "wireframe")");
		display["shading"] = shading;
	}
	for(const char* key : {"grid", "axes"})
		if(values.contains(key)) display[key] = boolean(values[key], path + "." + key);
	const auto color = [&](const char* key, const char* as) {
		if(!values.contains(key)) return;
		const Color read = reader.color(values[key], path + "." + key);
		display[as] = Color{read.r, read.g, read.b, 1.0f}.toHex();
	};
	color("background", "background");
	color("gridColor", "gridColor");
	color("outlineColor", "outline");
	if(values.contains("exposure")) display["exposure"] = within(values["exposure"], path + ".exposure", 0.0f, 64.0f);
	if(values.contains("gizmo")) display["gizmo"] = readGizmo(values["gizmo"], path + ".gizmo"); // else none
	if(values.contains("up")) {
		const std::string up = string(values["up"], path + ".up");
		if(up != "y" && up != "z") fail(path + ".up", R"(expected "y" or "z")");
		display["up"] = up;
	}
	if(values.contains("cursor3d") && !values["cursor3d"].is_null())
		display["cursor3d"] = point(values["cursor3d"], path + ".cursor3d");
	return display;
}

/// @brief Reads "controls": {orbit, pan, select (a button after any of "shift+", "control+" and "alt+", or
/// "none"), zoom, selects, minimumDistance, maximumDistance}.
/// @param values The node's values. @param path The node's place, for errors. @return The controls.
/// @throws DocumentError If a member is of the wrong kind or names no binding.
scene3d::SceneControls readControls(const json& values, const std::string& path) {
	scene3d::SceneControls controls;
	if(!values.contains("controls")) return controls;
	const std::string at = path + ".controls";
	const json& given = object(values["controls"], at);
	const auto binding = [&](const char* key, scene3d::PointerBinding& into) {
		if(!given.contains(key)) return;
		const auto read = scene3d::pointerBindingNamed(string(given[key], at + "." + key));
		if(!read) fail(at + "." + key, R"(expected "left", "middle" or "right", after any of "shift+", "control+" )"
		                               R"(and "alt+", or "none")");
		into = *read;
	};
	binding("orbit", controls.orbit);
	binding("pan", controls.pan);
	binding("select", controls.select);
	if(given.contains("zoom")) controls.zoom = boolean(given["zoom"], at + ".zoom");
	if(given.contains("selects")) controls.selects = boolean(given["selects"], at + ".selects");
	if(given.contains("minimumDistance"))
		controls.minimumDistance = within(given["minimumDistance"], at + ".minimumDistance", 0.0f, maxDistance);
	if(given.contains("maximumDistance"))
		controls.maximumDistance = within(given["maximumDistance"], at + ".maximumDistance", 0.0f, maxDistance);
	return controls;
}

/// @brief Sets a scene from its node: its models, how it looks, the gestures it answers, and the scene at its time.
/// @param panel The panel, a SceneView. @param values Its values, if given. @param reader Reads colours and files.
/// @param path The node's place, for errors.
/// @throws DocumentError If a value is malformed, or the scene is malformed or over its limits.
void applyScene(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* scene = dynamic_cast<SceneView*>(&panel);
	if(!scene) return;
	applyAssets(*scene, values, reader, path); // before the scene, which places them
	scene->setDisplay(readDisplay(values, reader, path));
	scene->setControls(readControls(values, path));
	const json animations = values.contains("animations") ? values["animations"] : json();
	if(!animations.is_null() && !animations.is_array()) fail(path + ".animations", "expected a list of clips");
	const float time = values.contains("time") ? within(values["time"], path + ".time", 0.0f, 1e7f) : 0.0f;
	try {
		scene->setScene(values.contains("scene") ? values["scene"] : json::object(), animations, time);
	} catch(const DocumentError& error) {
		fail(path + ".scene", error.what());
	}
}

/// @brief Shows the view: the bound value, else "view"; {camera: {target, yaw, pitch, distance, orthographic,
/// fieldOfView}, selection: [ids]}, each part only if given and well formed (the state is untrusted).
/// @param panel The panel, a SceneView. @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
void showView(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& /*lastText*/) {
	auto* scene = dynamic_cast<SceneView*>(&panel);
	const json& view = bound ? *bound : properties.contains("view") ? properties["view"] : emptyObject();
	if(!scene || !view.is_object()) return;
	if(const auto camera = view.find("camera"); camera != view.end() && camera->is_object()) try {
			scene->setCamera(scene3d::cameraFromJson(*camera));
		} catch(const std::exception&) {
			// a camera of the wrong kind leaves the one shown
		}
	if(const auto selection = view.find("selection"); selection != view.end() && selection->is_array()) {
		std::vector<std::string> ids;
		for(std::size_t i = 0; i < selection->size() && ids.size() < maxSelection; ++i)
			if((*selection)[i].is_string()) ids.push_back((*selection)[i].get<std::string>());
		if(ids != scene->selection()) scene->setSelection(std::move(ids));
	}
}

/// @brief The view of a scene, as showView() reads it. @param scene The view. @return {camera, selection}.
json viewOf(const SceneView& scene) {
	return {{"camera", scene3d::toJson(scene.camera())}, {"selection", scene.selection()}};
}

/// @brief Reports the view when a person changes it. @param panel The panel, a SceneView.
/// @param changed Gets the view. @return The connection.
Connection listenToView(Panel& panel, std::function<void(const json&)> changed) {
	auto* scene = dynamic_cast<SceneView*>(&panel);
	if(!scene) return 0;
	return scene->onViewChanged.connect([scene, changed = std::move(changed)] { changed(viewOf(*scene)); });
}

/// @brief Disconnects listenToView(). @param panel The panel. @param connection The connection.
void stopListeningToView(Panel& panel, Connection connection) {
	if(auto* scene = dynamic_cast<SceneView*>(&panel)) scene->onViewChanged.disconnect(connection);
}

/// @brief A button's name. @param button It. @return "left", "middle" or "right".
const char* buttonName(MouseButton button) {
	if(button == MouseButton::Right) return "right";
	return button == MouseButton::Middle ? "middle" : "left";
}

/// @brief A vector rounded to 1e-5, so that float noise doesn't reach a document. @param v It. @return The JSON.
json rounded(scene3d::Vec3 v) {
	const auto round = [](float x) { return std::round(static_cast<double>(x) * 1e5) / 1e5; };
	return json::array({round(v.x), round(v.y), round(v.z)});
}

/// @brief A gizmo drag as an event. @param transform It. @return {type: "transform", phase, mode, axis, nodes: [{id,
/// translation, rotation, rotationDegrees, scale}], text}.
json transformEvent(const scene3d::SceneTransform& transform) {
	static constexpr std::array<const char*, 3> phases{"preview", "end", "cancel"};
	json nodes = json::array();
	for(const scene3d::MovedNode& node : transform.nodes) {
		const scene3d::Vec3 angles = node.transform.rotation.toEuler();
		nodes.push_back({{"id", node.id},
		                 {"translation", rounded(node.transform.translation)},
		                 {"rotation", scene3d::toJson(node.transform.rotation)},
		                 {"rotationDegrees", rounded({scene3d::degrees(angles.x), scene3d::degrees(angles.y),
		                                              scene3d::degrees(angles.z)})},
		                 {"scale", rounded(node.transform.scale)}});
	}
	return {{"type", "transform"},
	        {"phase", phases.at(static_cast<std::size_t>(transform.phase))},
	        {"mode", scene3d::gizmoModeName(transform.mode)},
	        {"axis", scene3d::gizmoAxisName(transform.axis)},
	        {"nodes", std::move(nodes)},
	        {"text", transform.text}};
}

/// @brief Runs "onSelect" for a click (type "pick") or a box (type "boxSelect"), and "onTransform" for gizmo drags.
/// @param panel The panel, a SceneView. @param sink Runs a handler. @return The connections.
EventConnections listenToSceneEvents(Panel& panel, EventSink sink) {
	auto* scene = dynamic_cast<SceneView*>(&panel);
	if(!scene) return {};
	const Connection pick = scene->onPick.connect([scene, sink](const scene3d::ScenePick& pick) {
		sink("onSelect", json{{"type", "pick"},
		                      {"id", pick.node.empty() ? json() : json(pick.node)},
		                      {"button", buttonName(pick.button)},
		                      {"shift", pick.shift},
		                      {"x", pick.at[0]},
		                      {"y", pick.at[1]},
		                      {"point", pick.point ? rounded(*pick.point) : json()},
		                      {"selection", scene->selection()}});
	});
	const Connection box = scene->onBoxSelect.connect([scene, sink](const scene3d::SceneBoxSelect& box) {
		sink("onSelect", json{{"type", "boxSelect"},
		                      {"ids", box.nodes},
		                      {"shift", box.shift},
		                      {"box", box.box},
		                      {"selection", scene->selection()}});
	});
	const Connection transform = scene->onTransform.connect(
	    [sink](const scene3d::SceneTransform& transform) { sink("onTransform", transformEvent(transform)); });
	return {pick, box, transform, 0};
}

/// @brief Disconnects listenToSceneEvents(). @param panel The panel. @param connections The connections.
void stopListeningToSceneEvents(Panel& panel, const EventConnections& connections) {
	auto* scene = dynamic_cast<SceneView*>(&panel);
	if(!scene) return;
	scene->onPick.disconnect(connections[0]);
	scene->onBoxSelect.disconnect(connections[1]);
	scene->onTransform.disconnect(connections[2]);
}

} // namespace

void registerSceneType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},       {"view", ValueKind::Any},
	                   {"scene", ValueKind::Any},            {"assets", ValueKind::Any},
	                   {"animations", ValueKind::Any},       {"time", ValueKind::Number},
	                   {"shading", ValueKind::Text},         {"grid", ValueKind::Boolean},
	                   {"axes", ValueKind::Boolean},         {"background", ValueKind::Color},
	                   {"gridColor", ValueKind::Color},      {"outlineColor", ValueKind::Color},
	                   {"exposure", ValueKind::Number},      {"gizmo", ValueKind::Any},
	                   {"controls", ValueKind::Any},         {"up", ValueKind::Text},
	                   {"cursor3d", ValueKind::Any}};
	set->apply = applyScene;
	set->everyUpdate = false; // a scene of thousands of nodes: read again only when the values change
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const SceneView*>(&panel) != nullptr; };
	set->value = {"view", showView, listenToView, stopListeningToView};
	set->events = {listenToSceneEvents, stopListeningToSceneEvents};

	registry.add({"scene", "A 3D scene: primitives and glTF models, lights and materials, a camera and a selection",
	              [] { return std::make_unique<SceneView>(); }, set, false});
}

} // namespace affresco::format
