#pragma once

/// @file
/// @brief Placing glTF files in a scene by name: a node with an `asset` member gets the file's nodes under it, so a
/// document places a model as it places a cube. Pure: the `scene` node, its tests and its fuzzer use it alike.

#include "affresco/core/Json.hpp"
#include "affresco/scene3d/Gltf.hpp"
#include "affresco/scene3d/Scene.hpp"

#include <map>
#include <string>
#include <vector>

namespace affresco::scene3d {

/// @brief Loaded glTF files, by the name a scene's nodes give in `asset`.
using AssetFiles = std::map<std::string, GltfImport, std::less<>>;

/// @brief A scene's JSON with its assets placed.
struct PlacedScene {
	json scene;                                             ///< `{"nodes", "materials"}`, with the files' added.
	std::map<std::string, std::string, std::less<>> owners; ///< Each added node's id, to the node that placed it.
	std::vector<std::string> missing;                       ///< The asset names no file was loaded for.
};

/// @brief Places the files a scene's nodes name.
///
/// A node `{"id": "r", "asset": "robot"}` gets every node of the file loaded as "robot" under it, with the id
/// "r/<its id>"; the file's materials are added once, however many nodes place it. Its meshes and textures stay in
/// its Assets, which the caller merges into the store the renderer reads.
/// @param scene The scene's JSON; anything but an object with an array of `nodes` is returned as it is
/// (sceneFromJson() reports it).
/// @param files The loaded files.
/// @param limits What the placed scene may hold.
/// @return The scene placed.
/// @throws DocumentError If placing would go over limits.nodes or limits.materials.
[[nodiscard]] PlacedScene placeAssets(const json& scene, const AssetFiles& files, const SceneLimits& limits = {});

/// @brief The node a click on a placed node selects: the node that placed it, or itself.
/// @param placed The placed scene. @param id The node hit.
/// @return The id to select.
[[nodiscard]] std::string ownerOf(const PlacedScene& placed, const std::string& id);

} // namespace affresco::scene3d
