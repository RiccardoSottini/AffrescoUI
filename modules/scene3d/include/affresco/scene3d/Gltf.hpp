#pragma once

/// @file
/// @brief glTF 2.0: loading `.gltf` and `.glb` files (through cgltf) into a Scene, and exporting a scene as `.gltf`.
///
/// A file is untrusted input. Its buffers and images come only from the GLB's binary chunk, from `data:` URIs, or
/// from relative files in its own directory (no `..`, no absolute path, no scheme): never from the network. Every
/// count and size is checked against GltfLimits before anything is read. Extensions this loader doesn't decode
/// (Draco and meshopt compression, KTX2 textures) are skipped with a warning naming them: what can be loaded is.

#include "affresco/scene3d/Animation.hpp"
#include "affresco/scene3d/Scene.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace affresco::scene3d {

/// @brief A file that can't be loaded at all: malformed, over a limit, or asking for a refused buffer.
class GltfError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

/// @brief What a glTF file may hold.
struct GltfLimits {
	std::size_t fileBytes = std::size_t{256} << 20;   ///< The file, and each buffer and image file.
	std::size_t bufferBytes = std::size_t{512} << 20; ///< All buffers together.
	std::size_t nodes = 10000;                        ///< Nodes.
	std::size_t meshes = 10000;                       ///< Meshes, and primitives across them.
	std::size_t materials = 1000;                     ///< Materials.
	std::size_t vertices = std::size_t{8} << 20;      ///< Vertices across all primitives.
	std::size_t indices = std::size_t{24} << 20;      ///< Indices across all primitives.
	std::size_t images = 256;                         ///< Images.
	int imageSide = 8192;                             ///< The widest and tallest image.
	std::size_t imagePixels = std::size_t{1} << 24;   ///< The most pixels in one image (64 MiB decoded).
	std::size_t animations = 256;                     ///< Animations.
	std::size_t keys = std::size_t{1} << 20;          ///< Keyframes across all animation channels.
	std::size_t depth = 64;                           ///< The deepest node hierarchy.
};

/// @brief What a file held, ready to be merged into a scene.
struct GltfImport {
	Scene scene;                           ///< Its nodes and materials, with fresh ids.
	Assets assets;                         ///< Its meshes and textures, by the ids the scene uses.
	std::vector<AnimationClip> animations; ///< Its animations, on the scene's node ids.
	std::vector<std::string> warnings;     ///< What was skipped, and why.
};

/// @brief Loads a glTF file from memory.
/// @param bytes The `.gltf` (JSON) or `.glb` (binary) file.
/// @param directory Where relative buffer and image files are looked for; empty to refuse them.
/// @param limits What it may hold.
/// @param idPrefix Prefixed to every id it makes, so that several imports don't collide.
/// @return The scene.
/// @throws GltfError If it can't be loaded.
[[nodiscard]] GltfImport loadGltf(std::span<const std::uint8_t> bytes, const std::filesystem::path& directory,
                                  const GltfLimits& limits = {}, std::string_view idPrefix = "");

/// @brief Loads a glTF file.
/// @param file A `.gltf` or `.glb` file; relative buffers are looked for beside it.
/// @param limits What it may hold.
/// @param idPrefix Prefixed to every id it makes.
/// @return The scene.
/// @throws GltfError If it can't be read or loaded.
[[nodiscard]] GltfImport loadGltfFile(const std::filesystem::path& file, const GltfLimits& limits = {},
                                      std::string_view idPrefix = "");

/// @brief A scene as a `.gltf` file, its buffer embedded as a `data:` URI.
///
/// Nodes, meshes (positions, normals, texture coordinates, indices), material factors, lights
/// (KHR_lights_punctual), cameras and animations are written; textures are not (each one is named in the warnings).
/// @param scene The scene.
/// @param assets Its meshes.
/// @param animations Its animations.
/// @param warnings Receives what wasn't written; may be null.
/// @return The JSON text.
[[nodiscard]] std::string exportGltf(const Scene& scene, const Assets& assets,
                                     const std::vector<AnimationClip>& animations,
                                     std::vector<std::string>* warnings = nullptr);

/// @brief Whether a URI names a file this loader may read: relative, no `..`, no scheme, no drive, no backslash.
/// @param uri The URI, percent-decoded.
/// @return true if it may be read.
[[nodiscard]] bool isSafeRelativeUri(std::string_view uri) noexcept;

} // namespace affresco::scene3d
