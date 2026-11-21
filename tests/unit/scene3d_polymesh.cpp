/// @file
/// @brief The scene3d module's editable polygon meshes: their JSON, triangles, edit operations, modifiers and picking.

#include <affresco/scene3d/PolyMesh.hpp>
#include <affresco/scene3d/Viewport.hpp>

#include <doctest/doctest.h>

#include <cmath>

using namespace affresco::scene3d;
using affresco::json;

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first

namespace {

/// @brief Whether every face's normal points away from the mesh's centre (a closed convex mesh).
/// @param mesh It. @return Whether they do.
bool outward(const PolyMesh& mesh) {
	Vec3 centre;
	for(const Vec3 v : mesh.vertices) centre = centre + v;
	centre = centre / static_cast<float>(mesh.vertices.size());
	for(std::size_t f = 0; f < mesh.faces.size(); ++f)
		if(dot(faceNormal(mesh, f), faceCentre(mesh, f) - centre) <= 0.0f) return false;
	return true;
}

/// @brief Whether every edge of a mesh is held by exactly two faces (it is closed). @param mesh It. @return Whether.
bool closed(const PolyMesh& mesh) {
	for(const auto& edge : mesh.edges()) {
		int uses = 0;
		for(const auto& face : mesh.faces)
			for(std::size_t i = 0; i < face.size(); ++i) {
				const std::uint32_t a = face[i], b = face[(i + 1) % face.size()];
				if((a == edge[0] && b == edge[1]) || (a == edge[1] && b == edge[0])) ++uses;
			}
		if(uses != 2) return false;
	}
	return true;
}

} // namespace

TEST_CASE("scene3d: a polygon mesh reads and writes JSON, and refuses what is malformed") {
	const PolyMesh cube = polyPrimitive(Primitive::Cube);
	CHECK(cube.vertices.size() == 8);
	CHECK(cube.faces.size() == 6);
	CHECK(cube.edges().size() == 12);
	CHECK(outward(cube));
	CHECK(closed(cube));
	const PolyMesh again = polyMeshFromJson(toJson(cube));
	CHECK(again.faces == cube.faces);
	CHECK(again.vertices == cube.vertices);
	CHECK_THROWS_AS((void)polyMeshFromJson(json::parse(R"({"vertices": [[0,0,0]], "faces": [[0, 1, 2]]})")),
	                affresco::DocumentError);
	CHECK_THROWS_AS((void)polyMeshFromJson(json::parse(R"({"vertices": [], "faces": [[0, 0]]})")),
	                affresco::DocumentError);
	CHECK_THROWS_AS((void)polyMeshFromJson(json::parse(R"({"vertices": [[0, "a", 0]], "faces": []})")),
	                affresco::DocumentError);
}

TEST_CASE("scene3d: a polygon mesh triangulates flat or smooth") {
	PolyMesh cube = polyPrimitive(Primitive::Cube);
	const MeshData flat = triangulate(cube);
	CHECK(flat.triangleCount() == 12);
	CHECK(flat.positions.size() == 24); // each face its own corners
	CHECK(flat.validate().empty());
	cube.smooth = true;
	const MeshData smooth = triangulate(cube);
	CHECK(smooth.positions.size() == 8);
	CHECK(smooth.validate().empty());
	const PolyMesh sphere = polyPrimitive(Primitive::Sphere);
	CHECK(closed(sphere)); // welded at its seam and poles
}

TEST_CASE("scene3d: extrude, inset, delete and loop cut") {
	const PolyMesh cube = polyPrimitive(Primitive::Cube);
	const PolyMesh extruded = extrudeFaces(cube, {3}, 1.0f); // the top
	CHECK(extruded.vertices.size() == 12);
	CHECK(extruded.faces.size() == 10);
	CHECK(closed(extruded));
	CHECK(faceCentre(extruded, 3).y == doctest::Approx(2.0f)); // the moved face keeps its index
	CHECK(outward(extruded));

	const PolyMesh region = extrudeFaces(cube, {3, 1}, 0.5f); // two faces sharing an edge: no wall between them
	CHECK(closed(region));
	CHECK(region.faces.size() == 6 + 6);

	const PolyMesh inset = insetFaces(cube, {3}, 0.5f);
	CHECK(inset.faces.size() == 10);
	CHECK(inset.vertices.size() == 12);
	CHECK(closed(inset));
	CHECK(std::abs(inset.vertices[inset.faces[3][0]].x) == doctest::Approx(0.5f));

	const PolyMesh open = deleteFaces(cube, {3});
	CHECK(open.faces.size() == 5);
	CHECK(open.vertices.size() == 8);
	const PolyMesh cornerless = deleteVertices(cube, {0});
	CHECK(cornerless.faces.size() == 3);
	CHECK(cornerless.vertices.size() == 7);

	const PolyMesh cut = loopCut(cube, 2, 6); // an edge of the top: the ring around the cube across it
	CHECK(cut.faces.size() == 10);
	CHECK(cut.vertices.size() == 12);
	CHECK(closed(cut));
	CHECK(loopCut(cube, 0, 7).faces.size() == 6); // not an edge: unchanged

	const PolyMesh bevelled = bevelEdge(cube, 2, 6, 0.25f);
	CHECK(bevelled.faces.size() == 7);
	CHECK(bevelled.vertices.size() == 10);
	CHECK(closed(bevelled));
	CHECK(outward(bevelled));
	CHECK(bevelEdge(deleteFaces(cube, {3}), 2, 6, 0.25f).faces.size() == 5); // one face holds it: unchanged
}

TEST_CASE("scene3d: moving vertices with a falloff, scaling and flipping") {
	const PolyMesh plane = arrayMesh(polyPrimitive(Primitive::Plane), 3, {2.0f, 0.0f, 0.0f});
	CHECK(plane.vertices.size() == 12);
	const PolyMesh lifted = moveVertices(plane, {0}, {0.0f, 1.0f, 0.0f});
	CHECK(lifted.vertices[0].y == doctest::Approx(1.0f));
	CHECK(lifted.vertices[1].y == doctest::Approx(0.0f));
	const PolyMesh soft = moveVertices(plane, {0}, {0.0f, 1.0f, 0.0f}, 3.0f);
	CHECK(soft.vertices[0].y == doctest::Approx(1.0f));
	CHECK(soft.vertices[1].y > 0.0f); // 2 away: moved less
	CHECK(soft.vertices[1].y < 1.0f);
	CHECK(soft.vertices[5].y == doctest::Approx(0.0f)); // past the radius: still
	const PolyMesh grown = scaleVertices(polyPrimitive(Primitive::Cube), {0, 1}, 2.0f);
	CHECK(grown.vertices[0].x == doctest::Approx(-2.0f));
	CHECK(dot(faceNormal(flipFaces(plane), 0), {0.0f, 1.0f, 0.0f}) == doctest::Approx(-1.0f));
	const PolyMesh turned = rotateVertices(polyPrimitive(Primitive::Plane), {0, 1, 2, 3},
	                                       Quat::fromAxisAngle({0.0f, 1.0f, 0.0f}, 3.14159265f / 2.0f));
	CHECK(turned.vertices[0].x == doctest::Approx(1.0f).epsilon(1e-4)); // (-1, 0, 1) a quarter turn about y
	CHECK(turned.vertices[0].z == doctest::Approx(1.0f).epsilon(1e-4));
}

TEST_CASE("scene3d: mirror, array and subdivision") {
	const PolyMesh half = polyPrimitive(Primitive::Plane);
	PolyMesh right = half;
	for(Vec3& v : right.vertices) v.x = v.x * 0.5f + 0.5f; // from x = 0 to 1
	const PolyMesh both = mirrorMesh(right, 0);
	CHECK(both.faces.size() == 2);
	CHECK(both.vertices.size() == 6);                                             // the two on the plane are shared
	CHECK(dot(faceNormal(both, 1), {0.0f, 1.0f, 0.0f}) == doctest::Approx(1.0f)); // still facing up

	const PolyMesh rows = arrayMesh(polyPrimitive(Primitive::Cube), 4, {3.0f, 0.0f, 0.0f});
	CHECK(rows.faces.size() == 24);
	CHECK(rows.vertices[31].x == doctest::Approx(10.0f));

	const PolyMesh smooth = subdivide(polyPrimitive(Primitive::Cube), 1);
	CHECK(smooth.faces.size() == 24);
	CHECK(smooth.vertices.size() == 26);
	CHECK(closed(smooth));
	CHECK(outward(smooth));
	for(const Vec3 v : smooth.vertices) CHECK(length(v) < 1.74f); // pulled in from the corners
	CHECK(subdivide(polyPrimitive(Primitive::Cube), 2).faces.size() == 96);
	PolyLimits small;
	small.faces = 30;
	CHECK(subdivide(polyPrimitive(Primitive::Cube), 3, small).faces.size() == 24); // stops at the limit
}

TEST_CASE("scene3d: a wireframe as bars along the edges") {
	const PolyMesh cube = polyPrimitive(Primitive::Cube);
	const PolyMesh all = edgeBars(cube, {}, 0.02f);
	CHECK(all.vertices.size() == 12 * 8);
	CHECK(all.faces.size() == 12 * 6);
	const PolyMesh one = edgeBars(cube, {{0, 1}}, 0.02f);
	CHECK(one.faces.size() == 6);
	CHECK(closed(one));
	CHECK(outward(one));
	for(const Vec3 v : one.vertices) CHECK(std::abs(v.y + 1.0f) <= 0.0101f);
	CHECK(edgeBars(cube, {{0, 99}}, 0.02f).faces.empty()); // not a vertex: skipped
}

TEST_CASE("scene3d: picking a vertex, an edge or a face of a polygon mesh") {
	const PolyMesh cube = polyPrimitive(Primitive::Cube);
	const auto vertex = nearestVertex(cube, {0.9f, 1.0f, 0.8f});
	REQUIRE(vertex);
	CHECK(cube.vertices[*vertex] == Vec3{1.0f, 1.0f, 1.0f});
	const auto edge = nearestEdge(cube, {0.0f, 1.0f, 0.95f});
	REQUIRE(edge);
	CHECK(cube.vertices[(*edge)[0]].z == doctest::Approx(1.0f));
	CHECK(cube.vertices[(*edge)[1]].y == doctest::Approx(1.0f));
	const auto face = nearestFace(cube, {0.9f, 1.0f, 0.9f}); // near a corner of the top
	REQUIRE(face);
	CHECK(*face == 3);
	CHECK_FALSE(nearestVertex(PolyMesh{}, {}).has_value());
}

TEST_CASE("scene3d: a gimbal's axes follow the Euler angles") {
	const auto near = [](Vec3 a, Vec3 b) { return length(a - b) < 1e-4f; };
	Transform turned;
	turned.rotation = Quat::fromEuler({0.0f, 0.0f, 3.14159265f / 2.0f}); // a quarter turn about z
	const auto axes = gimbalAxes(turned.matrix(), Quat{});
	CHECK(near(axes[0], {0.0f, 1.0f, 0.0f})); // x turns with z
	CHECK(near(axes[1], {-1.0f, 0.0f, 0.0f}));
	CHECK(near(axes[2], {0.0f, 0.0f, 1.0f}));                            // z stays
	turned.rotation = Quat::fromEuler({3.14159265f / 2.0f, 0.0f, 0.0f}); // about x only: y and z don't follow it
	const auto still = gimbalAxes(turned.matrix(), Quat{});
	CHECK(near(still[1], {0.0f, 1.0f, 0.0f}));
	CHECK(near(still[2], {0.0f, 0.0f, 1.0f}));
	Gizmo gizmo;
	gizmo.axes = still;
	CHECK(near(gizmo.axis(1), {0.0f, 1.0f, 0.0f}));
}

// NOLINTEND(bugprone-unchecked-optional-access)
