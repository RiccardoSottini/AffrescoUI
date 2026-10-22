/// @file
/// @brief The scene3d module's tools: the orbit camera, ray picking and box select, gizmo handles and drags, and the
/// material graph's checks and assembly.

#include <affresco/scene3d/Camera.hpp>
#include <affresco/scene3d/Gizmo.hpp>
#include <affresco/scene3d/MaterialGraph.hpp>
#include <affresco/scene3d/Mesh.hpp>
#include <affresco/scene3d/Picking.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

using namespace affresco::scene3d;
using affresco::json;

namespace {

/// @brief Whether two vectors are within a tolerance. @param a One. @param b The other. @param tolerance It.
/// @return true if so.
bool near(Vec3 a, Vec3 b, float tolerance = 1e-3f) { return length(a - b) <= tolerance; }

/// @brief A scene of named cubes at some places. @param places Where. @param assets Receives the cube.
/// @return The scene, nodes "a", "b", ...
Scene cubes(std::initializer_list<Vec3> places, Assets& assets) {
	assets.setMesh("cube", makeCube());
	Scene scene;
	char name = 'a';
	for(const Vec3 place : places) {
		Node node;
		node.id = std::string(1, name++);
		node.mesh = "cube";
		node.transform.translation = place;
		scene.nodes.push_back(node);
	}
	return scene;
}

/// @brief The ray from a far point towards another. @param from Where. @param to Towards. @return The ray.
Ray rayFrom(Vec3 from, Vec3 to) { return {from, normalize(to - from)}; }

/// @brief A graph node for an operation. @param op It. @param id Its id. @return The node.
json node(const char* op, const char* id) { return makeMaterialNode(op, id, 0, 0); }

/// @brief A link. @param from Source node. @param to Target node. @param input Target port. @return The link.
json link(const char* from, const char* to, const char* input) {
	return {{"from", from}, {"output", "out"}, {"to", to}, {"input", input}};
}

} // namespace

TEST_CASE("the orbit camera places its eye, projects and casts rays") {
	OrbitCamera camera;
	camera.snap(StandardView::Front);
	camera.distance = 5.0f;
	CHECK(near(camera.eye(), {0, 0, 5}));
	const Ray centre = camera.rayThrough(400, 300, 800, 600);
	CHECK(near(centre.direction, {0, 0, -1}));
	const auto at = camera.project({0, 0, 0}, 800, 600);
	REQUIRE(at);
	CHECK(at.value().first == doctest::Approx(400));
	CHECK(at.value().second == doctest::Approx(300));
	CHECK_FALSE(camera.project({0, 0, 10}, 800, 600));
	camera.snap(StandardView::Top);
	CHECK(near(camera.eye(), {0, 5, 0}));
	const auto above = camera.project({1, 0, 0}, 800, 600);
	REQUIRE(above);
	CHECK(above.value().first > 400.0f); // +x stays to the right looking down
	camera.snap(StandardView::Right);
	CHECK(near(camera.eye(), {5, 0, 0}));
}

TEST_CASE("the orbit camera orbits, pans under the pointer, zooms and frames") {
	OrbitCamera camera;
	const auto before = camera.project({0, 0, 0}, 800, 600);
	camera.pan(10, 0, 600);
	const auto after = camera.project({0, 0, 0}, 800, 600);
	REQUIRE(before);
	REQUIRE(after);
	CHECK(after.value().first - before.value().first == doctest::Approx(10).epsilon(0.01));
	camera.orbit(0, 10000);
	CHECK(camera.pitch == doctest::Approx(std::numbers::pi_v<float> / 2));
	const float distance = camera.distance;
	camera.zoom(1);
	CHECK(camera.distance == doctest::Approx(distance * 0.9f));
	Aabb box;
	box.extend(Vec3{10, 10, 10});
	box.extend(Vec3{12, 14, 11});
	camera.frame(box);
	CHECK(near(camera.target, box.center()));
	for(const Vec3 corner : {box.min, box.max}) {
		const auto at = camera.project(corner, 600, 600);
		REQUIRE(at);
		CHECK(at.value().first >= 0.0f);
		CHECK(at.value().first <= 600.0f);
	}
	camera.lens.orthographic = true;
	const Ray a = camera.rayThrough(10, 10, 600, 600), b = camera.rayThrough(500, 400, 600, 600);
	CHECK(near(a.direction, b.direction)); // parallel rays
}

TEST_CASE("the orbit camera reads and writes JSON") {
	OrbitCamera camera;
	camera.target = {1, 2, 3};
	camera.lens.orthographic = true;
	const OrbitCamera back = cameraFromJson(toJson(camera));
	CHECK(near(back.target, camera.target));
	CHECK(back.lens.orthographic);
	CHECK(back.yaw == doctest::Approx(camera.yaw));
	CHECK(cameraFromJson(json{{"distance", -4}, {"pitch", 9}}).distance > 0.0f);
	CHECK(cameraFromJson(json{{"pitch", 9}}).pitch <= std::numbers::pi_v<float> / 2);
	CHECK_THROWS_AS((void)cameraFromJson(json{{"yaw", "left"}}), affresco::DocumentError);
	CHECK_THROWS_AS((void)cameraFromJson(json{{"orthographic", 1}}), affresco::DocumentError);
	CHECK_THROWS_AS((void)cameraFromJson(json::array()), affresco::DocumentError);
	CHECK(standardViewNamed("top") == StandardView::Top);
	CHECK_FALSE(standardViewNamed("diagonal"));
}

TEST_CASE("ray picking finds the nearest visible mesh") {
	Assets assets;
	Scene scene = cubes({{0, 0, 0}, {0, 0, -5}, {5, 0, 0}}, assets);
	const auto hit = pickNode(scene, assets, rayFrom({0, 0, 10}, {0, 0, 0}));
	REQUIRE(hit);
	CHECK(hit.value().node == "a");
	CHECK(hit.value().distance == doctest::Approx(9.0f));
	CHECK(near(hit.value().point, {0, 0, 1}));
	scene.nodes[0].visible = false;
	CHECK(pickNode(scene, assets, rayFrom({0, 0, 10}, {0, 0, 0})).value().node == "b");
	scene.nodes[1].parent = "a"; // a hidden parent hides its children
	scene.nodes[1].transform.translation = {0, 0, -5};
	CHECK_FALSE(pickNode(scene, assets, rayFrom({0, 0, 10}, {0, 0, 0})));
	CHECK_FALSE(pickNode(scene, assets, rayFrom({0, 10, 10}, {0, 20, 0})));
	scene.nodes[2].transform.scale = {3, 3, 3};
	CHECK(pickNode(scene, assets, rayFrom({5, 2.9f, 10}, {5, 2.9f, 0})).value().node == "c");
}

TEST_CASE("box select takes the meshes whose centre is inside") {
	Assets assets;
	const Scene scene = cubes({{-2, 0, 0}, {2, 0, 0}, {0, 3, 0}}, assets);
	OrbitCamera camera;
	camera.snap(StandardView::Front);
	camera.distance = 10;
	CHECK(boxSelect(scene, assets, camera, 800, 600, 0, 200, 400, 400) == std::vector<std::string>{"a"});
	CHECK(boxSelect(scene, assets, camera, 800, 600, 800, 400, 0, 200) == std::vector<std::string>{"a", "b"});
	CHECK(boxSelect(scene, assets, camera, 800, 600, 0, 0, 800, 600).size() == 3);
	CHECK(boxSelect(scene, assets, camera, 800, 600, 0, 0, 10, 10).empty());
}

TEST_CASE("gizmo handles are found along their axes, planes and rings") {
	Gizmo gizmo;
	CHECK(hitHandle(gizmo, rayFrom({0.7f, 0, 10}, {0.7f, 0, 0})) == GizmoAxis::X);
	CHECK(hitHandle(gizmo, rayFrom({0, 0.7f, 10}, {0, 0.7f, 0})) == GizmoAxis::Y);
	CHECK(hitHandle(gizmo, rayFrom({10, 0, 0.7f}, {0, 0, 0.7f})) == GizmoAxis::Z);
	CHECK(hitHandle(gizmo, rayFrom({0.3f, 0.3f, 10}, {0.3f, 0.3f, 0})) == GizmoAxis::XY);
	CHECK(hitHandle(gizmo, rayFrom({0, 0, 10}, {0, 0, 0})) == GizmoAxis::View);
	CHECK(hitHandle(gizmo, rayFrom({3, 3, 10}, {3, 3, 0})) == GizmoAxis::None);
	CHECK(hitHandle(gizmo, rayFrom({1.5f, 0, 10}, {1.5f, 0, 0})) == GizmoAxis::None);
	gizmo.mode = GizmoMode::Rotate;
	CHECK(hitHandle(gizmo, rayFrom({0, 10, 1}, {0, 0, 1})) == GizmoAxis::Y);
	CHECK(hitHandle(gizmo, rayFrom({0.7071f, 0.7071f, 10}, {0.7071f, 0.7071f, 0})) == GizmoAxis::Z);
	gizmo.mode = GizmoMode::Translate;
	gizmo.orientation = Quat::fromAxisAngle({0, 0, 1}, radians(90)); // local x points up
	CHECK(hitHandle(gizmo, rayFrom({0, 0.7f, 10}, {0, 0.7f, 0})) == GizmoAxis::X);
	CHECK(gizmoAxisNamed("zx") == GizmoAxis::ZX);
	CHECK(gizmoModeNamed("scale") == GizmoMode::Scale);
	CHECK_FALSE(gizmoAxisNamed("w"));
}

TEST_CASE("gizmo drags translate, rotate and scale with constraints and snapping") {
	Gizmo gizmo;
	const Ray start = rayFrom({0.5f, 0, 10}, {0.5f, 0, 0});
	const auto moved = dragGizmo(gizmo, GizmoAxis::X, start, rayFrom({2.2f, 1, 10}, {2.2f, 1, 0}), {});
	REQUIRE(moved);
	CHECK(near(moved.value().translation, {1.7f, 0, 0}));
	const auto snappedMove = dragGizmo(gizmo, GizmoAxis::X, start, rayFrom({2.2f, 1, 10}, {2.2f, 1, 0}), {0.5f, 0, 0});
	CHECK(near(snappedMove.value().translation, {1.5f, 0, 0}));
	const auto inPlane = dragGizmo(gizmo, GizmoAxis::XY, start, rayFrom({1.5f, 2, 10}, {1.5f, 2, 0}), {});
	CHECK(near(inPlane.value().translation, {1, 2, 0}));
	CHECK_FALSE(dragGizmo(gizmo, GizmoAxis::None, start, start, {}));

	gizmo.mode = GizmoMode::Rotate;
	const auto turned =
	    dragGizmo(gizmo, GizmoAxis::Y, rayFrom({1, 10, 0}, {1, 0, 0}), rayFrom({0, 10, -1}, {0, 0, -1}), {});
	REQUIRE(turned);
	CHECK(near(turned.value().rotation.rotate({1, 0, 0}), {0, 0, -1}));
	const auto snappedTurn = dragGizmo(gizmo, GizmoAxis::Y, rayFrom({1, 10, 0}, {1, 0, 0}),
	                                   rayFrom({0.9f, 10, -1}, {0.9f, 0, -1}), {0, radians(15), 0});
	CHECK(near(snappedTurn.value().rotation.rotate({1, 0, 0}),
	           Quat::fromAxisAngle({0, 1, 0}, radians(45)).rotate({1, 0, 0})));

	gizmo.mode = GizmoMode::Scale;
	const auto scaled =
	    dragGizmo(gizmo, GizmoAxis::Z, rayFrom({10, 0, 0.5f}, {0, 0, 0.5f}), rayFrom({10, 0, 1}, {0, 0, 1}), {});
	REQUIRE(scaled);
	CHECK(near(scaled.value().scale, {1, 1, 2}));
	const auto uniform = dragGizmo(gizmo, GizmoAxis::View, rayFrom({0.5f, 0, 10}, {0.5f, 0, 0}),
	                               rayFrom({1.5f, 0, 10}, {1.5f, 0, 0}), {});
	CHECK(near(uniform.value().scale, {3, 3, 3}));
}

TEST_CASE("typed numbers and deltas apply through the hierarchy") {
	Gizmo gizmo;
	CHECK(near(numericDelta(gizmo, GizmoAxis::Y, 2, {0, 0, -1}).translation, {0, 2, 0}));
	gizmo.mode = GizmoMode::Rotate;
	CHECK(near(numericDelta(gizmo, GizmoAxis::Z, 90, {0, 0, -1}).rotation.rotate({1, 0, 0}), {0, 1, 0}));
	gizmo.mode = GizmoMode::Scale;
	CHECK(near(numericDelta(gizmo, GizmoAxis::View, 2, {0, 0, -1}).scale, {2, 2, 2}));
	CHECK(near(numericDelta(gizmo, GizmoAxis::XY, 3, {0, 0, -1}).scale, {3, 3, 1}));

	// A child of a parent moved to (10, 0, 0) and turned a quarter about y; the gizmo at the child's world place.
	Transform parent;
	parent.translation = {10, 0, 0};
	parent.rotation = Quat::fromAxisAngle({0, 1, 0}, radians(90));
	Transform child;
	child.translation = {1, 0, 0};
	const Mat4 parentWorld = parent.matrix();
	gizmo = Gizmo{};
	gizmo.origin = parentWorld.transformPoint(child.translation);
	GizmoDelta up;
	up.translation = {0, 3, 0};
	const Transform moved = applyDelta(child, parentWorld, gizmo, up);
	CHECK(near((parentWorld * moved.matrix()).origin(), gizmo.origin + Vec3{0, 3, 0}));
	gizmo.mode = GizmoMode::Rotate;
	GizmoDelta turn;
	turn.rotation = Quat::fromAxisAngle({0, 1, 0}, radians(90));
	const Transform turned = applyDelta(child, parentWorld, gizmo, turn);
	CHECK(near(turned.translation, child.translation)); // it turns about its own place
	CHECK(near((parentWorld * turned.matrix()).transformDirection({1, 0, 0}), {-1, 0, 0}));
	GizmoDelta twice;
	twice.scale = {2, 2, 2};
	CHECK(near(applyDelta(child, parentWorld, gizmo, twice).scale, {2, 2, 2}));
}

TEST_CASE("material graphs compile from fixed snippets") {
	json graph{{"nodes",
	            {node("output", "out"), node("color", "red"), node("noise", "grain"), node("mix", "blend"),
	             node("number", "half")}},
	           {"links",
	            {link("red", "blend", "a"), link("grain", "blend", "factor"), link("blend", "out", "baseColor"),
	             link("half", "out", "roughness")}}};
	graph["nodes"][1]["value"] = {1, 0, 0};
	const auto result = compileMaterialGraph(graph);
	REQUIRE(result.ok());
	CHECK(result.glsl.find("void graphMaterial(") != std::string::npos);
	CHECK(result.glsl.find("vec3(1.000000, 0.000000, 0.000000)") != std::string::npos);
	CHECK(result.glsl.find("baseColor = n") != std::string::npos);
	CHECK(result.glsl.find("roughness = n") != std::string::npos);
	CHECK(result.glsl.find("metallic =") == std::string::npos);
	CHECK(result.glsl.find("grain") == std::string::npos); // ids never reach the shader
	for(const auto& op : materialOperations()) CHECK(makeMaterialNode(op, "x", 0, 0).is_object());
	CHECK(makeMaterialNode("shader", "x", 0, 0).is_null());
	CHECK(compileMaterialGraph({{"nodes", {node("output", "out")}}}).ok());
}

TEST_CASE("material graphs refuse wrong kinds, cycles, unknown ops and source text") {
	const auto errors = [](const json& graph) { return compileMaterialGraph(graph).errors; };
	const json output = node("output", "out");
	CHECK_FALSE(errors({{"nodes", json::array()}}).empty()); // no output
	CHECK_FALSE(errors({{"nodes", {output, output}}}).empty());
	CHECK_FALSE(errors({{"nodes", {output, {{"id", "s"}, {"op", "glsl"}}}}}).empty());
	CHECK_FALSE(errors({{"nodes", {output, node("number", "n")}}, {"links", {link("n", "out", "baseColor")}}}).empty());
	CHECK_FALSE(errors({{"nodes", {output, node("number", "n")}}, {"links", {link("n", "out", "shader")}}}).empty());
	CHECK_FALSE(errors({{"nodes", {output, node("number", "n"), node("number", "m")}},
	                    {"links", {link("n", "out", "roughness"), link("m", "out", "roughness")}}})
	                .empty());
	CHECK_FALSE(errors({{"nodes", {output, node("invert", "i"), node("invert", "j")}},
	                    {"links", {link("i", "j", "value"), link("j", "i", "value"), link("i", "out", "metallic")}}})
	                .empty());
	CHECK_FALSE(errors({{"nodes", {output, node("invert", "i")}}, {"links", {link("i", "i", "value")}}}).empty());
	json injected = node("number", "n");
	injected["value"] = "1.0; discard";
	const auto result =
	    compileMaterialGraph({{"nodes", {output, injected}}, {"links", {link("n", "out", "metallic")}}});
	CHECK_FALSE(result.ok());
	CHECK(result.glsl.empty());
	json many{{"nodes", {output}}};
	for(int i = 0; i < 300; ++i) many["nodes"].push_back(node("number", ("n" + std::to_string(i)).c_str()));
	CHECK_FALSE(errors(many).empty());
	CHECK_FALSE(errors(json::array()).empty());
}
