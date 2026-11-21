#pragma once

/// @file
/// @brief Editable polygon meshes: vertices and faces of any number of sides, small enough to live in a document's
/// state, with the operations a modeller's edit mode runs on them (extrude, inset, loop cut, delete, move with a
/// falloff) and the modifiers it stacks (mirror, array, subdivision).
///
/// Every operation is pure: it takes a mesh and gives another, so a document keeps its history and undo. A scene
/// gives such meshes in its `"meshes"` list (SceneView triangulates them into its Assets under their ids).

#include "affresco/core/Json.hpp"
#include "affresco/scene3d/Math.hpp"
#include "affresco/scene3d/Mesh.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace affresco::scene3d {

/// @brief A polygon mesh: faces list their vertices counter-clockwise, seen from outside.
struct PolyMesh {
	std::vector<Vec3> vertices;                    ///< The points.
	std::vector<std::vector<std::uint32_t>> faces; ///< Each at least 3 vertex indices.
	bool smooth = false;                           ///< Shaded smooth (shared normals) or flat (one per face).

	/// @brief The unique edges, each as its two vertex indices, the lower first. @return They.
	[[nodiscard]] std::vector<std::array<std::uint32_t, 2>> edges() const;
};

/// @brief How large a polygon mesh may be.
struct PolyLimits {
	std::size_t vertices = 65'536; ///< Vertices at most.
	std::size_t faces = 65'536;    ///< Faces at most.
	std::size_t sides = 64;        ///< Vertices of one face at most.
};

/// @brief Reads `{vertices: [[x, y, z], ...], faces: [[i, j, k, ...], ...], smooth}`.
/// @param value It. @param limits Its size at most.
/// @return The mesh.
/// @throws DocumentError If malformed, over the limits, or a face names a vertex it hasn't.
[[nodiscard]] PolyMesh polyMeshFromJson(const json& value, const PolyLimits& limits = {});
/// @brief A polygon mesh as polyMeshFromJson() reads it, numbers rounded to 1e-5. @param mesh It. @return The JSON.
[[nodiscard]] json toJson(const PolyMesh& mesh);
/// @brief Triangles to draw: flat (each face its own normal) or smooth (shared normals).
/// @param mesh It. @return The triangle mesh, with normals and bounds.
[[nodiscard]] MeshData triangulate(const PolyMesh& mesh);
/// @brief A primitive as polygons: the cube as six quads, the plane as one; the others welded from their triangles.
/// @param primitive Which. @return The mesh.
[[nodiscard]] PolyMesh polyPrimitive(Primitive primitive);
/// @brief A triangle mesh as polygons: its triangles as faces, the vertices at one place (seams, poles) joined.
/// @param mesh It. @param limits What the result may hold. @return The mesh, shaded smooth.
/// @throws DocumentError If it holds more than the limits.
[[nodiscard]] PolyMesh polyFromMesh(const MeshData& mesh, const PolyLimits& limits = {});
/// @brief A face's centre (the mean of its vertices). @param mesh The mesh. @param face Its index. @return It.
[[nodiscard]] Vec3 faceCentre(const PolyMesh& mesh, std::size_t face);
/// @brief A face's unit normal (Newell's method). @param mesh The mesh. @param face Its index. @return It.
[[nodiscard]] Vec3 faceNormal(const PolyMesh& mesh, std::size_t face);

/// @brief Extrudes a region of faces along their normals: the faces move out and walls join their outline.
/// @param mesh The mesh. @param faces The faces (indices; others ignored). @param distance How far.
/// @return The mesh; the moved faces keep their indices.
[[nodiscard]] PolyMesh extrudeFaces(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces, float distance);
/// @brief Insets each face: a smaller copy inside it, joined by a ring of quads.
/// @param mesh The mesh. @param faces The faces. @param amount How far towards the centre, 0 to 1.
/// @return The mesh; the inner faces keep the indices.
[[nodiscard]] PolyMesh insetFaces(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces, float amount);
/// @brief Removes faces, and the vertices no face uses any more. @param mesh The mesh. @param faces The faces.
/// @return The mesh.
[[nodiscard]] PolyMesh deleteFaces(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces);
/// @brief Removes vertices and every face that uses one. @param mesh The mesh. @param vertices They.
/// @return The mesh.
[[nodiscard]] PolyMesh deleteVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices);
/// @brief Cuts a loop of quads across an edge: each quad of the ring is split in two at its middle.
/// @param mesh The mesh. @param a One end of the edge. @param b The other.
/// @return The mesh (unchanged when no quad holds the edge).
[[nodiscard]] PolyMesh loopCut(const PolyMesh& mesh, std::uint32_t a, std::uint32_t b);
/// @brief Bevels an edge held by two faces: a narrow face takes its place, its ends joined into the faces around them.
/// @param mesh The mesh. @param a One end of the edge. @param b The other. @param amount How far into each face, 0 to
/// 0.5 of its neighbouring edges. @return The mesh (unchanged unless exactly two faces hold the edge); the new face
/// last.
[[nodiscard]] PolyMesh bevelEdge(const PolyMesh& mesh, std::uint32_t a, std::uint32_t b, float amount);
/// @brief Moves vertices, and with a radius the ones near them too, less the further they are (smooth falloff).
/// @param mesh The mesh. @param vertices They. @param delta The move. @param radius 0 moves only them.
/// @return The mesh.
[[nodiscard]] PolyMesh moveVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices, Vec3 delta,
                                    float radius = 0.0f);
/// @brief Scales vertices about their centre, and with a radius the ones near them, less the further they are.
/// @param mesh The mesh. @param vertices They. @param factor The scale. @param radius 0 scales only them.
/// @return The mesh.
[[nodiscard]] PolyMesh scaleVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices, float factor,
                                     float radius = 0.0f);
/// @brief Turns vertices about their centre, and with a radius the ones near them, less the further they are.
/// @param mesh The mesh. @param vertices They. @param rotation The turn. @param radius 0 turns only them.
/// @return The mesh.
[[nodiscard]] PolyMesh rotateVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& vertices, Quat rotation,
                                      float radius = 0.0f);
/// @brief Every vertex of some faces, once each, in order. @param mesh The mesh. @param faces They. @return They.
[[nodiscard]] std::vector<std::uint32_t> faceVertices(const PolyMesh& mesh, const std::vector<std::uint32_t>& faces);
/// @brief Flips every face: normals point the other way. @param mesh The mesh. @return The mesh.
[[nodiscard]] PolyMesh flipFaces(const PolyMesh& mesh);

/// @brief A mesh of thin square bars along edges: a wireframe drawn as surfaces, in any material.
/// @param mesh The mesh. @param edges The edges (each two vertex indices); none: every edge.
/// @param thickness The bars' width. @param limits What the result may hold: edges past it are left out.
/// @return The bars.
[[nodiscard]] PolyMesh edgeBars(const PolyMesh& mesh, const std::vector<std::array<std::uint32_t, 2>>& edges,
                                float thickness, const PolyLimits& limits = {});
/// @brief Mirrors a mesh across a plane through the origin, the vertices on the plane joined. @param mesh The mesh.
/// @param axis 0 (x), 1 (y) or 2 (z). @return The mesh and its mirror image.
[[nodiscard]] PolyMesh mirrorMesh(const PolyMesh& mesh, int axis);
/// @brief Copies of a mesh, each moved by an offset from the last. @param mesh The mesh. @param count Copies, 1 to
/// 64 (1 is the mesh). @param offset The move. @return The mesh.
[[nodiscard]] PolyMesh arrayMesh(const PolyMesh& mesh, int count, Vec3 offset);
/// @brief Catmull-Clark subdivision: every face becomes quads, the surface smoother each level.
/// @param mesh The mesh. @param levels 0 to 4. @param limits What the result may hold; a level past it stops.
/// @return The mesh.
[[nodiscard]] PolyMesh subdivide(const PolyMesh& mesh, int levels, const PolyLimits& limits = {});

/// @brief The vertex nearest a point. @param mesh The mesh. @param point In the mesh's space. @return Its index, or
/// nothing for an empty mesh.
[[nodiscard]] std::optional<std::uint32_t> nearestVertex(const PolyMesh& mesh, Vec3 point);
/// @brief The edge nearest a point. @param mesh The mesh. @param point In the mesh's space. @return Its ends.
[[nodiscard]] std::optional<std::array<std::uint32_t, 2>> nearestEdge(const PolyMesh& mesh, Vec3 point);
/// @brief The face a point on the surface lies in (else the nearest). @param mesh The mesh. @param point In the
/// mesh's space. @return Its index.
[[nodiscard]] std::optional<std::uint32_t> nearestFace(const PolyMesh& mesh, Vec3 point);

} // namespace affresco::scene3d
