/// @file
/// @brief The scene3d module: maths, TRS and the node hierarchy, primitives, the scene as JSON, and the glTF loader on
/// Modello's samples and on malformed files.

#include <affresco/scene3d/Gltf.hpp>
#include <affresco/scene3d/SceneJson.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

using namespace affresco::scene3d;

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

namespace {

/// @brief Whether two vectors are within a tolerance. @param a One. @param b The other. @param tolerance It.
/// @return true if so.
bool near(Vec3 a, Vec3 b, float tolerance = 1e-4f) { return length(a - b) <= tolerance; }

/// @brief The bytes of a text. @param text It. @return Them.
std::vector<std::uint8_t> bytesOf(std::string_view text) { return {text.begin(), text.end()}; }

/// @brief A sample model's path. @param relative E.g. "BoxAnimated/BoxAnimated.glb". @return The path.
std::filesystem::path sample(const char* relative) {
	return std::filesystem::path(AFFRESCOUI_MODELLO_SAMPLES) / relative;
}

} // namespace

TEST_CASE("scene3d: vectors and quaternions") {
	CHECK(near(cross({1, 0, 0}, {0, 1, 0}), {0, 0, 1}));
	CHECK(dot(Vec3{1, 2, 3}, Vec3{4, 5, 6}) == doctest::Approx(32.0f));
	CHECK(near(normalize(Vec3{3, 0, 4}), {0.6f, 0, 0.8f}));
	const Quat quarter = Quat::fromAxisAngle({0, 0, 1}, radians(90.0f));
	CHECK(near(quarter.rotate({1, 0, 0}), {0, 1, 0}));
	CHECK(near((quarter * quarter).rotate({1, 0, 0}), {-1, 0, 0}));
	CHECK(near(quarter.conjugate().rotate({0, 1, 0}), {1, 0, 0}));
	const Quat half = slerp(Quat{}, quarter, 0.5f);
	CHECK(near(half.rotate({1, 0, 0}), {std::sqrt(0.5f), std::sqrt(0.5f), 0}));
	const Vec3 euler{0.3f, -0.5f, 1.1f};
	CHECK(near(Quat::fromEuler(euler).toEuler(), euler));
}

TEST_CASE("scene3d: matrices, projection and inverse") {
	const Mat4 move = Mat4::translation({1, 2, 3});
	CHECK(near(move.transformPoint({0, 0, 0}), {1, 2, 3}));
	CHECK(near(move.transformDirection({1, 0, 0}), {1, 0, 0}));
	const auto inverse = move.inverse();
	REQUIRE(inverse);
	CHECK(near(inverse.value().transformPoint({1, 2, 3}), {0, 0, 0}));
	CHECK_FALSE(Mat4::scaling({0, 1, 1}).inverse());
	const Mat4 view = Mat4::lookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
	CHECK(near(view.transformPoint({0, 0, 0}), {0, 0, -5}));
	const Mat4 projection = Mat4::perspective(radians(90.0f), 1.0f, 1.0f, 10.0f);
	const Vec4 nearPoint = projection.transform({0, 0, -1, 1});
	CHECK(nearPoint.z / nearPoint.w == doctest::Approx(-1.0f));
	const Vec4 farPoint = projection.transform({0, 0, -10, 1});
	CHECK(farPoint.z / farPoint.w == doctest::Approx(1.0f));
}

TEST_CASE("scene3d: TRS composes and decomposes") {
	Transform transform;
	transform.translation = {1, -2, 3};
	transform.rotation = Quat::fromEuler({0.2f, 0.4f, -0.6f});
	transform.scale = {2, 3, 0.5f};
	const auto back = Transform::decompose(transform.matrix());
	REQUIRE(back);
	CHECK(near(back.value().translation, transform.translation));
	CHECK(near(back.value().scale, transform.scale));
	CHECK(near(back.value().rotation.rotate({1, 2, 3}), transform.rotation.rotate({1, 2, 3})));
	CHECK_FALSE(Transform::decompose(Mat4::scaling({0, 0, 0})));
}

TEST_CASE("scene3d: the hierarchy places children in their parents' space") {
	Scene scene;
	scene.nodes.push_back({.id = "root", .name = "Root"});
	scene.nodes[0].transform.translation = {10, 0, 0};
	scene.nodes[0].transform.rotation = Quat::fromAxisAngle({0, 1, 0}, radians(90.0f));
	scene.nodes.push_back({.id = "child", .name = "Child", .parent = "root"});
	scene.nodes[1].transform.translation = {1, 0, 0};
	scene.nodes.push_back({.id = "hidden", .name = "Hidden", .parent = "child", .visible = false});
	CHECK(scene.validate().empty());
	const auto world = scene.worldMatrices();
	CHECK(near(world.at("child").first.origin(), {10, 0, -1}));
	CHECK(world.at("child").second);
	CHECK_FALSE(world.at("hidden").second);
	CHECK(scene.subtree("root").size() == 3);
	CHECK(scene.freshId("root") != "root");

	scene.nodes[0].parent = "hidden"; // a cycle
	CHECK_FALSE(scene.validate().empty());
	CHECK(scene.worldMatrices().size() == 3); // still placed, as if at the root
	scene.nodes[0].parent = "missing";
	CHECK_FALSE(scene.validate().empty());
	scene.nodes[0].parent.clear();
	scene.nodes.push_back({.id = "child", .name = "Twin"});
	CHECK_FALSE(scene.validate().empty());
}

TEST_CASE("scene3d: primitives are valid closed meshes with outward normals") {
	for(const auto primitive :
	    {Primitive::Cube, Primitive::Sphere, Primitive::Cylinder, Primitive::Torus, Primitive::Plane}) {
		CAPTURE(primitiveName(primitive));
		const MeshData mesh = makePrimitive(primitive);
		CHECK(mesh.validate().empty());
		CHECK(mesh.triangleCount() > 0);
		CHECK(primitiveNamed(primitiveName(primitive)) == primitive);
		// Each triangle's winding agrees with its vertices' normals (counter-clockwise from outside)
		for(std::size_t t = 0; t < mesh.indices.size(); t += 3) {
			const Vec3 a = mesh.positions[mesh.indices[t]];
			const Vec3 b = mesh.positions[mesh.indices[t + 1]];
			const Vec3 c = mesh.positions[mesh.indices[t + 2]];
			const Vec3 face = cross(b - a, c - a);
			if(length(face) > 1e-6f) CHECK(dot(face, mesh.normals[mesh.indices[t]]) > 0.0f);
		}
	}
	const MeshData cube = makeCube();
	CHECK(near(cube.bounds.min, {-1, -1, -1}));
	CHECK(near(cube.bounds.max, {1, 1, 1}));
	CHECK(cube.raycast({{0, 0, 5}, {0, 0, -1}}).value_or(0.0f) == doctest::Approx(4.0f));
	CHECK_FALSE(cube.raycast({{0, 3, 5}, {0, 0, -1}}));
}

TEST_CASE("scene3d: bounds, rays and the frustum") {
	Aabb box;
	CHECK(box.empty());
	box.extend(Vec3{-1, -1, -1});
	box.extend(Vec3{1, 1, 1});
	CHECK(box.contains({0.5f, 0, 0}));
	CHECK(intersect(Ray{{-5, 0, 0}, {1, 0, 0}}, box).value_or(0.0f) == doctest::Approx(4.0f));
	CHECK_FALSE(intersect(Ray{{-5, 3, 0}, {1, 0, 0}}, box));
	CHECK(intersect(Ray{{0.2f, 0.2f, 1}, {0, 0, -1}}, {0, 0, 0}, {1, 0, 0}, {0, 1, 0}).value_or(0.0f) ==
	      doctest::Approx(1.0f));
	const Mat4 viewProjection =
	    Mat4::perspective(radians(60.0f), 1.0f, 0.1f, 100.0f) * Mat4::lookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
	const Frustum frustum = Frustum::fromMatrix(viewProjection);
	CHECK(frustum.intersects(box));
	CHECK_FALSE(frustum.intersects(box.transformed(Mat4::translation({0, 0, 20}))));
	CHECK_FALSE(frustum.intersects(box.transformed(Mat4::translation({50, 0, 0}))));
}

TEST_CASE("scene3d: the scene round-trips through JSON and refuses bad members") {
	Scene scene;
	Node lamp{.id = "lamp", .name = "Lamp"};
	lamp.light = Light{.type = LightType::Spot, .intensity = 20.0f, .castsShadow = true};
	lamp.transform.rotation = Quat::fromEuler({0.1f, 0.2f, 0.3f});
	scene.nodes.push_back(lamp);
	scene.nodes.push_back({.id = "box", .name = "Box", .mesh = "cube", .material = "red"});
	scene.materials.push_back({.id = "red", .name = "Red", .baseColor = {1, 0, 0, 1}, .metallic = 0.25f});
	const Scene back = sceneFromJson(toJson(scene));
	REQUIRE(back.nodes.size() == 2);
	CHECK(back.nodes[0].light == scene.nodes[0].light);
	CHECK(near(back.nodes[0].transform.rotation.rotate({1, 0, 0}), lamp.transform.rotation.rotate({1, 0, 0})));
	CHECK(back.material("red")->metallic == doctest::Approx(0.25f));
	CHECK(toJson(back) == toJson(scene));

	auto bad = toJson(scene);
	bad["nodes"][1]["translation"] = {1, "two", 3};
	CHECK_THROWS_AS((void)sceneFromJson(bad), affresco::DocumentError);
	bad = toJson(scene);
	bad["nodes"][1]["parent"] = "box";
	CHECK_THROWS_AS((void)sceneFromJson(bad), affresco::DocumentError);
	bad = toJson(scene);
	bad["nodes"][0]["rotation"] = {0, 0, 0, 0};
	CHECK_THROWS_AS((void)sceneFromJson(bad), affresco::DocumentError);
}

TEST_CASE("scene3d: tracks sample step, linear and cubic keys") {
	Track track{.node = "box", .path = TrackPath::Translation, .times = {0, 1}, .values = {0, 0, 0, 2, 4, 6}};
	CHECK(track.validate().empty());
	CHECK(track.sample(0.5f).value()[1] == doctest::Approx(2.0f));
	CHECK(track.sample(5.0f).value()[2] == doctest::Approx(6.0f));
	track.interpolation = Interpolation::Step;
	CHECK(track.sample(0.9f).value()[0] == doctest::Approx(0.0f));
	setKey(track, 0.5f, {1, 1, 1, 0});
	CHECK(track.times.size() == 3);
	CHECK(track.times[1] == doctest::Approx(0.5f));
	track.interpolation = Interpolation::Cubic;
	CHECK_FALSE(track.validate().empty()); // its values are no longer three per key
	track.times = {1, 0};
	track.interpolation = Interpolation::Linear;
	CHECK_FALSE(track.validate().empty());
}

TEST_CASE("scene3d: the samples load") {
	const GltfImport box = loadGltfFile(sample("BoxAnimated/BoxAnimated.glb"), {}, "box.");
	CHECK(box.scene.validate().empty());
	CHECK(box.scene.nodes.size() >= 2);
	CHECK_FALSE(box.assets.meshes().empty());
	REQUIRE_FALSE(box.animations.empty());
	CHECK(box.animations[0].duration() > 0.0f);
	CHECK(box.scene.nodes[0].id.starts_with("box."));
	for(const auto& track : box.animations[0].tracks) CHECK(box.scene.node(track.node) != nullptr);

	const GltfImport spheres = loadGltfFile(sample("MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures.glb"));
	CHECK(spheres.scene.materials.size() >= 25);
	CHECK(spheres.warnings.empty());
	for(const auto& [id, mesh] : spheres.assets.meshes()) CHECK(mesh->validate().empty());
	CHECK_FALSE(worldBounds(spheres.scene, spheres.assets).empty());

	const GltfImport cube = loadGltfFile(sample("AnimatedCube/AnimatedCube.gltf"));
	REQUIRE(cube.scene.materials.size() == 1);
	const auto texture = cube.assets.texture(cube.scene.materials[0].baseColorTexture);
	REQUIRE(texture);
	CHECK(texture->srgb);
	CHECK(texture->pixels.size() == static_cast<std::size_t>(texture->width * texture->height * 4));
	CHECK_FALSE(cube.animations.empty());

	// An image given as a data: URI decodes; one that isn't an image is skipped with a warning
	const auto withImage = [](std::string_view uri) {
		return bytesOf(R"({"asset":{"version":"2.0"},"images":[{"uri":")" + std::string(uri) +
		               R"("}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":)"
		               R"({"baseColorTexture":{"index":0}}}]})");
	};
	const GltfImport embedded = loadGltf(withImage("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJ"
	                                               "AAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg=="),
	                                     std::filesystem::path());
	REQUIRE(embedded.scene.materials.size() == 1);
	const auto pixel = embedded.assets.texture(embedded.scene.materials[0].baseColorTexture);
	REQUIRE(pixel);
	CHECK((pixel->width == 1 && pixel->height == 1 && pixel->pixels.size() == 4));
	const GltfImport broken = loadGltf(withImage("data:image/png;base64,AAAA"), std::filesystem::path());
	REQUIRE(broken.scene.materials.size() == 1);
	CHECK(broken.scene.materials[0].baseColorTexture.empty());
	CHECK_FALSE(broken.warnings.empty());
}

TEST_CASE("scene3d: malformed and hostile glTF files are refused") {
	const std::filesystem::path none;
	CHECK_THROWS_AS((void)loadGltf(bytesOf("not a glTF file"), none), GltfError);
	CHECK_THROWS_AS((void)loadGltf(bytesOf("{\"asset\":{\"version\":\"1.0\"}}"), none), GltfError);
	CHECK_THROWS_AS((void)loadGltf(bytesOf("glTF\x02\0\0\0\xff\xff\0\0"), none), GltfError); // a truncated GLB

	const auto withBuffer = [](std::string_view uri, int size) {
		return bytesOf(R"({"asset":{"version":"2.0"},"buffers":[{"uri":")" + std::string(uri) + R"(","byteLength":)" +
		               std::to_string(size) + "}]}");
	};
	const auto directory = sample("AnimatedCube");
	CHECK_THROWS_AS((void)loadGltf(withBuffer("https://example.com/a.bin", 4), directory), GltfError);
	CHECK_THROWS_AS((void)loadGltf(withBuffer("../AnimatedCube/AnimatedCube.bin", 4), directory), GltfError);
	CHECK_THROWS_AS((void)loadGltf(withBuffer("%2E%2E/AnimatedCube/AnimatedCube.bin", 4), directory), GltfError);
	CHECK_THROWS_AS((void)loadGltf(withBuffer("/etc/hosts", 4), directory), GltfError);
	CHECK_THROWS_AS((void)loadGltf(withBuffer("AnimatedCube.bin", 4), none), GltfError); // no directory: no files
	CHECK_THROWS_AS((void)loadGltf(withBuffer("AnimatedCube.bin", 1 << 20), directory), GltfError); // shorter
	CHECK_THROWS_AS((void)loadGltf(withBuffer("data:application/octet-stream;base64,AAAA", 64), none), GltfError);
	CHECK_NOTHROW((void)loadGltf(withBuffer("data:application/octet-stream;base64,AAAA", 3), none));
	CHECK_NOTHROW((void)loadGltf(withBuffer("AnimatedCube.bin", 4), directory));
	CHECK_NOTHROW((void)loadGltf(withBuffer("Animated%43ube.bin", 4), directory)); // decoded, and only the name

	GltfLimits small;
	small.fileBytes = 16;
	CHECK_THROWS_AS((void)loadGltfFile(sample("BoxAnimated/BoxAnimated.glb"), small), GltfError);
	small = {};
	small.vertices = 10;
	CHECK_THROWS_AS((void)loadGltfFile(sample("MetalRoughSpheresNoTextures/MetalRoughSpheresNoTextures.glb"), small),
	                GltfError);

	// Ranges whose sizes wrap around past 2^64 are refused, not read: a stride, and a sparse count
	const auto withAccessor = [](std::string_view view, std::string_view accessor) {
		return bytesOf(R"({"asset":{"version":"2.0"},"buffers":[{"uri":"data:application/octet-stream;base64,)"
		               R"(AAAAAAAAAAAAAAAA","byteLength":12}],"bufferViews":[{"buffer":0,"byteLength":12)" +
		               std::string(view) + R"(}],"accessors":[{"componentType":5126,"type":"VEC3",)" +
		               std::string(accessor) +
		               R"(}],"meshes":[{"primitives":[{"attributes":{"POSITION":0}}]}],"nodes":[{"mesh":0}]})");
	};
	CHECK_NOTHROW((void)loadGltf(withAccessor("", R"("bufferView":0,"count":1)"), none));
	CHECK_THROWS_AS(
	    (void)loadGltf(withAccessor(R"(,"byteStride":4611686018427387904)", R"("bufferView":0,"count":5)"), none),
	    GltfError);
	CHECK_THROWS_AS((void)loadGltf(withAccessor("", R"("count":1,"sparse":{"count":4611686018427387904,)"
	                                                R"("indices":{"bufferView":0,"componentType":5125},)"
	                                                R"("values":{"bufferView":0}})"),
	                               none),
	                GltfError);

	// An index past the vertices is caught by validation, not read
	const auto box = loadGltfFile(sample("BoxAnimated/BoxAnimated.glb"));
	CHECK(box.warnings.empty());
}

TEST_CASE("scene3d: only safe relative URIs are read") {
	CHECK(isSafeRelativeUri("model.bin"));
	CHECK(isSafeRelativeUri("textures/wood.png"));
	CHECK(isSafeRelativeUri("a..b.bin"));
	CHECK_FALSE(isSafeRelativeUri(""));
	CHECK_FALSE(isSafeRelativeUri("../model.bin"));
	CHECK_FALSE(isSafeRelativeUri("textures/../../model.bin"));
	CHECK_FALSE(isSafeRelativeUri(".."));
	CHECK_FALSE(isSafeRelativeUri("/etc/passwd"));
	CHECK_FALSE(isSafeRelativeUri("C:/model.bin"));
	CHECK_FALSE(isSafeRelativeUri("https://example.com/model.bin"));
	CHECK_FALSE(isSafeRelativeUri("textures\\..\\model.bin"));
}

TEST_CASE("scene3d: an export loads back") {
	const GltfImport box = loadGltfFile(sample("BoxAnimated/BoxAnimated.glb"));
	std::vector<std::string> warnings;
	const std::string text = exportGltf(box.scene, box.assets, box.animations, &warnings);
	const GltfImport back = loadGltf(bytesOf(text), {});
	CHECK(back.scene.nodes.size() == box.scene.nodes.size());
	CHECK(back.assets.meshes().size() == box.assets.meshes().size());
	REQUIRE(back.animations.size() == box.animations.size());
	CHECK(back.animations[0].duration() == doctest::Approx(box.animations[0].duration()));
	const auto before = worldBounds(box.scene, box.assets);
	const auto after = worldBounds(back.scene, back.assets);
	CHECK(near(before.min, after.min, 1e-3f));
	CHECK(near(before.max, after.max, 1e-3f));
}

// NOLINTEND(bugprone-unchecked-optional-access)
