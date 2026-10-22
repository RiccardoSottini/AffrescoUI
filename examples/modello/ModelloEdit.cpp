/// @file
/// @brief Modello's editing natives (ModelloInternal.hpp): adding, removing and duplicating nodes, material graph
/// nodes and checks, the timeline's tracks and keys, and the viewport's statistics. Each edit returns a JSON patch on
/// `computed.scene` or `computed.animations`, which the screen applies as a revision.

#include "ModelloInternal.hpp"

#include <affresco/scene3d/Animation.hpp>
#include <affresco/scene3d/MaterialGraph.hpp>
#include <affresco/scene3d/SceneJson.hpp>

#include <algorithm>
#include <set>

namespace modello::detail {

namespace {

using namespace affresco::scene3d;

/// @brief The most keys a call takes, as the renderer does.
constexpr std::size_t maxKeys = 100000;

/// @brief Appends to the scene's nodes or materials. @param patch Receives the operations.
/// @param scene The scene's JSON. @param member "nodes" or "materials". @param values A JSON array.
void appendToScene(json& patch, const json& scene, const std::string& member, const json& values) {
	appendTo(patch, scene.is_object() ? scene.value(member, json()) : json(), std::string(scenePath) + "/" + member,
	         values);
}

/// @brief A light of a kind, placed above the floor and pointing down. @param kind "point", "spot" or "directional".
/// @param node The node to hold it. @return Whether the kind is known.
bool makeLight(std::string_view kind, Node& node) {
	Light light;
	if(kind == "point") {
		light.type = LightType::Point;
		light.intensity = 40.0f;
		node.transform.translation = {0.0f, 3.0f, 0.0f};
	} else if(kind == "spot") {
		light.type = LightType::Spot;
		light.intensity = 80.0f;
		light.innerCone = 0.3f;
		light.outerCone = 0.6f;
		light.castsShadow = true;
		node.transform.translation = {0.0f, 4.0f, 0.0f};
		node.transform.rotation = Quat::fromAxisAngle({1.0f, 0.0f, 0.0f}, radians(-90.0f));
	} else if(kind == "directional") {
		light.intensity = 3.0f;
		light.castsShadow = true;
		node.transform.translation = {0.0f, 5.0f, 0.0f};
		node.transform.rotation = Quat::fromEuler({radians(-55.0f), radians(30.0f), 0.0f});
	} else return false;
	node.light = light;
	return true;
}

/// @brief A new node of a kind: a primitive (with a material of its own, standing on the floor), a light, a camera
/// or an empty. @param model The store (for the primitives' bounds). @param with The arguments (`scene`, `kind`).
/// @return `{ patch, selection, node }`.
json addNode(const Model& model, const json& with) {
	const json sceneJson = with.value("scene", json::object());
	const Scene scene = sceneFromJson(sceneJson);
	const std::string kind = textIn(with, "kind");
	const auto primitive = primitiveNamed(kind);
	const std::string prefix = kind.starts_with("light:") ? "light" : kind;
	Node node;
	node.id = scene.freshId(prefix.empty() ? "node" : prefix);
	node.name = node.id;
	json patch = json::array();
	if(primitive) {
		const auto mesh = model.assets->mesh(primitiveMesh(*primitive));
		node.mesh = primitiveMesh(*primitive);
		if(mesh && !mesh->bounds.empty()) node.transform.translation.y = -mesh->bounds.min.y;
		Material material;
		material.id = scene.freshId("material");
		if(material.id == node.id) material.id += "m";
		material.name = node.name;
		material.baseColor = {0.8f, 0.8f, 0.8f, 1.0f};
		node.material = material.id;
		appendToScene(patch, sceneJson, "materials", json::array({toJson(material)}));
	} else if(kind.starts_with("light:")) {
		if(!makeLight(std::string_view(kind).substr(6), node)) throw std::runtime_error("unknown light '" + kind + "'");
	} else if(kind == "camera") {
		node.camera = Lens{};
		node.transform.translation = {0.0f, 1.5f, 6.0f};
	} else if(kind != "empty") throw std::runtime_error("unknown kind '" + kind + "'");
	appendToScene(patch, sceneJson, "nodes", json::array({toJson(node)}));
	return {{"patch", std::move(patch)}, {"selection", json::array({node.id})}, {"node", node.id}};
}

/// @brief The selection and everything under it. @param scene The scene. @param selection The ids.
/// @return The ids.
std::set<std::string, std::less<>> withChildren(const Scene& scene, const std::vector<std::string>& selection) {
	std::set<std::string, std::less<>> ids;
	for(const std::string& id : selection)
		if(scene.node(id))
			for(std::string& below : scene.subtree(id)) ids.insert(std::move(below));
	return ids;
}

/// @brief The selected nodes and their children taken away, with the tracks that move them.
/// @param with The arguments (`scene`, `animations`, `selection`). @return `{ patch, selection }`.
json removeNodes(const json& with) {
	const Scene scene = sceneFromJson(with.value("scene", json::object()));
	const auto removed = withChildren(scene, selectionIn(with));
	json patch = json::array();
	for(std::size_t index = scene.nodes.size(); index-- > 0;)
		if(removed.contains(scene.nodes[index].id))
			patch.push_back({{"op", "remove"}, {"path", std::string(scenePath) + "/nodes/" + std::to_string(index)}});
	const json animations = with.value("animations", json());
	if(!patch.empty() && animations.is_array()) {
		json kept = animations;
		bool changed = false;
		for(json& clip : kept) {
			if(!clip.is_object() || !clip.contains("tracks") || !clip["tracks"].is_array()) continue;
			json& tracks = clip["tracks"];
			const auto before = tracks.size();
			tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
			                            [&](const json& track) {
				                            return track.is_object() && removed.contains(textIn(track, "node"));
			                            }),
			             tracks.end());
			changed = changed || tracks.size() != before;
		}
		if(changed) patch.push_back({{"op", "add"}, {"path", std::string(animationsPath)}, {"value", std::move(kept)}});
	}
	return {{"patch", std::move(patch)}, {"selection", json::array()}};
}

/// @brief Copies of the selected nodes and their children, in place, with fresh ids; the copies become the selection.
/// @param with The arguments (`scene`, `selection`). @return `{ patch, selection }`.
json duplicateNodes(const json& with) {
	const json sceneJson = with.value("scene", json::object());
	Scene scene = sceneFromJson(sceneJson);
	const std::vector<std::string> roots = movedRoots(scene, selectionIn(with));
	std::map<std::string, std::string, std::less<>> renamed;
	std::vector<std::string> order;
	for(const std::string& root : roots)
		for(const std::string& id : scene.subtree(root)) {
			Node placeholder; // takes the fresh id, so the next one differs
			placeholder.id = scene.freshId(id);
			renamed[id] = placeholder.id;
			order.push_back(id);
			scene.nodes.push_back(std::move(placeholder));
		}
	json copies = json::array();
	for(const std::string& id : order) {
		json copy = sceneJson["nodes"][indexOf(scene, id)];
		copy["id"] = renamed[id];
		const auto parent = renamed.find(textIn(copy, "parent"));
		if(parent != renamed.end()) copy["parent"] = parent->second;
		else copy["name"] = textIn(copy, "name").empty() ? renamed[id] : textIn(copy, "name") + " copy";
		copies.push_back(std::move(copy));
	}
	json patch = json::array();
	appendToScene(patch, sceneJson, "nodes", copies);
	json selection = json::array();
	for(const std::string& root : roots) selection.push_back(renamed[root]);
	return {{"patch", std::move(patch)}, {"selection", std::move(selection)}};
}

/// @brief A new material graph node with an id the graph doesn't use. @param with The arguments (`op`, `graph`,
/// `x`, `y`). @return `{ node }`.
json materialNode(const json& with) {
	std::set<std::string, std::less<>> used;
	const json graph = with.value("graph", json::object());
	if(graph.is_object() && graph.contains("nodes") && graph["nodes"].is_array())
		for(const json& node : graph["nodes"]) used.insert(textIn(node, "id"));
	std::string id;
	for(std::size_t n = 1; id.empty() || used.contains(id); ++n) id = "n" + std::to_string(n);
	json node = makeMaterialNode(textIn(with, "op"), id, numberIn(with, "x", 40.0f), numberIn(with, "y", 40.0f));
	if(node.is_null()) throw std::runtime_error("unknown operation '" + textIn(with, "op") + "'");
	return {{"node", std::move(node)}};
}

/// @brief Checks a material graph as the renderer will. @param with The arguments (`graph`).
/// @return `{ ok, errors }`.
json checkGraph(const json& with) {
	const MaterialGraphResult result = compileMaterialGraph(with.value("graph", json::object()));
	return {{"ok", result.ok()}, {"errors", result.errors}};
}

/// @brief A key's easing on the timeline. @param interpolation The track's. @return "step", "linear" or "bezier".
const char* easingOf(Interpolation interpolation) noexcept {
	if(interpolation == Interpolation::Step) return "step";
	return interpolation == Interpolation::Cubic ? "bezier" : "linear";
}

/// @brief The animations as the timeline's tracks: a group per clip, a track per node and part.
/// @param with The arguments (`animations`). @return `{ tracks, duration }`.
json timelineTracks(const json& with) {
	const json animations = with.value("animations", json());
	const auto clips = animations.is_array() ? clipsFromJson(animations, maxKeys) : std::vector<AnimationClip>{};
	json groups = json::array();
	float duration = 0.0f;
	for(std::size_t c = 0; c < clips.size(); ++c) {
		json tracks = json::array();
		for(std::size_t t = 0; t < clips[c].tracks.size(); ++t) {
			const Track& track = clips[c].tracks[t];
			const std::size_t stride = track.width() * (track.interpolation == Interpolation::Cubic ? 3 : 1);
			const std::size_t first = track.interpolation == Interpolation::Cubic ? track.width() : 0;
			json keyframes = json::array();
			for(std::size_t k = 0; k < track.times.size(); ++k)
				keyframes.push_back({{"time", track.times[k]},
				                     {"value", track.values[k * stride + first]},
				                     {"easing", easingOf(track.interpolation)}});
			tracks.push_back({{"id", std::to_string(c) + "/" + std::to_string(t)},
			                  {"label", track.node + " " + trackPathName(track.path)},
			                  {"keyframes", std::move(keyframes)}});
		}
		duration = std::max(duration, clips[c].duration());
		groups.push_back({{"id", "clip" + std::to_string(c)},
		                  {"label", clips[c].name.empty() ? "Clip " + std::to_string(c + 1) : clips[c].name},
		                  {"expanded", true},
		                  {"tracks", std::move(tracks)}});
	}
	return {{"tracks", std::move(groups)}, {"duration", duration}};
}

/// @brief The patch that replaces the animations. @param clips The new ones. @return It.
json animationsPatch(const std::vector<AnimationClip>& clips) {
	return json::array({{{"op", "add"}, {"path", std::string(animationsPath)}, {"value", toJson(clips)}}});
}

/// @brief Keys a node's translation, rotation and scale (as the scene holds them) at a time, into the first clip
/// (made, as "Action", if there is none). @param with The arguments (`scene`, `animations`, `node`, `time`).
/// @return `{ patch }`.
json keyNode(const json& with) {
	const Scene scene = sceneFromJson(with.value("scene", json::object()));
	const json animations = with.value("animations", json());
	auto clips = animations.is_array() ? clipsFromJson(animations, maxKeys) : std::vector<AnimationClip>{};
	const Node* node = scene.node(textIn(with, "node"));
	if(!node) throw std::runtime_error("no node to key");
	if(clips.empty()) clips.push_back(AnimationClip{"Action", {}});
	const float time = std::max(numberIn(with, "time", 0.0f), 0.0f);
	for(const TrackPath path : {TrackPath::Translation, TrackPath::Rotation, TrackPath::Scale}) {
		auto& tracks = clips.front().tracks;
		auto found = std::ranges::find_if(tracks, [&](const Track& t) { return t.node == node->id && t.path == path; });
		if(found == tracks.end()) {
			tracks.push_back(Track{node->id, path, Interpolation::Linear, {}, {}});
			found = std::prev(tracks.end());
		}
		setKey(*found, time, partValue(*node, path));
	}
	return {{"patch", animationsPatch(clips)}};
}

/// @brief Moves a key to another time. @param with The arguments (`animations`, `track` "clip/track", `index`,
/// `time`). @return `{ patch }`, empty if the key isn't there.
json moveKey(const json& with) {
	const json animations = with.value("animations", json());
	auto clips = animations.is_array() ? clipsFromJson(animations, maxKeys) : std::vector<AnimationClip>{};
	const std::string id = textIn(with, "track");
	const auto slash = id.find('/');
	std::size_t c = clips.size(), t = 0;
	try {
		if(slash != std::string::npos) c = std::stoul(id.substr(0, slash)), t = std::stoul(id.substr(slash + 1));
	} catch(const std::exception&) { c = clips.size(); }
	const float index = numberIn(with, "index", -1.0f);
	if(c >= clips.size() || t >= clips[c].tracks.size() || index < 0.0f) return {{"patch", json::array()}};
	Track& track = clips[c].tracks[t];
	const auto k = static_cast<std::size_t>(index);
	if(k >= track.times.size()) return {{"patch", json::array()}};
	const bool cubic = track.interpolation == Interpolation::Cubic;
	const std::size_t width = track.width(), stride = width * (cubic ? 3 : 1);
	std::array<float, 4> value{};
	std::copy_n(track.values.begin() + static_cast<std::ptrdiff_t>(k * stride + (cubic ? width : 0)), width,
	            value.begin());
	track.times.erase(track.times.begin() + static_cast<std::ptrdiff_t>(k));
	const auto from = track.values.begin() + static_cast<std::ptrdiff_t>(k * stride);
	track.values.erase(from, from + static_cast<std::ptrdiff_t>(stride));
	setKey(track, std::max(numberIn(with, "time", 0.0f), 0.0f), value);
	return {{"patch", animationsPatch(clips)}};
}

/// @brief What the viewport drew last. @param model The probe and the store. @return The counts.
json viewportStats(const Model& model) {
	const RenderStats& stats = model.probe->stats;
	std::size_t triangles = 0;
	for(const auto& [id, mesh] : model.assets->meshes())
		if(!id.starts_with("primitive:")) triangles += mesh->indices.size() / 3;
	return {{"drawn", stats.drawn},       {"culled", stats.culled},    {"triangles", stats.triangles},
	        {"gpuBytes", stats.gpuBytes}, {"evicted", stats.evicted},  {"frames", stats.frames},
	        {"error", stats.error},       {"meshes", model.assets->meshes().size()}, {"importedTriangles", triangles}};
}

} // namespace

void registerEditNatives(affresco::Screen& screen, const std::shared_ptr<Model>& model) {
	define(screen, "modello.add", [model](const json& with) { return addNode(*model, with); });
	define(screen, "modello.remove", removeNodes);
	define(screen, "modello.duplicate", duplicateNodes);
	define(screen, "modello.materialNode", materialNode);
	define(screen, "modello.checkGraph", checkGraph);
	define(screen, "modello.tracks", timelineTracks);
	define(screen, "modello.setKey", keyNode);
	define(screen, "modello.moveKey", moveKey);
	define(screen, "modello.stats", [model](const json&) { return viewportStats(*model); });
}

} // namespace modello::detail
