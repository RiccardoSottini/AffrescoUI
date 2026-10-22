/// @file
/// @brief Modello's model, the `scene` surface, and the helpers its natives share (Modello.hpp, ModelloInternal.hpp).

#include "ModelloInternal.hpp"

#include <affresco/scene3d/Picking.hpp>
#include <affresco/scene3d/SceneJson.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace modello {

using affresco::json;
using namespace affresco::scene3d;

Model::Model(std::filesystem::path samplesFolder, std::filesystem::path dataFolder)
    : assets(std::make_shared<Assets>()), samples(std::move(samplesFolder)), data(std::move(dataFolder)) {
	for(const Primitive primitive :
	    {Primitive::Cube, Primitive::Sphere, Primitive::Cylinder, Primitive::Torus, Primitive::Plane})
		assets->setMesh(primitiveMesh(primitive), makePrimitive(primitive));
}

std::string primitiveMesh(Primitive primitive) { return std::string("primitive:") + primitiveName(primitive); }

void registerModello(affresco::Screen& screen, const std::shared_ptr<Model>& model) {
	screen.registerSurface(
	    "scene", [model] { return makeSceneRenderer(model->assets, model->probe); }, {true});
	detail::registerViewNatives(screen, model);
	detail::registerEditNatives(screen, model);
	detail::registerFileNatives(screen, model);
}

namespace detail {

ViewCall readView(const json& with) {
	if(!with.is_object() || !with.contains("scene")) throw std::runtime_error("no scene");
	ViewCall call;
	call.settings = viewportSettingsFromJson(with.value("view", json::object()));
	call.scene = sceneAtTime(with["scene"], with.value("animations", json()), call.settings.time);
	call.width = std::max(numberIn(with, "width", 1.0f), 1.0f);
	call.height = std::max(numberIn(with, "height", 1.0f), 1.0f);
	return call;
}

float numberIn(const json& with, std::string_view key, float fallback) noexcept {
	if(!with.is_object()) return fallback;
	const auto found = with.find(key);
	if(found == with.end() || !found->is_number()) return fallback;
	const auto value = found->get<double>();
	return std::isfinite(value) ? static_cast<float>(std::clamp(value, -1e9, 1e9)) : fallback;
}

std::string textIn(const json& with, std::string_view key) {
	if(!with.is_object()) return {};
	const auto found = with.find(key);
	return found != with.end() && found->is_string() ? found->get<std::string>() : std::string();
}

std::vector<std::string> selectionIn(const json& with) {
	std::vector<std::string> ids;
	if(!with.is_object()) return ids;
	const auto found = with.find("selection");
	if(found == with.end() || !found->is_array()) return ids;
	for(const json& id : *found)
		if(id.is_string()) ids.push_back(id.get<std::string>());
	return ids;
}

std::size_t indexOf(const Scene& scene, std::string_view id) noexcept {
	const auto found = std::ranges::find(scene.nodes, id, &Node::id);
	return static_cast<std::size_t>(found - scene.nodes.begin());
}

json transformPatch(const Scene& scene, const std::vector<std::string>& ids) {
	json patch = json::array();
	for(const std::string& id : ids) {
		const std::size_t index = indexOf(scene, id);
		if(index == scene.nodes.size()) continue;
		const Transform& transform = scene.nodes[index].transform;
		const std::string node = std::string(scenePath) + "/nodes/" + std::to_string(index) + "/";
		// "add" sets a member whether or not the node's JSON had it
		patch.push_back({{"op", "add"}, {"path", node + "translation"}, {"value", toJson(transform.translation)}});
		patch.push_back({{"op", "add"}, {"path", node + "rotation"}, {"value", toJson(transform.rotation)}});
		patch.push_back({{"op", "add"}, {"path", node + "scale"}, {"value", toJson(transform.scale)}});
	}
	return patch;
}

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

void appendTo(json& patch, const json& array, const std::string& path, const json& values) {
	if(values.empty()) return;
	if(!array.is_array()) {
		patch.push_back({{"op", "add"}, {"path", path}, {"value", values}});
		return;
	}
	for(const json& value : values) patch.push_back({{"op", "add"}, {"path", path + "/-"}, {"value", value}});
}

void define(affresco::Screen& screen, const std::string& name, std::function<json(const json&)> call) {
	screen.onCall(name, [call = std::move(call)](const std::string& arguments) {
		return call(affresco::parseUntrusted(arguments)).dump();
	});
}

} // namespace detail

} // namespace modello
