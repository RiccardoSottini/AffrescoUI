/// @file
/// @brief Picking on the CPU (Picking.hpp).

#include "affresco/scene3d/Picking.hpp"

#include "affresco/scene3d/Mesh.hpp"

#include <algorithm>
#include <limits>

namespace affresco::scene3d {

namespace {

/// @brief Where a ray first meets a placed mesh, unless its bounds are already farther than a bound.
/// @param mesh The mesh. @param world Where it is placed. @param ray The ray, in the world.
/// @param bound The nearest hit so far. @return The distance along the ray, or nothing.
[[nodiscard]] std::optional<float> meshHit(const MeshData& mesh, const Mat4& world, const Ray& ray, float bound) {
	const auto box = intersect(ray, mesh.bounds.transformed(world));
	if(!box || *box > bound) return std::nullopt;
	const auto inverse = world.inverse();
	if(!inverse) return std::nullopt;
	// The ray goes into the mesh's space unnormalised, so the distance found there is the world distance.
	const Ray local{inverse->transformPoint(ray.origin), inverse->transformDirection(ray.direction)};
	return mesh.raycast(local);
}

} // namespace

std::optional<PickHit> pickNode(const Scene& scene, const Assets& assets, const Ray& ray) {
	std::optional<PickHit> best;
	for(const auto& [id, placed] : scene.worldMatrices()) {
		const auto& [world, visible] = placed;
		const Node* node = scene.node(id);
		const auto mesh = visible && node != nullptr && !node->mesh.empty() ? assets.mesh(node->mesh) : nullptr;
		if(!mesh) continue;
		const auto hit = meshHit(*mesh, world, ray, best ? best->distance : std::numeric_limits<float>::infinity());
		if(!hit || (best && *hit >= best->distance)) continue;
		best = PickHit{id, *hit, ray.at(*hit)};
	}
	return best;
}

std::vector<std::string> boxSelect(const Scene& scene, const Assets& assets, const OrbitCamera& camera, float width,
                                   float height, float x0, float y0, float x1, float y1) {
	const float left = std::min(x0, x1), right = std::max(x0, x1);
	const float top = std::min(y0, y1), bottom = std::max(y0, y1);
	const auto worlds = scene.worldMatrices();
	std::vector<std::string> ids;
	for(const auto& node : scene.nodes) {
		const auto placed = worlds.find(node.id);
		const auto mesh = node.mesh.empty() ? nullptr : assets.mesh(node.mesh);
		if(!mesh || placed == worlds.end() || !placed->second.second) continue;
		const auto at = camera.project(mesh->bounds.transformed(placed->second.first).center(), width, height);
		if(at && at->first >= left && at->first <= right && at->second >= top && at->second <= bottom)
			ids.push_back(node.id);
	}
	return ids;
}

} // namespace affresco::scene3d
