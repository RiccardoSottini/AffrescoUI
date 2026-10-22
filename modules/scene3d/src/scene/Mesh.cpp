/// @file
/// @brief Mesh data: bounds, normals, validation, edges and ray casts (Mesh.hpp).

#include "affresco/scene3d/Mesh.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace affresco::scene3d {

void MeshData::computeBounds() noexcept {
	bounds = {};
	for(const Vec3& position : positions) bounds.extend(position);
}

void MeshData::computeNormals() {
	normals.assign(positions.size(), Vec3{});
	for(std::size_t i = 0; i + 2 < indices.size(); i += 3) {
		const std::uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
		if(a >= positions.size() || b >= positions.size() || c >= positions.size()) continue;
		// The cross product's length is twice the area: larger triangles weigh more
		const Vec3 face = cross(positions[b] - positions[a], positions[c] - positions[a]);
		normals[a] = normals[a] + face;
		normals[b] = normals[b] + face;
		normals[c] = normals[c] + face;
	}
	for(Vec3& normal : normals) normal = length(normal) > 0.0f ? normalize(normal) : Vec3{0.0f, 1.0f, 0.0f};
}

// NOLINTNEXTLINE(modernize-use-string-view): its public declaration stays as it is
std::string MeshData::validate() const {
	if(!normals.empty() && normals.size() != positions.size()) return "the normals don't match the positions";
	if(!texCoords.empty() && texCoords.size() != positions.size())
		return "the texture coordinates don't match the positions";
	if(indices.size() % 3 != 0) return "the indices aren't whole triangles";
	for(const std::uint32_t index : indices)
		if(index >= positions.size()) return "an index is out of range";
	for(const Vec3& position : positions)
		if(!isFinite(position)) return "a position isn't finite";
	for(const Vec3& normal : normals)
		if(!isFinite(normal)) return "a normal isn't finite";
	return {};
}

std::vector<std::uint32_t> MeshData::edges() const {
	std::set<std::pair<std::uint32_t, std::uint32_t>> unique;
	for(std::size_t i = 0; i + 2 < indices.size(); i += 3)
		for(std::size_t corner = 0; corner < 3; ++corner) {
			const std::uint32_t a = indices[i + corner], b = indices[i + (corner + 1) % 3];
			unique.emplace(std::min(a, b), std::max(a, b));
		}
	std::vector<std::uint32_t> result;
	result.reserve(unique.size() * 2);
	for(const auto& [a, b] : unique) {
		result.push_back(a);
		result.push_back(b);
	}
	return result;
}

std::optional<float> MeshData::raycast(const Ray& ray) const noexcept {
	if(!intersect(ray, bounds)) return std::nullopt;
	std::optional<float> nearest;
	for(std::size_t i = 0; i + 2 < indices.size(); i += 3) {
		const std::uint32_t a = indices[i], b = indices[i + 1], c = indices[i + 2];
		if(a >= positions.size() || b >= positions.size() || c >= positions.size()) continue;
		const auto hit = intersect(ray, positions[a], positions[b], positions[c]);
		if(hit && (!nearest || *hit < *nearest)) nearest = hit;
	}
	return nearest;
}

} // namespace affresco::scene3d
