/// @file
/// @brief Fuzzes the glTF loader (modules/scene3d/src/gltf/): Modello imports `.gltf` and `.glb` files a user
/// drops in its imports folder, so a damaged or hostile one must be refused with an error, never crash, read past its
/// bytes or go over its limits.
///
/// The input is a file's bytes, `.gltf` or `.glb`, loaded without a folder, so only embedded and `data:` buffers are
/// read. A loaded file stays within the (small) limits it was given.

#include "Fuzz.hpp"

#include <affresco/scene3d/Gltf.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>

namespace {

/// @brief Limits small enough that every input loads quickly.
/// @return Them.
affresco::scene3d::GltfLimits fuzzLimits() {
	affresco::scene3d::GltfLimits limits;
	limits.fileBytes = std::size_t{1} << 20;
	limits.bufferBytes = std::size_t{1} << 20;
	limits.nodes = 256;
	limits.meshes = 64;
	limits.materials = 64;
	limits.vertices = std::size_t{1} << 16;
	limits.indices = std::size_t{1} << 17;
	limits.images = 4;
	limits.imageSide = 256;
	limits.imagePixels = std::size_t{1} << 16;
	limits.animations = 16;
	limits.keys = std::size_t{1} << 14;
	limits.depth = 16;
	return limits;
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace scene3d = affresco::scene3d;
	const scene3d::GltfLimits limits = fuzzLimits();
	try {
		const scene3d::GltfImport loaded = scene3d::loadGltf(std::span(data, size), std::filesystem::path(), limits, "f_");
		if(loaded.scene.nodes.size() > limits.nodes || loaded.scene.materials.size() > limits.materials ||
		   loaded.animations.size() > limits.animations)
			affresco::fuzz::broken("a loaded file stays within its limits");
		for(const scene3d::Node& node : loaded.scene.nodes)
			if(!node.id.starts_with("f_")) affresco::fuzz::broken("every id a file makes carries the import's prefix");
	} catch(const std::exception&) { // NOLINT(bugprone-empty-catch): refusing is the expected outcome
		// refused, as a damaged file should be (GltfError, or a texture that would not decode)
	}
	return 0;
}
