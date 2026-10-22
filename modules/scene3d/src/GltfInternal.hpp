#pragma once

/// @file
/// @brief What the glTF loader's two halves share: reading files and buffers (GltfLoad.cpp) and turning cgltf's
/// data into a Scene (GltfConvert.cpp).

#include "affresco/scene3d/Gltf.hpp"

#include <cgltf.h>

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace affresco::scene3d::detail {

/// @brief One load in progress.
struct GltfContext {
	const cgltf_data& data;                 ///< The parsed file, its buffers loaded and validated.
	const std::filesystem::path& directory; ///< Where relative files are; empty to refuse them.
	const GltfLimits& limits;               ///< What it may hold.
	std::string prefix;                     ///< Prefixed to every id.
	GltfImport& out;                        ///< Receives the result.
	std::map<std::pair<const cgltf_image*, bool>, std::string> textures;     ///< Images decoded so far, sRGB or not.
	std::vector<std::vector<std::pair<std::string, std::string>>> meshParts; ///< Per glTF mesh: (mesh id, material id).
};

/// @brief Reads a whole file of at most a size.
/// @param file It. @param maxBytes The largest accepted.
/// @return Its bytes.
/// @throws GltfError If it can't be read or is too large.
[[nodiscard]] std::vector<std::uint8_t> readLimited(const std::filesystem::path& file, std::size_t maxBytes);

/// @brief Whether every buffer an accessor reads (its sparse parts too) is loaded; cgltf_validate() has checked
/// the ranges.
/// @param accessor It.
/// @return true if it can be read.
[[nodiscard]] bool accessorInBounds(const cgltf_accessor& accessor) noexcept;

/// @brief Whether an accessor needs an extension this loader doesn't decode (meshopt compression).
/// @param accessor It.
/// @return true if it can't be read.
[[nodiscard]] bool accessorCompressed(const cgltf_accessor& accessor) noexcept;

/// @brief The id of a texture's image, decoded into the import's assets on first use.
/// @param context The load. @param texture The texture, or null. @param srgb Whether it holds colour.
/// @return The id, or empty if there is none or it can't be decoded (a warning says why).
[[nodiscard]] std::string textureId(GltfContext& context, const cgltf_texture* texture, bool srgb);

/// @brief Adds a warning once. @param context The load. @param warning The text.
void warn(GltfContext& context, const std::string& warning);

/// @brief Converts the materials. @param context The load.
void convertMaterials(GltfContext& context);
/// @brief Converts the meshes' primitives into assets, filling context.meshParts. @param context The load.
void convertMeshes(GltfContext& context);
/// @brief Converts the nodes, after the meshes. @param context The load.
void convertNodes(GltfContext& context);
/// @brief Converts the animations, after the nodes. @param context The load.
void convertAnimations(GltfContext& context);

/// @brief The id of the n-th object of a kind. @param context The load. @param kind E.g. "node". @param index n.
/// @return E.g. "importnode3".
[[nodiscard]] std::string makeId(const GltfContext& context, std::string_view kind, std::size_t index);

} // namespace affresco::scene3d::detail
