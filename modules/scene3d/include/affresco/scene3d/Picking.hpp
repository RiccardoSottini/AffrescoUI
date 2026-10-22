#pragma once

/// @file
/// @brief Picking on the CPU: the node under a ray (bounds first, then triangles) and the nodes inside a screen box.
/// The renderer's id pass answers single clicks on the GPU; this answers handles, tests and headless runs.

#include "affresco/scene3d/Camera.hpp"
#include "affresco/scene3d/Scene.hpp"

#include <optional>
#include <string>
#include <vector>

namespace affresco::scene3d {

/// @brief What a ray hit.
struct PickHit {
	std::string node;      ///< The node's id.
	float distance = 0.0f; ///< Along the ray.
	Vec3 point;            ///< Where, in the world.
};

/// @brief The nearest visible mesh node a ray hits.
/// @param scene The scene. @param assets Its meshes. @param ray The ray, in the world.
/// @return The hit, or nothing.
[[nodiscard]] std::optional<PickHit> pickNode(const Scene& scene, const Assets& assets, const Ray& ray);

/// @brief The visible mesh nodes whose projected bounds' centre lies in a screen rectangle.
/// @param scene The scene. @param assets Its meshes. @param camera The view.
/// @param width The viewport's width. @param height The viewport's height.
/// @param x0 One corner, from the left. @param y0 It, from the top. @param x1 The opposite corner. @param y1 It.
/// @return Their ids in outliner order.
[[nodiscard]] std::vector<std::string> boxSelect(const Scene& scene, const Assets& assets, const OrbitCamera& camera,
                                                 float width, float height, float x0, float y0, float x1, float y1);

} // namespace affresco::scene3d
