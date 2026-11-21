/// @file
/// @brief Editable polygon meshes and their operations (PolyMesh.hpp).

#include "affresco/scene3d/PolyMesh.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_set>

namespace affresco::scene3d {

namespace {

using Edge = std::array<std::uint32_t, 2>; ///< An undirected edge, the lower index first.

/// @brief An undirected edge's key. @param a One end. @param b The other. @return It, the lower first.
Edge edgeKey(std::uint32_t a, std::uint32_t b) { return a < b ? Edge{a, b} : Edge{b, a}; }

/// @brief Throws a DocumentError. @param what The fault.
[[noreturn]] void fail(const std::string& what) { throw DocumentError("mesh: " + what); }

/// @brief A set of valid face indices. @param mesh The mesh. @param faces The indices. @return The valid ones.
std::set<std::uint32_t> faceSet(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces) {
	std::set<std::uint32_t> result;
	for(const std::uint32_t face : faces)
		if(face < mesh.faces.size()) result.insert(face);
	return result;
}

/// @brief Removes the vertices no face uses, renumbering the faces; the others keep their order.
/// @param mesh The mesh. @return It, compacted.
PolyMesh compact(PolyMesh mesh) {
	std::vector<std::int64_t> remap(mesh.vertices.size(), -1);
	for(const auto& face : mesh.faces)
		for(const std::uint32_t index : face) remap[index] = 0;
	std::vector<Vec3> kept;
	for(std::size_t i = 0; i < remap.size(); ++i)
		if(remap[i] == 0) {
			remap[i] = static_cast<std::int64_t>(kept.size());
			kept.push_back(mesh.vertices[i]);
		}
	for(auto& face : mesh.faces)
		for(std::uint32_t& index : face) index = static_cast<std::uint32_t>(remap[index]);
	mesh.vertices = std::move(kept);
	return mesh;
}

/// @brief The weights of a smooth falloff around some vertices. @param mesh The mesh. @param vertices They (weight
/// 1). @param radius 0 for them alone. @return One weight per vertex, 0 to 1.
std::vector<float> falloff(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices, float radius) {
	std::vector<float> weights(mesh.vertices.size(), 0.0f);
	for(const std::uint32_t v : vertices)
		if(v < weights.size()) weights[v] = 1.0f;
	if(!(radius > 0.0f)) return weights;
	for(std::size_t i = 0; i < weights.size(); ++i) {
		if(weights[i] == 1.0f) continue;
		float nearest = radius;
		for(const std::uint32_t v : vertices)
			if(v < mesh.vertices.size()) nearest = std::min(nearest, length(mesh.vertices[i] - mesh.vertices[v]));
		const float t = 1.0f - nearest / radius;
		weights[i] = t * t * (3.0f - 2.0f * t);
	}
	return weights;
}

/// @brief Distance from a point to a segment. @param p The point. @param a One end. @param b The other. @return It.
float segmentDistance(Vec3 p, Vec3 a, Vec3 b) {
	const Vec3 ab = b - a;
	const float span = dot(ab, ab);
	const float t = span > 0.0f ? std::clamp(dot(p - a, ab) / span, 0.0f, 1.0f) : 0.0f;
	return length(p - (a + ab * t));
}

/// @brief Whether a point lies in a triangle, near its plane. @param p The point. @param a,b,c The corners.
/// @param normal The face's normal. @return Whether it does.
bool inTriangle(Vec3 p, Vec3 a, Vec3 b, Vec3 c, Vec3 normal) {
	const auto side = [&](Vec3 from, Vec3 to) { return dot(cross(to - from, p - from), normal) >= -1e-5f; };
	return side(a, b) && side(b, c) && side(c, a);
}

/// @brief A unit vector, or zero. @param v It. @return Normalised.
Vec3 unit(Vec3 v) {
	const float l = length(v);
	return l > 1e-12f ? v / l : Vec3{};
}

} // namespace

std::vector<std::array<std::uint32_t, 2>> PolyMesh::edges() const {
	std::set<Edge> unique;
	for(const auto& face : faces)
		for(std::size_t i = 0; i < face.size(); ++i) unique.insert(edgeKey(face[i], face[(i + 1) % face.size()]));
	return {unique.begin(), unique.end()};
}

PolyMesh polyMeshFromJson(const json& value, const PolyLimits& limits) {
	if(!value.is_object()) fail("expected {vertices, faces}");
	PolyMesh mesh;
	const auto vertices = value.find("vertices");
	const auto faces = value.find("faces");
	if(vertices == value.end() || !vertices->is_array()) fail("vertices: expected a list of [x, y, z]");
	if(faces == value.end() || !faces->is_array()) fail("faces: expected a list of vertex lists");
	if(vertices->size() > limits.vertices || faces->size() > limits.faces) fail("too large");
	for(const json& v : *vertices) {
		if(!v.is_array() || v.size() != 3) fail("vertices: expected [x, y, z]");
		Vec3 point;
		for(int axis = 0; axis < 3; ++axis) {
			const json& coordinate = v[static_cast<std::size_t>(axis)];
			if(!coordinate.is_number() || !std::isfinite(coordinate.get<double>())) fail("vertices: expected numbers");
			point[axis] = coordinate.get<float>();
		}
		mesh.vertices.push_back(point);
	}
	for(const json& f : *faces) {
		if(!f.is_array() || f.size() < 3 || f.size() > limits.sides) fail("faces: expected 3 or more indices");
		std::vector<std::uint32_t> face;
		for(const json& index : f) {
			if(!index.is_number_integer() || index.get<std::int64_t>() < 0 ||
			   index.get<std::uint64_t>() >= mesh.vertices.size())
				fail("faces: an index names no vertex");
			face.push_back(index.get<std::uint32_t>());
		}
		mesh.faces.push_back(std::move(face));
	}
	if(const auto smooth = value.find("smooth"); smooth != value.end() && smooth->is_boolean())
		mesh.smooth = smooth->get<bool>();
	return mesh;
}

json toJson(const PolyMesh& mesh) {
	const auto round = [](float x) { return std::round(static_cast<double>(x) * 1e5) / 1e5; };
	json vertices = json::array();
	for(const Vec3 v : mesh.vertices) vertices.push_back(json::array({round(v.x), round(v.y), round(v.z)}));
	json faces = json::array();
	for(const auto& face : mesh.faces) faces.push_back(face);
	json result{{"vertices", std::move(vertices)}, {"faces", std::move(faces)}};
	if(mesh.smooth) result["smooth"] = true;
	return result;
}

Vec3 faceCentre(const PolyMesh& mesh, std::size_t face) {
	Vec3 sum;
	for(const std::uint32_t index : mesh.faces[face]) sum = sum + mesh.vertices[index];
	return sum / static_cast<float>(mesh.faces[face].size());
}

Vec3 faceNormal(const PolyMesh& mesh, std::size_t face) {
	Vec3 n;
	const auto& f = mesh.faces[face];
	for(std::size_t i = 0; i < f.size(); ++i) {
		const Vec3 a = mesh.vertices[f[i]], b = mesh.vertices[f[(i + 1) % f.size()]];
		n = n + Vec3{(a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)};
	}
	return unit(n);
}

MeshData triangulate(const PolyMesh& mesh) {
	MeshData data;
	if(mesh.smooth) data.positions = mesh.vertices;
	for(std::size_t f = 0; f < mesh.faces.size(); ++f) {
		const auto& face = mesh.faces[f];
		std::vector<std::uint32_t> corners = face;
		if(!mesh.smooth) { // its own vertices, with its normal
			const Vec3 normal = faceNormal(mesh, f);
			for(std::uint32_t& corner : corners) {
				data.positions.push_back(mesh.vertices[corner]);
				data.normals.push_back(normal);
				corner = static_cast<std::uint32_t>(data.positions.size() - 1);
			}
		}
		for(std::size_t i = 1; i + 1 < corners.size(); ++i)
			data.indices.insert(data.indices.end(), {corners[0], corners[i], corners[i + 1]});
		for(std::size_t i = 0; i < corners.size(); ++i) // the polygon's own edges, not the triangles'
			data.lines.insert(data.lines.end(), {corners[i], corners[(i + 1) % corners.size()]});
	}
	if(mesh.smooth) data.computeNormals();
	data.computeBounds();
	return data;
}

PolyMesh polyPrimitive(Primitive primitive) {
	PolyMesh mesh;
	if(primitive == Primitive::Cube) {
		for(int i = 0; i < 8; ++i)
			mesh.vertices.push_back({(i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f});
		mesh.faces = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
		return mesh;
	}
	if(primitive == Primitive::Plane) {
		mesh.vertices = {{-1, 0, 1}, {1, 0, 1}, {1, 0, -1}, {-1, 0, -1}};
		mesh.faces = {{0, 1, 2, 3}};
		return mesh;
	}
	return polyFromMesh(makePrimitive(primitive));
}

PolyMesh polyFromMesh(const MeshData& data, const PolyLimits& limits) {
	PolyMesh mesh;
	std::map<std::array<long, 3>, std::uint32_t> welded; // seams and poles share their points
	std::vector<std::uint32_t> remap;
	for(const Vec3 p : data.positions) {
		const std::array<long, 3> key{std::lround(p.x * 1e4f), std::lround(p.y * 1e4f), std::lround(p.z * 1e4f)};
		const auto [at, added] = welded.emplace(key, static_cast<std::uint32_t>(mesh.vertices.size()));
		if(added) mesh.vertices.push_back(p);
		remap.push_back(at->second);
	}
	if(mesh.vertices.size() > limits.vertices || data.indices.size() / 3 > limits.faces) fail("too large");
	for(std::size_t t = 0; t + 2 < data.indices.size(); t += 3) {
		if(data.indices[t] >= remap.size() || data.indices[t + 1] >= remap.size() ||
		   data.indices[t + 2] >= remap.size())
			continue;
		const std::uint32_t a = remap[data.indices[t]], b = remap[data.indices[t + 1]], c = remap[data.indices[t + 2]];
		if(a != b && b != c && a != c) mesh.faces.push_back({a, b, c});
	}
	mesh.smooth = true;
	return mesh;
}

std::vector<std::uint32_t> faceVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces) {
	std::vector<std::uint32_t> result;
	std::unordered_set<std::uint32_t> seen;
	for(const std::uint32_t face : faceSet(mesh, faces))
		for(const std::uint32_t v : mesh.faces[face])
			if(seen.insert(v).second) result.push_back(v);
	return result;
}

PolyMesh extrudeFaces(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces, float distance) {
	const std::set<std::uint32_t> region = faceSet(mesh, faces);
	if(region.empty()) return mesh;
	PolyMesh result = mesh;
	std::map<Edge, int> uses; // how many of the region's faces hold each edge
	std::map<std::uint32_t, Vec3> normals;
	for(const std::uint32_t f : region) {
		const auto& face = mesh.faces[f];
		const Vec3 normal = faceNormal(mesh, f);
		for(std::size_t i = 0; i < face.size(); ++i) {
			++uses[edgeKey(face[i], face[(i + 1) % face.size()])];
			normals[face[i]] = normals[face[i]] + normal;
		}
	}
	std::map<std::uint32_t, std::uint32_t> moved; // each vertex of the region, to its copy
	for(const auto& [v, normal] : normals) {
		moved[v] = static_cast<std::uint32_t>(result.vertices.size());
		result.vertices.push_back(mesh.vertices[v] + unit(normal) * distance);
	}
	for(const std::uint32_t f : region) {
		const auto& face = mesh.faces[f];
		for(std::size_t i = 0; i < face.size(); ++i) {
			const std::uint32_t a = face[i], b = face[(i + 1) % face.size()];
			if(uses[edgeKey(a, b)] == 1) result.faces.push_back({a, b, moved[b], moved[a]}); // a wall
		}
		for(std::uint32_t& v : result.faces[f]) v = moved[v];
	}
	return compact(std::move(result));
}

PolyMesh insetFaces(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces, float amount) {
	const float t = std::clamp(amount, 0.0f, 0.99f);
	PolyMesh result = mesh;
	for(const std::uint32_t f : faceSet(mesh, faces)) {
		const auto& face = mesh.faces[f];
		const Vec3 centre = faceCentre(mesh, f);
		std::vector<std::uint32_t> inner;
		for(const std::uint32_t v : face) {
			inner.push_back(static_cast<std::uint32_t>(result.vertices.size()));
			result.vertices.push_back(lerp(mesh.vertices[v], centre, t));
		}
		for(std::size_t i = 0; i < face.size(); ++i) {
			const std::size_t j = (i + 1) % face.size();
			result.faces.push_back({face[i], face[j], inner[j], inner[i]});
		}
		result.faces[f] = inner;
	}
	return result;
}

PolyMesh deleteFaces(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces) {
	const std::set<std::uint32_t> gone = faceSet(mesh, faces);
	PolyMesh result = mesh;
	result.faces.clear();
	for(std::size_t f = 0; f < mesh.faces.size(); ++f)
		if(!gone.contains(static_cast<std::uint32_t>(f))) result.faces.push_back(mesh.faces[f]);
	return compact(std::move(result));
}

PolyMesh deleteVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices) {
	const std::unordered_set<std::uint32_t> gone(vertices.begin(), vertices.end());
	PolyMesh result = mesh;
	result.faces.clear();
	for(const auto& face : mesh.faces)
		if(std::ranges::none_of(face, [&](std::uint32_t v) { return gone.contains(v); })) result.faces.push_back(face);
	return compact(std::move(result));
}

PolyMesh loopCut(const PolyMesh& mesh, std::uint32_t a, std::uint32_t b) {
	std::map<Edge, std::vector<std::uint32_t>> owners; // each edge, to the faces holding it
	for(std::size_t f = 0; f < mesh.faces.size(); ++f) {
		const auto& face = mesh.faces[f];
		for(std::size_t i = 0; i < face.size(); ++i)
			owners[edgeKey(face[i], face[(i + 1) % face.size()])].push_back(static_cast<std::uint32_t>(f));
	}
	std::map<std::uint32_t, std::size_t> cuts; // each quad of the ring, to the position of its cut edge
	const auto positionOf = [&](std::uint32_t f, Edge e) -> std::size_t {
		const auto& face = mesh.faces[f];
		for(std::size_t i = 0; i < 4; ++i)
			if(edgeKey(face[i], face[(i + 1) % 4]) == e) return i;
		return 0;
	};
	const auto walk = [&](Edge edge) {
		for(;;) {
			const auto found = owners.find(edge);
			if(found == owners.end()) return;
			std::optional<std::uint32_t> next;
			for(const std::uint32_t f : found->second)
				if(mesh.faces[f].size() == 4 && !cuts.contains(f)) next = f;
			if(!next) return;
			const std::size_t at = positionOf(*next, edge);
			cuts[*next] = at;
			const auto& quad = mesh.faces[*next];
			edge = edgeKey(quad[(at + 2) % 4], quad[(at + 3) % 4]);
		}
	};
	const Edge start = edgeKey(a, b);
	walk(start);
	walk(start); // the other way round, from the start edge's other quad
	if(cuts.empty()) return mesh;
	PolyMesh result = mesh;
	std::map<Edge, std::uint32_t> middles;
	const auto middle = [&](std::uint32_t p, std::uint32_t q) {
		const Edge key = edgeKey(p, q);
		const auto [at, added] = middles.emplace(key, static_cast<std::uint32_t>(result.vertices.size()));
		if(added) result.vertices.push_back(lerp(mesh.vertices[p], mesh.vertices[q], 0.5f));
		return at->second;
	};
	for(const auto& [f, at] : cuts) {
		const auto quad = mesh.faces[f];
		const std::uint32_t q0 = quad[at], q1 = quad[(at + 1) % 4], q2 = quad[(at + 2) % 4], q3 = quad[(at + 3) % 4];
		const std::uint32_t m1 = middle(q0, q1), m2 = middle(q2, q3);
		result.faces[f] = {q0, m1, m2, q3};
		result.faces.push_back({m1, q1, q2, m2});
	}
	for(std::size_t f = 0; f < mesh.faces.size(); ++f) { // other faces on a cut edge take its middle too
		if(cuts.contains(static_cast<std::uint32_t>(f))) continue;
		const auto& face = mesh.faces[f];
		std::vector<std::uint32_t> with;
		for(std::size_t i = 0; i < face.size(); ++i) {
			with.push_back(face[i]);
			if(const auto found = middles.find(edgeKey(face[i], face[(i + 1) % face.size()])); found != middles.end())
				with.push_back(found->second);
		}
		result.faces[f] = std::move(with);
	}
	return result;
}

PolyMesh moveVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices, Vec3 delta, float radius) {
	const std::vector<float> weights = falloff(mesh, vertices, radius);
	PolyMesh result = mesh;
	for(std::size_t i = 0; i < weights.size(); ++i) result.vertices[i] = result.vertices[i] + delta * weights[i];
	return result;
}

PolyMesh scaleVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices, float factor, float radius) {
	Vec3 centre;
	std::size_t count = 0;
	for(const std::uint32_t v : vertices)
		if(v < mesh.vertices.size()) {
			centre = centre + mesh.vertices[v];
			++count;
		}
	if(count == 0) return mesh;
	centre = centre / static_cast<float>(count);
	const std::vector<float> weights = falloff(mesh, vertices, radius);
	PolyMesh result = mesh;
	for(std::size_t i = 0; i < weights.size(); ++i) {
		const float s = 1.0f + (factor - 1.0f) * weights[i];
		result.vertices[i] = centre + (result.vertices[i] - centre) * s;
	}
	return result;
}

PolyMesh rotateVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices, Quat rotation, float radius) {
	Vec3 centre;
	std::size_t count = 0;
	for(const std::uint32_t v : vertices)
		if(v < mesh.vertices.size()) {
			centre = centre + mesh.vertices[v];
			++count;
		}
	if(count == 0) return mesh;
	centre = centre / static_cast<float>(count);
	const std::vector<float> weights = falloff(mesh, vertices, radius);
	PolyMesh result = mesh;
	for(std::size_t i = 0; i < weights.size(); ++i) {
		if(weights[i] <= 0.0f) continue;
		const Quat turn = weights[i] >= 1.0f ? rotation : slerp(Quat{}, rotation, weights[i]);
		result.vertices[i] = centre + turn.rotate(result.vertices[i] - centre);
	}
	return result;
}

PolyMesh flipFaces(const PolyMesh& mesh) {
	PolyMesh result = mesh;
	for(auto& face : result.faces) std::reverse(face.begin(), face.end());
	return result;
}

PolyMesh edgeBars(const PolyMesh& mesh, const std::vector<std::array<std::uint32_t, 2>>& edges, float thickness,
                  const PolyLimits& limits) {
	PolyMesh bars;
	const float half = std::max(thickness, 1e-5f) * 0.5f;
	for(const auto& edge : edges.empty() ? mesh.edges() : edges) {
		if(edge[0] >= mesh.vertices.size() || edge[1] >= mesh.vertices.size()) continue;
		if(bars.vertices.size() + 8 > limits.vertices || bars.faces.size() + 6 > limits.faces) break;
		const Vec3 a = mesh.vertices[edge[0]], b = mesh.vertices[edge[1]];
		const Vec3 along = unit(b - a);
		if(along == Vec3{}) continue;
		const Vec3 side =
		    unit(cross(along, std::abs(along.y) < 0.9f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f}));
		const Vec3 up = cross(side, along);
		const auto base = static_cast<std::uint32_t>(bars.vertices.size());
		for(const Vec3 end : {a, b})
			for(const auto& [s, u] : {std::pair{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}})
				bars.vertices.push_back(end + side * (s * half) + up * (u * half));
		const auto at = [base](std::uint32_t i) { return base + i; };
		bars.faces.push_back({at(0), at(1), at(2), at(3)}); // the end at a
		bars.faces.push_back({at(4), at(7), at(6), at(5)}); // the end at b
		for(std::uint32_t i = 0; i < 4; ++i) {
			const std::uint32_t j = (i + 1) % 4;
			bars.faces.push_back({at(i), at(i + 4), at(j + 4), at(j)});
		}
	}
	return bars;
}

PolyMesh mirrorMesh(const PolyMesh& mesh, int axis) {
	axis = std::clamp(axis, 0, 2);
	PolyMesh result = mesh;
	std::vector<std::uint32_t> image(mesh.vertices.size());
	for(std::size_t i = 0; i < mesh.vertices.size(); ++i) {
		Vec3 p = mesh.vertices[i];
		if(std::abs(p[axis]) < 1e-4f) {
			image[i] = static_cast<std::uint32_t>(i); // on the plane: shared
			continue;
		}
		p[axis] = -p[axis];
		image[i] = static_cast<std::uint32_t>(result.vertices.size());
		result.vertices.push_back(p);
	}
	for(const auto& face : mesh.faces) {
		if(std::ranges::all_of(face, [&](std::uint32_t v) { return image[v] == v; })) continue; // on the plane
		std::vector<std::uint32_t> mirrored;
		for(auto v = face.rbegin(); v != face.rend(); ++v) mirrored.push_back(image[*v]);
		result.faces.push_back(std::move(mirrored));
	}
	return result;
}

PolyMesh arrayMesh(const PolyMesh& mesh, int count, Vec3 offset) {
	count = std::clamp(count, 1, 64);
	PolyMesh result = mesh;
	for(int copy = 1; copy < count; ++copy) {
		const auto base = static_cast<std::uint32_t>(result.vertices.size());
		for(const Vec3 v : mesh.vertices) result.vertices.push_back(v + offset * static_cast<float>(copy));
		for(const auto& face : mesh.faces) {
			std::vector<std::uint32_t> moved = face;
			for(std::uint32_t& v : moved) v += base;
			result.faces.push_back(std::move(moved));
		}
	}
	return result;
}

PolyMesh subdivide(const PolyMesh& mesh, int levels, const PolyLimits& limits) {
	PolyMesh current = mesh;
	for(int level = 0; level < std::clamp(levels, 0, 4); ++level) {
		std::size_t corners = 0;
		for(const auto& face : current.faces) corners += face.size();
		const std::vector<Edge> edges = current.edges();
		if(corners > limits.faces || current.vertices.size() + edges.size() + current.faces.size() > limits.vertices)
			break;
		const auto vertexCount = static_cast<std::uint32_t>(current.vertices.size());
		std::map<Edge, std::vector<std::uint32_t>> owners;
		for(std::size_t f = 0; f < current.faces.size(); ++f) {
			const auto& face = current.faces[f];
			for(std::size_t i = 0; i < face.size(); ++i)
				owners[edgeKey(face[i], face[(i + 1) % face.size()])].push_back(static_cast<std::uint32_t>(f));
		}
		std::vector<Vec3> facePoints;
		for(std::size_t f = 0; f < current.faces.size(); ++f) facePoints.push_back(faceCentre(current, f));
		PolyMesh next;
		next.smooth = current.smooth;
		next.vertices.resize(vertexCount);
		std::map<Edge, std::uint32_t> edgeIndex;
		std::vector<Vec3> edgeSums(vertexCount), boundarySums(vertexCount), faceSums(vertexCount);
		std::vector<int> valence(vertexCount, 0), boundary(vertexCount, 0), faceCounts(vertexCount, 0);
		for(const Edge& e : edges) {
			const auto& by = owners[e];
			const Vec3 mid = lerp(current.vertices[e[0]], current.vertices[e[1]], 0.5f);
			Vec3 point = mid;
			if(by.size() == 2)
				point =
				    (current.vertices[e[0]] + current.vertices[e[1]] + facePoints[by[0]] + facePoints[by[1]]) * 0.25f;
			edgeIndex[e] = static_cast<std::uint32_t>(next.vertices.size());
			next.vertices.push_back(point);
			for(const std::uint32_t v : e) {
				edgeSums[v] = edgeSums[v] + mid;
				++valence[v];
				if(by.size() != 2) {
					boundarySums[v] = boundarySums[v] + mid;
					++boundary[v];
				}
			}
		}
		for(std::size_t f = 0; f < current.faces.size(); ++f)
			for(const std::uint32_t v : current.faces[f]) {
				faceSums[v] = faceSums[v] + facePoints[f];
				++faceCounts[v];
			}
		for(std::uint32_t v = 0; v < vertexCount; ++v) {
			const Vec3 p = current.vertices[v];
			if(boundary[v] >= 2)
				next.vertices[v] = p * 0.5f + boundarySums[v] * (0.5f / static_cast<float>(boundary[v]));
			else if(valence[v] >= 3 && faceCounts[v] > 0) {
				const auto n = static_cast<float>(valence[v]);
				const Vec3 f = faceSums[v] / static_cast<float>(faceCounts[v]), r = edgeSums[v] / n;
				next.vertices[v] = (f + r * 2.0f + p * (n - 3.0f)) / n;
			} else next.vertices[v] = p;
		}
		for(std::size_t f = 0; f < current.faces.size(); ++f) {
			const auto& face = current.faces[f];
			const auto centre = static_cast<std::uint32_t>(next.vertices.size());
			next.vertices.push_back(facePoints[f]);
			for(std::size_t i = 0; i < face.size(); ++i) {
				const std::uint32_t before = face[(i + face.size() - 1) % face.size()];
				const std::uint32_t after = face[(i + 1) % face.size()];
				next.faces.push_back(
				    {face[i], edgeIndex[edgeKey(face[i], after)], centre, edgeIndex[edgeKey(before, face[i])]});
			}
		}
		current = std::move(next);
	}
	return current;
}

std::optional<std::uint32_t> nearestVertex(const PolyMesh& mesh, Vec3 point) {
	std::optional<std::uint32_t> best;
	float distance = 0.0f;
	for(std::size_t i = 0; i < mesh.vertices.size(); ++i) {
		const float d = length(mesh.vertices[i] - point);
		if(!best || d < distance) {
			best = static_cast<std::uint32_t>(i);
			distance = d;
		}
	}
	return best;
}

std::optional<std::array<std::uint32_t, 2>> nearestEdge(const PolyMesh& mesh, Vec3 point) {
	std::optional<std::array<std::uint32_t, 2>> best;
	float distance = 0.0f;
	for(const Edge& e : mesh.edges()) {
		const float d = segmentDistance(point, mesh.vertices[e[0]], mesh.vertices[e[1]]);
		if(!best || d < distance) {
			best = e;
			distance = d;
		}
	}
	return best;
}

std::optional<std::uint32_t> nearestFace(const PolyMesh& mesh, Vec3 point) {
	std::optional<std::uint32_t> best;
	float score = 0.0f;
	for(std::size_t f = 0; f < mesh.faces.size(); ++f) {
		const auto& face = mesh.faces[f];
		const Vec3 normal = faceNormal(mesh, f), centre = faceCentre(mesh, f);
		const float plane = std::abs(dot(point - centre, normal));
		bool inside = false;
		for(std::size_t i = 1; i + 1 < face.size() && !inside; ++i)
			inside =
			    inTriangle(point, mesh.vertices[face[0]], mesh.vertices[face[i]], mesh.vertices[face[i + 1]], normal);
		const float s = plane * 4.0f + (inside ? 0.0f : 1e3f + length(point - centre));
		if(!best || s < score) {
			best = static_cast<std::uint32_t>(f);
			score = s;
		}
	}
	return best;
}

} // namespace affresco::scene3d
