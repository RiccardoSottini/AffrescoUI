/// @file
/// @brief Scenes and their asset store (Scene.hpp).

#include "affresco/scene3d/Scene.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace affresco::scene3d {

namespace {

/// @brief Each node's parent, by the node's id.
using Parents = std::map<std::string_view, std::string_view>;

/// @brief Checks every node's id and transform, noting its parent.
/// @param nodes The nodes. @param parents Receives each node's parent.
/// @return The first problem, or empty.
[[nodiscard]] std::string checkNodes(const std::vector<Node>& nodes, Parents& parents) {
	for(const Node& node : nodes) {
		if(node.id.empty()) return "a node has no id";
		if(!parents.emplace(node.id, node.parent).second) return "two nodes have the id " + node.id;
		if(!isFinite(node.transform.translation) || !isFinite(node.transform.scale))
			return "the node " + node.id + " has a transform that isn't finite";
	}
	return {};
}

/// @brief Checks that the materials' ids are there and unique. @param materials The materials.
/// @return The first problem, or empty.
[[nodiscard]] std::string checkMaterials(const std::vector<Material>& materials) {
	std::set<std::string_view> materialIds;
	for(const Material& material : materials)
		if(material.id.empty() || !materialIds.insert(material.id).second)
			return "a material id is empty or repeated: " + material.id;
	return {};
}

/// @brief Checks that a node's ancestors are there, end, and are not too many.
/// @param node The node. @param parents Each node's parent. @param depth The most ancestors.
/// @return The problem, or empty.
[[nodiscard]] std::string checkAncestry(const Node& node, const Parents& parents, std::size_t depth) {
	std::size_t up = 0;
	for(std::string_view at = node.parent; !at.empty(); ++up) {
		const auto parent = parents.find(at);
		if(parent == parents.end()) return "the node " + node.id + " has a missing parent";
		if(up >= depth) return "the node " + node.id + " is too deep or in a cycle";
		at = parent->second;
	}
	return {};
}

} // namespace

const Node* Scene::node(std::string_view id) const noexcept {
	const auto found = std::ranges::find(nodes, id, &Node::id);
	return found == nodes.end() ? nullptr : &*found;
}

Node* Scene::node(std::string_view id) noexcept {
	const auto found = std::ranges::find(nodes, id, &Node::id);
	return found == nodes.end() ? nullptr : &*found;
}

const Material* Scene::material(std::string_view id) const noexcept {
	const auto found = std::ranges::find(materials, id, &Material::id);
	return found == materials.end() ? nullptr : &*found;
}

std::string Scene::validate(const SceneLimits& limits) const {
	if(nodes.size() > limits.nodes) return "too many nodes";
	if(materials.size() > limits.materials) return "too many materials";
	Parents parents;
	if(std::string problem = checkNodes(nodes, parents); !problem.empty()) return problem;
	if(std::string problem = checkMaterials(materials); !problem.empty()) return problem;
	for(const Node& node : nodes)
		if(std::string problem = checkAncestry(node, parents, limits.depth); !problem.empty()) return problem;
	return {};
}

std::map<std::string, std::pair<Mat4, bool>, std::less<>> Scene::worldMatrices() const {
	std::map<std::string, std::pair<Mat4, bool>, std::less<>> world;
	std::set<std::string_view> visiting;
	// Depth first from each node to its root, memoised; a cycle or missing parent ends at the root
	auto resolve = [&](auto& self, const Node& node) -> std::pair<Mat4, bool> {
		if(const auto done = world.find(node.id); done != world.end()) return done->second;
		std::pair<Mat4, bool> result{node.transform.matrix(), node.visible};
		const Node* parent = node.parent.empty() ? nullptr : this->node(node.parent);
		if(parent && visiting.insert(node.id).second) {
			const auto [parentMatrix, parentVisible] = self(self, *parent);
			result = {parentMatrix * result.first, parentVisible && node.visible};
			visiting.erase(node.id);
		}
		world.emplace(node.id, result);
		return result;
	};
	for(const Node& node : nodes) resolve(resolve, node);
	return world;
}

std::string Scene::freshId(std::string_view prefix) const {
	for(std::size_t number = 1;; ++number) {
		std::string id = std::string(prefix) + std::to_string(number);
		if(!node(id) && !material(id)) return id;
	}
}

std::vector<std::string> Scene::subtree(std::string_view id) const {
	std::vector<std::string> result;
	if(!node(id)) return result;
	result.emplace_back(id);
	// Breadth first; the visited set stops cycles
	std::set<std::string, std::less<>> seen{result.front()};
	for(std::size_t next = 0; next < result.size(); ++next)
		for(const Node& child : nodes)
			if(child.parent == result[next] && seen.insert(child.id).second) result.push_back(child.id);
	return result;
}

void Assets::setMesh(const std::string& id, MeshData mesh) {
	meshes_[id] = std::make_shared<const MeshData>(std::move(mesh));
	++revision_;
}

void Assets::setTexture(const std::string& id, TextureData texture) {
	textures_[id] = std::make_shared<const TextureData>(std::move(texture));
	++revision_;
}

std::shared_ptr<const MeshData> Assets::mesh(std::string_view id) const {
	const auto found = meshes_.find(id);
	return found == meshes_.end() ? nullptr : found->second;
}

std::shared_ptr<const TextureData> Assets::texture(std::string_view id) const {
	const auto found = textures_.find(id);
	return found == textures_.end() ? nullptr : found->second;
}

void Assets::merge(const Assets& other) {
	for(const auto& [id, mesh] : other.meshes_) meshes_[id] = mesh;
	for(const auto& [id, texture] : other.textures_) textures_[id] = texture;
	++revision_;
}

std::pair<Vec3, Vec3> placement(const Mat4& world) noexcept {
	return {world.origin(), normalize(world.transformDirection({0.0f, 0.0f, -1.0f}))};
}

Aabb worldBounds(const Scene& scene, const Assets& assets, const std::vector<std::string>& ids) {
	const auto world = scene.worldMatrices();
	Aabb bounds;
	for(const Node& node : scene.nodes) {
		if(!ids.empty() && std::ranges::find(ids, node.id) == ids.end()) continue;
		const auto& [matrix, visible] = world.at(node.id);
		if(!visible) continue;
		if(const auto mesh = assets.mesh(node.mesh)) bounds.extend(mesh->bounds.transformed(matrix));
		else if(!ids.empty()) bounds.extend(matrix.origin());
	}
	return bounds;
}

} // namespace affresco::scene3d
