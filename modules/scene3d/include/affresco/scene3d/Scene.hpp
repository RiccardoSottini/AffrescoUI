#pragma once

/// @file
/// @brief A scene: nodes in a hierarchy with meshes, materials, lights and cameras, and the store of their heavy data.
///
/// The Scene is small and goes into a document's state as JSON (SceneJson.hpp): names, transforms, material factors,
/// ids. The vertices and pixels it refers to by id stay in an Assets store on the host, never in JSON.

#include "affresco/scene3d/Mesh.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::scene3d {

/// @brief The kinds of light.
enum class LightType { Directional, Point, Spot };

/// @brief A light, at its node's position and pointing down its node's -z (as KHR_lights_punctual).
struct Light {
	LightType type = LightType::Directional; ///< Which kind.
	Vec3 color{1.0f, 1.0f, 1.0f};            ///< Linear RGB.
	float intensity = 1.0f;                  ///< Lux for directional, candela for point and spot.
	float range = 0.0f;                      ///< Where a point or spot light ends; 0 for no end.
	float innerCone = 0.0f;                  ///< A spot's full-strength angle from its axis, radians.
	float outerCone = 0.7853982f;            ///< A spot's cut-off angle from its axis, radians.
	bool castsShadow = false;                ///< Whether it draws the shadow map (one light at a time does).

	/// @brief Equal when every field is equal.
	bool operator==(const Light&) const noexcept = default;
};

/// @brief A camera's lens, looking down its node's -z.
struct Lens {
	bool orthographic = false; ///< Perspective if false.
	float fieldOfView = 0.8f;  ///< Vertical, radians (perspective).
	float halfHeight = 1.0f;   ///< Half the visible height (orthographic).
	float nearPlane = 0.05f;   ///< The near distance.
	float farPlane = 1000.0f;  ///< The far distance.

	/// @brief Equal when every field is equal.
	bool operator==(const Lens&) const noexcept = default;
};

/// @brief How a material's alpha is used (as glTF).
enum class AlphaMode { Opaque, Mask, Blend };

/// @brief A PBR metallic-roughness material: factors, and textures by id in the Assets store.
struct Material {
	std::string id;                          ///< Unique in the scene.
	std::string name;                        ///< Shown to people.
	Vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};  ///< Linear RGBA.
	float metallic = 0.0f;                   ///< 0 dielectric, 1 metal.
	float roughness = 0.5f;                  ///< 0 mirror, 1 matte.
	Vec3 emissive;                           ///< Linear RGB.
	AlphaMode alphaMode = AlphaMode::Opaque; ///< How alpha is used.
	float alphaCutoff = 0.5f;                ///< For AlphaMode::Mask.
	bool doubleSided = false;                ///< Whether back faces are drawn.
	std::string baseColorTexture;            ///< A texture id, or empty.
	std::string metallicRoughnessTexture;    ///< A texture id (green roughness, blue metal), or empty.
	std::string normalTexture;               ///< A texture id, or empty.
	std::string emissiveTexture;             ///< A texture id, or empty.
};

/// @brief A node: a transform in its parent's space, and what is there.
struct Node {
	std::string id;             ///< Unique in the scene.
	std::string name;           ///< Shown to people.
	std::string parent;         ///< The parent's id, or empty at the root.
	Transform transform;        ///< Relative to the parent.
	std::string mesh;           ///< A mesh id in the Assets store, or empty.
	std::string material;       ///< A material id, or empty for the default material.
	std::optional<Light> light; ///< A light here, if any.
	std::optional<Lens> camera; ///< A camera here, if any.
	bool visible = true;        ///< Hidden nodes and their children aren't drawn or picked.
};

/// @brief The limits a scene is checked against: what a document or a file may ask for.
struct SceneLimits {
	std::size_t nodes = 100000;    ///< Nodes at most.
	std::size_t materials = 10000; ///< Materials at most.
	std::size_t depth = 64;        ///< The deepest hierarchy.
};

/// @brief The nodes and materials.
struct Scene {
	std::vector<Node> nodes;         ///< In outliner order; parents need not come first.
	std::vector<Material> materials; ///< By id.

	/// @brief A node by id. @param id Its id. @return It, or null.
	[[nodiscard]] const Node* node(std::string_view id) const noexcept;
	/// @brief A node by id, writable. @param id Its id. @return It, or null.
	[[nodiscard]] Node* node(std::string_view id) noexcept;
	/// @brief A material by id. @param id Its id. @return It, or null.
	[[nodiscard]] const Material* material(std::string_view id) const noexcept;
	/// @brief Checks the ids are unique, parents exist, there is no cycle, and the limits hold.
	/// @param limits The limits.
	/// @return An empty string if valid, else what is wrong.
	[[nodiscard]] std::string validate(const SceneLimits& limits = {}) const;
	/// @brief Every node's matrix in world space, with whether it is visible (it and all its parents are).
	/// A node whose parents form a cycle or are missing is placed as if at the root.
	/// @return By node id.
	[[nodiscard]] std::map<std::string, std::pair<Mat4, bool>, std::less<>> worldMatrices() const;
	/// @brief A fresh id with a prefix, not used by any node or material. @param prefix E.g. "cube".
	/// @return E.g. "cube3".
	[[nodiscard]] std::string freshId(std::string_view prefix) const;
	/// @brief The ids of a node and every node under it. @param id The node. @return Them, the node first.
	[[nodiscard]] std::vector<std::string> subtree(std::string_view id) const;
};

/// @brief The heavy data of scenes, by id: meshes and textures. Shared and immutable once added.
class Assets {
public:
	/// @brief Adds or replaces a mesh. @param id Its id. @param mesh The data.
	void setMesh(const std::string& id, MeshData mesh);
	/// @brief Adds or replaces a texture. @param id Its id. @param texture The data.
	void setTexture(const std::string& id, TextureData texture);
	/// @brief A mesh. @param id Its id. @return It, or null.
	[[nodiscard]] std::shared_ptr<const MeshData> mesh(std::string_view id) const;
	/// @brief A texture. @param id Its id. @return It, or null.
	[[nodiscard]] std::shared_ptr<const TextureData> texture(std::string_view id) const;
	/// @brief Every mesh. @return By id.
	[[nodiscard]] const std::map<std::string, std::shared_ptr<const MeshData>, std::less<>>& meshes() const noexcept {
		return meshes_;
	}
	/// @brief Every texture. @return By id.
	[[nodiscard]] const std::map<std::string, std::shared_ptr<const TextureData>, std::less<>>&
	textures() const noexcept {
		return textures_;
	}
	/// @brief Takes every mesh and texture of another store, replacing those with the same ids. @param other It.
	void merge(const Assets& other);
	/// @brief Counts every change, so a GPU cache knows when to upload. @return The count.
	[[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

private:
	std::map<std::string, std::shared_ptr<const MeshData>, std::less<>> meshes_;      ///< By id.
	std::map<std::string, std::shared_ptr<const TextureData>, std::less<>> textures_; ///< By id.
	std::uint64_t revision_ = 0;                                                      ///< Counts changes.
};

/// @brief A light's or camera's world placement: its position and the way its -z points.
/// @param world The node's world matrix.
/// @return The position and direction (of length 1).
[[nodiscard]] std::pair<Vec3, Vec3> placement(const Mat4& world) noexcept;

/// @brief The world bounds of the visible meshes of some nodes (all if the list is empty).
/// @param scene The scene. @param assets Its meshes. @param ids The nodes.
/// @return The bounds; empty if there's nothing.
[[nodiscard]] Aabb worldBounds(const Scene& scene, const Assets& assets, const std::vector<std::string>& ids = {});

} // namespace affresco::scene3d
