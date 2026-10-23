/// @file
/// @brief Fuzzes what a `scene` node reads from a document (modules/scene3d, SceneType.cpp): its scene, its animations
/// and a glTF model placed by name. A scene comes from state an agent or a peer may set, and a model from a file the
/// document names, so a damaged or hostile one must be refused with an error, never crash or go over its limits.
///
/// The input is JSON: `{"gltf": <a .gltf file's JSON>, "scene": {"nodes", "materials"}, "animations": [...],
/// "time": seconds}`, each part optional. The glTF is loaded as the asset "m" (only `data:` buffers), placed where the
/// scene's nodes name it, and the scene read at the time, as the node does when it draws.

#include "Fuzz.hpp"

#include <affresco/scene3d/Gltf.hpp>
#include <affresco/scene3d/SceneAssets.hpp>
#include <affresco/scene3d/SceneJson.hpp>
#include <affresco/scene3d/Viewport.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>

namespace {

namespace scene3d = affresco::scene3d;

/// @brief Limits small enough that every input loads quickly.
/// @return Them.
scene3d::GltfLimits modelLimits() {
	scene3d::GltfLimits limits;
	limits.fileBytes = std::size_t{1} << 18;
	limits.bufferBytes = std::size_t{1} << 18;
	limits.nodes = 128;
	limits.meshes = 32;
	limits.materials = 32;
	limits.vertices = std::size_t{1} << 14;
	limits.indices = std::size_t{1} << 15;
	limits.images = 2;
	limits.imageSide = 64;
	limits.imagePixels = std::size_t{1} << 12;
	limits.animations = 8;
	limits.keys = std::size_t{1} << 12;
	limits.depth = 16;
	return limits;
}

/// @brief Loads the input's model as the asset "m", if it has one that loads.
/// @param input The input. @param files Where it goes.
void loadModel(const affresco::json& input, scene3d::AssetFiles& files) {
	const auto gltf = input.find("gltf");
	if(gltf == input.end()) return;
	const std::string bytes = gltf->dump();
	try {
		files.emplace("m", scene3d::loadGltf(std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()),
		                                     std::filesystem::path(), modelLimits(), "m_"));
	} catch(const std::exception&) { // NOLINT(bugprone-empty-catch): a model that doesn't load is left out
	}
}

/// @brief A member of the input, or null. @param input It. @param key The member. @return Its value.
affresco::json member(const affresco::json& input, const char* key) {
	const auto found = input.find(key);
	return found == input.end() ? affresco::json() : *found;
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const affresco::json input = affresco::fuzz::untrusted(affresco::fuzz::text(data, size));
	if(!input.is_object()) return 0;
	scene3d::AssetFiles files;
	loadModel(input, files);

	scene3d::SceneLimits limits;
	limits.nodes = 512;
	limits.materials = 64;
	limits.depth = 16;
	const affresco::json time = member(input, "time");
	try {
		const scene3d::PlacedScene placed = scene3d::placeAssets(member(input, "scene"), files, limits);
		const scene3d::Scene scene = scene3d::sceneAtTime(placed.scene, member(input, "animations"),
		                                                  time.is_number() ? static_cast<float>(std::clamp(time.get<double>(), -1e6, 1e6)) : 0.0f, limits);
		if(scene.nodes.size() > limits.nodes || scene.materials.size() > limits.materials)
			affresco::fuzz::broken("a placed scene stays within its limits");
		for(const auto& [id, owner] : placed.owners)
			if(!scene.node(id) || scene3d::ownerOf(placed, id) != owner)
				affresco::fuzz::broken("every placed node is in the scene, and a click on it selects its owner");
		const scene3d::Scene again = scene3d::sceneFromJson(scene3d::toJson(scene), limits);
		if(again.nodes.size() != scene.nodes.size() || again.materials.size() != scene.materials.size())
			affresco::fuzz::broken("a scene read back from its JSON is the same scene");
		(void)scene.worldMatrices();
	} catch(const std::exception&) { // NOLINT(bugprone-empty-catch): refusing is the expected outcome
		// refused, as a damaged scene should be (DocumentError naming the place)
	}
	return 0;
}
