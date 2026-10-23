/// @file
/// @brief Placing glTF files in a scene by name (SceneAssets.hpp).

#include "affresco/scene3d/SceneAssets.hpp"

#include "affresco/scene3d/SceneJson.hpp"

#include <set>

namespace affresco::scene3d {

namespace {

/// @brief Adds a file's nodes under the node that places it, with ids it prefixes.
/// @param placed The scene being placed. @param owner The placing node's id. @param file The file.
/// @param limits What the scene may hold. @param where Names the placing node, for errors.
/// @throws DocumentError If the scene would have too many nodes.
void placeNodes(PlacedScene& placed, const std::string& owner, const GltfImport& file, const SceneLimits& limits,
                const std::string& where) {
	json& nodes = placed.scene["nodes"];
	if(nodes.size() + file.scene.nodes.size() > limits.nodes)
		throw DocumentError(where + ": placing it makes more than " + std::to_string(limits.nodes) + " nodes");
	for(const Node& node : file.scene.nodes) {
		json entry = toJson(node);
		const std::string id = owner + "/" + node.id;
		entry["id"] = id;
		entry["parent"] = node.parent.empty() ? owner : owner + "/" + node.parent;
		placed.owners.emplace(id, owner);
		nodes.push_back(std::move(entry));
	}
}

/// @brief Adds a file's materials. @param placed The scene being placed. @param file The file.
/// @param limits What the scene may hold. @param where Names the placing node, for errors.
/// @throws DocumentError If the scene would have too many materials.
void placeMaterials(PlacedScene& placed, const GltfImport& file, const SceneLimits& limits, const std::string& where) {
	json& materials = placed.scene["materials"];
	if(!materials.is_array()) materials = json::array();
	if(materials.size() + file.scene.materials.size() > limits.materials)
		throw DocumentError(where + ": placing it makes more than " + std::to_string(limits.materials) + " materials");
	for(const Material& material : file.scene.materials) materials.push_back(toJson(material));
}

} // namespace

PlacedScene placeAssets(const json& scene, const AssetFiles& files, const SceneLimits& limits) {
	PlacedScene placed{scene, {}, {}};
	if(!scene.is_object() || !scene.contains("nodes") || !scene["nodes"].is_array()) return placed;
	const json& nodes = scene["nodes"];
	std::set<std::string, std::less<>> withMaterials;
	for(std::size_t i = 0; i < nodes.size(); ++i) {
		const json& node = nodes[i];
		if(!node.is_object() || !node.contains("asset") || !node["asset"].is_string() || !node.contains("id") ||
		   !node["id"].is_string())
			continue; // sceneFromJson() names a node without an id; an asset that isn't a name places nothing
		const std::string& name = node["asset"].get_ref<const std::string&>();
		const auto file = files.find(name);
		if(file == files.end()) {
			placed.missing.push_back(name);
			continue;
		}
		const std::string where = "scene.nodes[" + std::to_string(i) + "].asset";
		placeNodes(placed, node["id"].get<std::string>(), file->second, limits, where);
		if(withMaterials.insert(name).second) placeMaterials(placed, file->second, limits, where);
	}
	return placed;
}

std::string ownerOf(const PlacedScene& placed, const std::string& id) {
	const auto owner = placed.owners.find(id);
	return owner == placed.owners.end() ? id : owner->second;
}

} // namespace affresco::scene3d
