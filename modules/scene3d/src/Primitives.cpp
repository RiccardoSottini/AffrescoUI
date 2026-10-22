/// @file
/// @brief The generated primitives: cube, sphere, cylinder, torus and plane (Mesh.hpp).

#include "affresco/scene3d/Mesh.hpp"

#include <algorithm>
#include <array>
#include <numbers>

namespace affresco::scene3d {

namespace {

constexpr float pi = std::numbers::pi_v<float>;

/// @brief Appends a vertex. @param mesh The mesh. @param position Where. @param normal Facing. @param uv Its texture
/// coordinate.
void addVertex(MeshData& mesh, Vec3 position, Vec3 normal, TexCoord uv) {
	mesh.positions.push_back(position);
	mesh.normals.push_back(normal);
	mesh.texCoords.push_back(uv);
}

/// @brief Appends two triangles of a quad, counter-clockwise seen from the front.
/// @param mesh The mesh. @param a Corner. @param b Next. @param c Next. @param d Last.
void addQuad(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
	mesh.indices.insert(mesh.indices.end(), {a, b, c, a, c, d});
}

/// @brief A grid of vertices, rows by columns, from a function of (column / columns, row / rows), and its quads.
/// @param mesh The mesh. @param columns Cells across. @param rows Cells down.
/// @param vertex Makes the position and normal at (s, t).
template <typename Vertex> void addGrid(MeshData& mesh, int columns, int rows, Vertex vertex) {
	const auto first = static_cast<std::uint32_t>(mesh.positions.size());
	for(int row = 0; row <= rows; ++row)
		for(int column = 0; column <= columns; ++column) {
			const float s = static_cast<float>(column) / static_cast<float>(columns);
			const float t = static_cast<float>(row) / static_cast<float>(rows);
			const auto [position, normal] = vertex(s, t);
			addVertex(mesh, position, normal, {s, t});
		}
	const auto stride = static_cast<std::uint32_t>(columns + 1);
	for(int row = 0; row < rows; ++row)
		for(int column = 0; column < columns; ++column) {
			const std::uint32_t corner =
			    first + static_cast<std::uint32_t>(row) * stride + static_cast<std::uint32_t>(column);
			addQuad(mesh, corner, corner + stride, corner + stride + 1, corner + 1);
		}
}

/// @brief A disc cap of the cylinder. @param mesh The mesh. @param segments Around. @param y Its height.
/// @param up Whether it faces up.
void addCap(MeshData& mesh, int segments, float y, bool up) {
	const auto center = static_cast<std::uint32_t>(mesh.positions.size());
	const Vec3 normal{0.0f, up ? 1.0f : -1.0f, 0.0f};
	addVertex(mesh, {0.0f, y, 0.0f}, normal, {0.5f, 0.5f});
	for(int i = 0; i <= segments; ++i) {
		const float angle = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
		const float x = std::sin(angle), z = std::cos(angle);
		addVertex(mesh, {x, y, z}, normal, {0.5f + 0.5f * x, 0.5f - 0.5f * z});
	}
	for(int i = 0; i < segments; ++i) {
		const std::uint32_t a = center + 1 + static_cast<std::uint32_t>(i);
		if(up) mesh.indices.insert(mesh.indices.end(), {center, a, a + 1});
		else mesh.indices.insert(mesh.indices.end(), {center, a + 1, a});
	}
}

/// @brief The finishing touches every primitive gets. @param mesh The mesh. @return It.
MeshData finish(MeshData mesh) {
	mesh.computeBounds();
	return mesh;
}

} // namespace

const char* primitiveName(Primitive primitive) noexcept {
	switch(primitive) {
	case Primitive::Cube:
		return "cube";
	case Primitive::Sphere:
		return "sphere";
	case Primitive::Cylinder:
		return "cylinder";
	case Primitive::Torus:
		return "torus";
	case Primitive::Plane:
		return "plane";
	}
	return "cube";
}

std::optional<Primitive> primitiveNamed(std::string_view name) noexcept {
	for(const Primitive primitive :
	    {Primitive::Cube, Primitive::Sphere, Primitive::Cylinder, Primitive::Torus, Primitive::Plane})
		if(name == primitiveName(primitive)) return primitive;
	return std::nullopt;
}

MeshData makeCube() {
	MeshData mesh;
	// Each face: its normal and two axes across it, so that u × v = normal
	constexpr std::array<std::array<Vec3, 3>, 6> faces{{{{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}},
	                                                    {{{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}}},
	                                                    {{{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}},
	                                                    {{{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}},
	                                                    {{{0, 0, 1}, {1, 0, 0}, {0, 1, 0}}},
	                                                    {{{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}}}};
	for(const auto& [normal, u, v] : faces) {
		const auto first = static_cast<std::uint32_t>(mesh.positions.size());
		addVertex(mesh, normal - u - v, normal, {0, 1});
		addVertex(mesh, normal + u - v, normal, {1, 1});
		addVertex(mesh, normal + u + v, normal, {1, 0});
		addVertex(mesh, normal - u + v, normal, {0, 0});
		addQuad(mesh, first, first + 1, first + 2, first + 3);
	}
	return finish(std::move(mesh));
}

MeshData makeSphere(int segments, int rings) {
	MeshData mesh;
	addGrid(mesh, std::max(segments, 3), std::max(rings, 2), [](float s, float t) {
		const float theta = 2.0f * pi * s, phi = pi * t;
		const Vec3 point{std::sin(phi) * std::sin(theta), std::cos(phi), std::sin(phi) * std::cos(theta)};
		return std::pair{point, point};
	});
	return finish(std::move(mesh));
}

MeshData makeCylinder(int segments) {
	MeshData mesh;
	segments = std::max(segments, 3);
	addGrid(mesh, segments, 1, [](float s, float t) {
		const float angle = 2.0f * pi * s;
		const Vec3 normal{std::sin(angle), 0.0f, std::cos(angle)};
		return std::pair{Vec3{normal.x, 1.0f - 2.0f * t, normal.z}, normal};
	});
	addCap(mesh, segments, 1.0f, true);
	addCap(mesh, segments, -1.0f, false);
	return finish(std::move(mesh));
}

MeshData makeTorus(float majorRadius, float minorRadius, int segments, int sides) {
	MeshData mesh;
	addGrid(mesh, std::max(segments, 3), std::max(sides, 3), [&](float s, float t) {
		const float theta = 2.0f * pi * s, phi = -2.0f * pi * t; // t runs down the outside
		const Vec3 around{std::sin(theta), 0.0f, std::cos(theta)};
		const Vec3 normal = around * std::cos(phi) + Vec3{0.0f, std::sin(phi), 0.0f};
		return std::pair{around * majorRadius + normal * minorRadius, normal};
	});
	return finish(std::move(mesh));
}

MeshData makePlane(int subdivisions) {
	MeshData mesh;
	const int cells = std::max(subdivisions, 1);
	addGrid(mesh, cells, cells, [](float s, float t) {
		return std::pair{Vec3{2.0f * s - 1.0f, 0.0f, 2.0f * t - 1.0f}, Vec3{0.0f, 1.0f, 0.0f}};
	});
	return finish(std::move(mesh));
}

MeshData makePrimitive(Primitive primitive) {
	switch(primitive) {
	case Primitive::Cube:
		return makeCube();
	case Primitive::Sphere:
		return makeSphere();
	case Primitive::Cylinder:
		return makeCylinder();
	case Primitive::Torus:
		return makeTorus();
	case Primitive::Plane:
		return makePlane();
	}
	return makeCube();
}

} // namespace affresco::scene3d
