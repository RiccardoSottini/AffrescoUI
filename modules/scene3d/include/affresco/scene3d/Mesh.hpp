#pragma once

/// @file
/// @brief Mesh and texture data on the CPU, and the generated primitives (cube, sphere, cylinder, torus, plane).
///
/// This is the heavy data of a scene: it lives in native stores, never in a document's JSON state.

#include "affresco/scene3d/Bounds.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::scene3d {

/// @brief A texture coordinate.
struct TexCoord {
	float u = 0.0f; ///< Across, 0 at the left.
	float v = 0.0f; ///< Down, 0 at the top (as glTF).

	/// @brief Equal when both are equal.
	constexpr bool operator==(const TexCoord&) const noexcept = default;
};

/// @brief An indexed triangle mesh.
struct MeshData {
	std::vector<Vec3> positions;        ///< One per vertex.
	std::vector<Vec3> normals;          ///< One per vertex, or none (computed by computeNormals()).
	std::vector<TexCoord> texCoords;    ///< One per vertex, or none.
	std::vector<std::uint32_t> indices; ///< Three per triangle.
	Aabb bounds;                        ///< Around the positions (computeBounds()).

	/// @brief The number of triangles. @return It.
	[[nodiscard]] std::size_t triangleCount() const noexcept { return indices.size() / 3; }
	/// @brief Recomputes the bounds from the positions.
	void computeBounds() noexcept;
	/// @brief Smooth normals from the triangles, weighted by area.
	void computeNormals();
	/// @brief Whether the arrays agree: sizes match, every index is in range, everything is finite.
	/// @return An empty string if valid, else what is wrong.
	[[nodiscard]] std::string validate() const;
	/// @brief The unique edges, two indices each, for wireframe drawing with lines.
	/// @return The edge index list.
	[[nodiscard]] std::vector<std::uint32_t> edges() const;
	/// @brief The nearest triangle a ray hits, in the mesh's own space.
	/// @param ray The ray.
	/// @return The distance, or nothing.
	[[nodiscard]] std::optional<float> raycast(const Ray& ray) const noexcept;
};

/// @brief An RGBA8 texture, rows from the top, not premultiplied.
struct TextureData {
	int width = 0;                    ///< In pixels.
	int height = 0;                   ///< In pixels.
	std::vector<std::uint8_t> pixels; ///< width * height * 4 bytes.
	bool srgb = true;                 ///< Colour (sRGB) or data (linear: normals, metal and roughness).
};

/// @brief The kinds of generated primitive.
enum class Primitive { Cube, Sphere, Cylinder, Torus, Plane };

/// @brief A primitive's name, as documents spell it. @param primitive It. @return "cube", "sphere"...
[[nodiscard]] const char* primitiveName(Primitive primitive) noexcept;
/// @brief A primitive by name. @param name "cube", "sphere", "cylinder", "torus" or "plane". @return It, or nothing.
[[nodiscard]] std::optional<Primitive> primitiveNamed(std::string_view name) noexcept;

/// @brief A cube of side 2 centred at the origin, flat-shaded (24 vertices).
/// @return The mesh.
[[nodiscard]] MeshData makeCube();
/// @brief A UV sphere of radius 1.
/// @param segments Around, at least 3. @param rings From pole to pole, at least 2.
/// @return The mesh.
[[nodiscard]] MeshData makeSphere(int segments = 32, int rings = 16);
/// @brief A capped cylinder of radius 1 and height 2 along y.
/// @param segments Around, at least 3.
/// @return The mesh.
[[nodiscard]] MeshData makeCylinder(int segments = 32);
/// @brief A torus around y.
/// @param majorRadius To the tube's centre. @param minorRadius Of the tube.
/// @param segments Around y, at least 3. @param sides Around the tube, at least 3.
/// @return The mesh.
[[nodiscard]] MeshData makeTorus(float majorRadius = 1.0f, float minorRadius = 0.25f, int segments = 48,
                                 int sides = 16);
/// @brief A square of side 2 in the xz plane, facing up.
/// @param subdivisions Cells per side, at least 1.
/// @return The mesh.
[[nodiscard]] MeshData makePlane(int subdivisions = 1);
/// @brief Any primitive, with its default detail. @param primitive Which. @return The mesh.
[[nodiscard]] MeshData makePrimitive(Primitive primitive);

} // namespace affresco::scene3d
