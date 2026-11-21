/// @file
/// @brief Expression functions of polygon meshes: what a modeller's edit mode does to the meshes a "scene" node's
/// `"meshes"` list holds ({vertices, faces, smooth}), each giving a new mesh (scene3d's PolyMesh.hpp); and of node
/// transforms, composed and taken into a parent's space.

#include "ExpressionFunctions.hpp"

#include "affresco/scene3d/Gltf.hpp"
#include "affresco/scene3d/PolyMesh.hpp"
#include "affresco/scene3d/SceneJson.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format::detail {
namespace {

using scene3d::PolyMesh;
using scene3d::Vec3;

/// @brief A mesh argument. @param call The call. @param index Its position. @return The mesh.
/// @throws DocumentError If it isn't a mesh.
PolyMesh meshAt(const Call& call, std::size_t index) {
	try {
		PolyMesh mesh = scene3d::polyMeshFromJson(call[index].toJson());
		call.spend(mesh.vertices.size() + mesh.faces.size());
		return mesh;
	} catch(const DocumentError& error) {
		call.fail(error.what());
	}
}

/// @brief A list of indices argument (a single number counts as a list of one). @param call The call.
/// @param index Its position. @return The non-negative integers it holds.
std::vector<std::uint32_t> indicesAt(const Call& call, std::size_t index) {
	std::vector<std::uint32_t> result;
	const Value& value = call[index];
	const auto add = [&](const Value& item) {
		if(item.isNumber() && item.number() >= 0 && item.number() < 4e9)
			result.push_back(static_cast<std::uint32_t>(item.number()));
	};
	if(value.isList())
		for(std::size_t i = 0; i < value.size(); ++i) add(value.item(i));
	else add(value);
	return result;
}

/// @brief A point argument [x, y, z]. @param call The call. @param index Its position. @return It.
Vec3 pointAt(const Call& call, std::size_t index) {
	const Value& value = call[index];
	if(!value.isList() || value.size() != 3) call.fail("expected [x, y, z]");
	Vec3 point;
	for(int axis = 0; axis < 3; ++axis) {
		const Value item = value.item(static_cast<std::size_t>(axis));
		if(!item.isNumber() || !std::isfinite(item.number())) call.fail("expected [x, y, z]");
		point[axis] = static_cast<float>(item.number());
	}
	return point;
}

/// @brief An optional number argument. @param call The call. @param index Its position. @param fallback If absent.
/// @return It.
float numberOr(const Call& call, std::size_t index, float fallback) {
	if(index >= call.size() || call[index].isNull()) return fallback;
	const double read = call.number(index);
	return std::isfinite(read) ? static_cast<float>(read) : fallback;
}

/// @brief A mesh result. @param mesh It. @return The value.
Value result(const PolyMesh& mesh) { return Value::ownJson(scene3d::toJson(mesh)); }

/// @brief An axis argument: 'x', 'y', 'z' or 0 to 2. @param call The call. @param index Its position. @return 0 to 2.
int axisAt(const Call& call, std::size_t index) {
	const Value& value = call[index];
	if(value.isText() && value.text().size() == 1 && value.text()[0] >= 'x' && value.text()[0] <= 'z')
		return value.text()[0] - 'x';
	if(value.isNumber()) return std::clamp(static_cast<int>(value.number()), 0, 2);
	call.fail("expected 'x', 'y' or 'z'");
}

/// @brief meshPrimitive(name): a primitive as polygons. @param call The call. @return The mesh, or null.
Value meshPrimitive(const Call& call) {
	const auto primitive = scene3d::primitiveNamed(call[0].text());
	return primitive ? result(scene3d::polyPrimitive(*primitive)) : Value{};
}

/// @brief meshExtrude(mesh, faces, distance). @param call The call. @return The mesh.
Value meshExtrude(const Call& call) {
	return result(scene3d::extrudeFaces(meshAt(call, 0), indicesAt(call, 1), numberOr(call, 2, 0.5f)));
}

/// @brief meshInset(mesh, faces, amount). @param call The call. @return The mesh.
Value meshInset(const Call& call) {
	return result(scene3d::insetFaces(meshAt(call, 0), indicesAt(call, 1), numberOr(call, 2, 0.25f)));
}

/// @brief meshDeleteFaces(mesh, faces). @param call The call. @return The mesh.
Value meshDeleteFaces(const Call& call) { return result(scene3d::deleteFaces(meshAt(call, 0), indicesAt(call, 1))); }

/// @brief meshDeleteVertices(mesh, vertices). @param call The call. @return The mesh.
Value meshDeleteVertices(const Call& call) {
	return result(scene3d::deleteVertices(meshAt(call, 0), indicesAt(call, 1)));
}

/// @brief meshLoopCut(mesh, [a, b]). @param call The call. @return The mesh.
Value meshLoopCut(const Call& call) {
	const std::vector<std::uint32_t> edge = indicesAt(call, 1);
	if(edge.size() != 2) call.fail("expected an edge [a, b]");
	return result(scene3d::loopCut(meshAt(call, 0), edge[0], edge[1]));
}

/// @brief meshMove(mesh, vertices, [dx, dy, dz], radius?). @param call The call. @return The mesh.
Value meshMove(const Call& call) {
	return result(scene3d::moveVertices(meshAt(call, 0), indicesAt(call, 1), pointAt(call, 2), numberOr(call, 3, 0)));
}

/// @brief meshScale(mesh, vertices, factor, radius?). @param call The call. @return The mesh.
Value meshScale(const Call& call) {
	return result(
	    scene3d::scaleVertices(meshAt(call, 0), indicesAt(call, 1), numberOr(call, 2, 1), numberOr(call, 3, 0)));
}

/// @brief meshFaceVertices(mesh, faces). @param call The call. @return The vertex indices.
Value meshFaceVertices(const Call& call) {
	json indices = json::array();
	for(const std::uint32_t v : scene3d::faceVertices(meshAt(call, 0), indicesAt(call, 1))) indices.push_back(v);
	return Value::ownJson(std::move(indices));
}

/// @brief meshFaces(mesh, faces, offset?): those faces alone, pushed out along their normals (to draw a highlight).
/// @param call The call. @return The mesh.
Value meshFaces(const Call& call) {
	const PolyMesh mesh = meshAt(call, 0);
	const float offset = numberOr(call, 2, 0.0f);
	PolyMesh part;
	for(const std::uint32_t f : indicesAt(call, 1)) {
		if(f >= mesh.faces.size()) continue;
		const Vec3 normal = scene3d::faceNormal(mesh, f);
		std::vector<std::uint32_t> face;
		for(const std::uint32_t v : mesh.faces[f]) {
			face.push_back(static_cast<std::uint32_t>(part.vertices.size()));
			part.vertices.push_back(mesh.vertices[v] + normal * offset);
		}
		part.faces.push_back(std::move(face));
	}
	return result(part);
}

/// @brief meshCentre(mesh, vertices): the middle of some vertices. @param call The call. @return [x, y, z], or null
/// when none is the mesh's.
Value meshCentre(const Call& call) {
	const PolyMesh mesh = meshAt(call, 0);
	Vec3 sum;
	std::size_t count = 0;
	for(const std::uint32_t v : indicesAt(call, 1))
		if(v < mesh.vertices.size()) {
			sum = sum + mesh.vertices[v];
			++count;
		}
	if(count == 0) return {};
	const Vec3 centre = sum / static_cast<float>(count);
	const auto round = [](float x) { return std::round(static_cast<double>(x) * 1e5) / 1e5; };
	return Value::ownJson(json::array({round(centre.x), round(centre.y), round(centre.z)}));
}

/// @brief meshModifiers(mesh, modifiers): a stack of modifiers applied in order: {type: 'mirror', axis},
/// {type: 'array', count, offset} and {type: 'subdivision', levels}; others are skipped. @param call The call.
/// @return The mesh.
Value meshModifiers(const Call& call) {
	PolyMesh mesh = meshAt(call, 0);
	const Value& stack = call[1];
	for(std::size_t i = 0; stack.isList() && i < stack.size() && i < 16; ++i) {
		const Value modifier = stack.item(i);
		const std::optional<Value> type = modifier.member("type");
		if(!type || !type->isText()) continue;
		const auto number = [&](const char* key, double fallback) {
			const std::optional<Value> v = modifier.member(key);
			return v && v->isNumber() && std::isfinite(v->number()) ? v->number() : fallback;
		};
		if(type->text() == "mirror") {
			const std::optional<Value> axis = modifier.member("axis");
			const std::string_view name = axis && axis->isText() ? axis->text() : "x";
			mesh = scene3d::mirrorMesh(mesh, name == "y" ? 1 : name == "z" ? 2 : 0);
		} else if(type->text() == "array") {
			Vec3 offset{2.0f, 0.0f, 0.0f};
			if(const std::optional<Value> given = modifier.member("offset");
			   given && given->isList() && given->size() == 3)
				for(int axis = 0; axis < 3; ++axis)
					offset[axis] = static_cast<float>(given->item(static_cast<std::size_t>(axis)).number());
			mesh = scene3d::arrayMesh(mesh, std::clamp(static_cast<int>(number("count", 2)), 1, 64), offset);
		} else if(type->text() == "subdivision") {
			const bool smooth = mesh.smooth;
			mesh = scene3d::subdivide(mesh, std::clamp(static_cast<int>(number("levels", 1)), 0, 3));
			mesh.smooth = smooth || number("levels", 1) > 0;
		}
		call.spend(mesh.vertices.size() + mesh.faces.size());
	}
	return result(mesh);
}

/// @brief meshEdges(mesh, thickness, edges?): thin bars along a mesh's edges (all, or a list of [a, b]).
/// @param call The call. @return The bars, a mesh.
Value meshEdges(const Call& call) {
	const PolyMesh mesh = meshAt(call, 0);
	std::vector<std::array<std::uint32_t, 2>> edges;
	if(call.size() > 2 && call[2].isList())
		for(std::size_t i = 0; i < call[2].size(); ++i) {
			const Value edge = call[2].item(i);
			if(edge.isList() && edge.size() == 2 && edge.item(0).isNumber() && edge.item(1).isNumber() &&
			   edge.item(0).number() >= 0 && edge.item(1).number() >= 0)
				edges.push_back({static_cast<std::uint32_t>(edge.item(0).number()),
				                 static_cast<std::uint32_t>(edge.item(1).number())});
		}
	if(call.size() > 2 && call[2].isList() && edges.empty()) return result(PolyMesh{});
	const PolyMesh bars = scene3d::edgeBars(mesh, edges, numberOr(call, 1, 0.01f));
	call.spend(bars.vertices.size());
	return result(bars);
}

/// @brief meshFlip(mesh). @param call The call. @return The mesh.
Value meshFlip(const Call& call) { return result(scene3d::flipFaces(meshAt(call, 0))); }

/// @brief meshMirror(mesh, axis). @param call The call. @return The mesh.
Value meshMirror(const Call& call) { return result(scene3d::mirrorMesh(meshAt(call, 0), axisAt(call, 1))); }

/// @brief meshArray(mesh, count, [dx, dy, dz]). @param call The call. @return The mesh.
Value meshArray(const Call& call) {
	const PolyMesh mesh = meshAt(call, 0);
	const int count = std::clamp(static_cast<int>(numberOr(call, 1, 1)), 1, 64);
	call.spend((mesh.vertices.size() + mesh.faces.size()) * static_cast<std::size_t>(count));
	return result(scene3d::arrayMesh(mesh, count, pointAt(call, 2)));
}

/// @brief meshSubdivide(mesh, levels). @param call The call. @return The mesh.
Value meshSubdivide(const Call& call) {
	const PolyMesh mesh = meshAt(call, 0);
	const int levels = std::clamp(static_cast<int>(numberOr(call, 1, 1)), 0, 4);
	PolyMesh smoothed = scene3d::subdivide(mesh, levels);
	call.spend(smoothed.vertices.size() + smoothed.faces.size());
	smoothed.smooth = true;
	return result(smoothed);
}

/// @brief A node's transform, from {translation, rotation | rotationDegrees, scale}; missing parts take their defaults.
/// @param node The node, or anything else for none. @return The transform.
scene3d::Transform transformOf(const Value& node) {
	scene3d::Transform transform;
	if(!node.isObject()) return transform;
	const auto vector = [&](const char* key, Vec3 fallback) {
		const std::optional<Value> v = node.member(key);
		if(!v || !v->isList() || v->size() != 3) return fallback;
		Vec3 read;
		for(int axis = 0; axis < 3; ++axis)
			read[axis] = static_cast<float>(v->item(static_cast<std::size_t>(axis)).number());
		return read;
	};
	transform.translation = vector("translation", {});
	transform.scale = vector("scale", {1.0f, 1.0f, 1.0f});
	if(const std::optional<Value> q = node.member("rotation"); q && q->isList() && q->size() == 4) {
		transform.rotation = scene3d::normalize(
		    scene3d::Quat{static_cast<float>(q->item(0).number()), static_cast<float>(q->item(1).number()),
			              static_cast<float>(q->item(2).number()), static_cast<float>(q->item(3).number())});
	}
	if(node.member("rotationDegrees")) {
		const Vec3 angles = vector("rotationDegrees", {});
		transform.rotation = scene3d::Quat::fromEuler(
		    {scene3d::radians(angles.x), scene3d::radians(angles.y), scene3d::radians(angles.z)});
	}
	return transform;
}

/// @brief A world point in a node's own space. @param node The node {translation, rotation | rotationDegrees, scale},
/// or null. @param point The point. @return In the node's space.
Vec3 intoNode(const Value& node, Vec3 point) {
	if(!node.isObject()) return point;
	const scene3d::Transform transform = transformOf(node);
	const Vec3 local = transform.rotation.conjugate().rotate(point - transform.translation);
	const auto safe = [](float s) { return std::abs(s) > 1e-6f ? s : 1e-6f; };
	return {local.x / safe(transform.scale.x), local.y / safe(transform.scale.y), local.z / safe(transform.scale.z)};
}

/// @brief meshPick(mesh, point, 'vertex' | 'edge' | 'face', node?): what of the mesh is at a picked world point.
/// @param call The call. @return A vertex index, an edge [a, b], a face index, or null.
Value meshPick(const Call& call) {
	const PolyMesh mesh = meshAt(call, 0);
	const Vec3 point = intoNode(call.size() > 3 ? call[3] : Value{}, pointAt(call, 1));
	const std::string_view mode = call[2].text();
	if(mode == "edge") {
		const auto edge = scene3d::nearestEdge(mesh, point);
		return edge ? Value::ownJson(json::array({(*edge)[0], (*edge)[1]})) : Value{};
	}
	const auto found = mode == "face" ? scene3d::nearestFace(mesh, point) : scene3d::nearestVertex(mesh, point);
	return found ? Value::fromInteger(*found) : Value{};
}

/// @brief A transform as {translation, rotationDegrees, scale}, rounded. @param transform It. @return The value.
Value transformValue(const scene3d::Transform& transform) {
	const auto round = [](float x) { return std::round(static_cast<double>(x) * 1e4) / 1e4; };
	const auto list = [&](Vec3 v) { return json::array({round(v.x), round(v.y), round(v.z)}); };
	const Vec3 angles = transform.rotation.toEuler();
	return Value::ownJson(json{
	    {"translation", list(transform.translation)},
	    {"rotationDegrees", list({scene3d::degrees(angles.x), scene3d::degrees(angles.y), scene3d::degrees(angles.z)})},
	    {"scale", list(transform.scale)}});
}

/// @brief transformCompose(outer, inner): a child's transform in its parent's space taken out to the parent's.
/// @param call The call. @return {translation, rotationDegrees, scale}, or null when it can't be split (a shear).
Value transformCompose(const Call& call) {
	const scene3d::Mat4 matrix = transformOf(call[0]).matrix() * transformOf(call[1]).matrix();
	const auto split = scene3d::Transform::decompose(matrix);
	return split ? transformValue(*split) : Value{};
}

/// @brief transformRelative(transform, outer): a transform taken into another's space (what a new parent keeps).
/// @param call The call. @return {translation, rotationDegrees, scale}, or null when it can't be.
Value transformRelative(const Call& call) {
	const scene3d::Mat4 outer = transformOf(call[1]).matrix();
	const auto inverse = outer.inverse();
	if(!inverse) return {};
	const auto split = scene3d::Transform::decompose(*inverse * transformOf(call[0]).matrix());
	return split ? transformValue(*split) : Value{};
}

/// @brief gltfScene(gltf, prefix): a .gltf file's JSON (buffers and images as data: URIs, never other files) as a
/// scene's {nodes, meshes, materials}, the meshes editable, every id prefixed; textures and animations are left out.
/// @param call The call. @return The scene part, or null when the file can't be loaded.
Value gltfScene(const Call& call) {
	scene3d::GltfLimits limits;
	limits.fileBytes = std::size_t{16} << 20;
	limits.bufferBytes = std::size_t{32} << 20;
	limits.nodes = 1024;
	limits.meshes = 256;
	limits.materials = 128;
	limits.vertices = 65'536;
	limits.indices = 196'608;
	limits.images = 16;
	limits.imageSide = 2048;
	limits.animations = 0;
	limits.depth = 32;
	const std::string text = call[0].isObject() ? call[0].toJson().dump() : std::string();
	if(text.empty() || text.size() > limits.fileBytes) return {};
	call.spend(text.size() / 64);
	const std::string prefix(call.size() > 1 ? call[1].text() : std::string_view());
	try {
		const scene3d::GltfImport import =
		    scene3d::loadGltf({reinterpret_cast<const std::uint8_t*>(text.data()), text.size()}, {}, limits, prefix);
		json nodes = json::array(), meshes = json::array(), materials = json::array();
		std::set<std::string, std::less<>> placed;
		for(const scene3d::Node& node : import.scene.nodes) {
			json value = scene3d::toJson(node);
			const Vec3 angles = node.transform.rotation.toEuler();
			value["rotationDegrees"] =
			    json::array({scene3d::degrees(angles.x), scene3d::degrees(angles.y), scene3d::degrees(angles.z)});
			value.erase("rotation");
			nodes.push_back(std::move(value));
			if(node.mesh.empty() || !placed.insert(node.mesh).second) continue;
			if(const auto mesh = import.assets.mesh(node.mesh)) {
				json poly = scene3d::toJson(scene3d::polyFromMesh(*mesh));
				call.spend(mesh->positions.size());
				poly["id"] = node.mesh;
				meshes.push_back(std::move(poly));
			}
		}
		for(const scene3d::Material& material : import.scene.materials) {
			json value = scene3d::toJson(material);
			value.erase("textures");
			materials.push_back(std::move(value));
		}
		return Value::ownJson(
		    json{{"nodes", std::move(nodes)}, {"meshes", std::move(meshes)}, {"materials", std::move(materials)}});
	} catch(const std::exception&) {
		return {};
	}
}

/// @brief meshStats(mesh): {vertices, edges, faces}, counted. @param call The call. @return The counts.
Value meshStats(const Call& call) {
	const PolyMesh mesh = meshAt(call, 0);
	return Value::ownJson(
	    json{{"vertices", mesh.vertices.size()}, {"edges", mesh.edges().size()}, {"faces", mesh.faces.size()}});
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "meshPrimitive",
	 .arguments = "name",
	 .about = "A primitive ('cube', 'plane', 'sphere', 'cylinder', 'cone', 'torus') as an editable mesh "
	          "{ vertices, faces, smooth }; null for another name",
	 .fewest = 1,
	 .most = 1,
	 .apply = meshPrimitive},
    {.name = "meshExtrude",
	 .arguments = "mesh, faces, distance",
	 .about = "The mesh with a region of faces moved out along their normals, walls joining their outline",
	 .fewest = 2,
	 .most = 3,
	 .apply = meshExtrude},
    {.name = "meshInset",
	 .arguments = "mesh, faces, amount",
	 .about = "The mesh with each face inset (amount 0 to 1 towards its centre), a ring of quads around it",
	 .fewest = 2,
	 .most = 3,
	 .apply = meshInset},
    {.name = "meshDeleteFaces",
	 .arguments = "mesh, faces",
	 .about = "The mesh without those faces, nor the vertices no face uses then",
	 .fewest = 2,
	 .most = 2,
	 .apply = meshDeleteFaces},
    {.name = "meshDeleteVertices",
	 .arguments = "mesh, vertices",
	 .about = "The mesh without those vertices and every face using one",
	 .fewest = 2,
	 .most = 2,
	 .apply = meshDeleteVertices},
    {.name = "meshLoopCut",
	 .arguments = "mesh, edge",
	 .about = "The mesh with the ring of quads across an edge [a, b] cut in two at their middles",
	 .fewest = 2,
	 .most = 2,
	 .apply = meshLoopCut},
    {.name = "meshMove",
	 .arguments = "mesh, vertices, delta, radius?",
	 .about = "The mesh with vertices moved by [dx, dy, dz]; with a radius, the ones near them too, less the further "
	          "(proportional editing)",
	 .fewest = 3,
	 .most = 4,
	 .apply = meshMove},
    {.name = "meshScale",
	 .arguments = "mesh, vertices, factor, radius?",
	 .about = "The mesh with vertices scaled about their centre; with a radius, the ones near them too",
	 .fewest = 3,
	 .most = 4,
	 .apply = meshScale},
    {.name = "meshFaceVertices",
	 .arguments = "mesh, faces",
	 .about = "The vertex indices of some faces, each once",
	 .fewest = 2,
	 .most = 2,
	 .apply = meshFaceVertices},
    {.name = "meshFaces",
	 .arguments = "mesh, faces, offset?",
	 .about = "A mesh of those faces alone, pushed out along their normals by offset (a selection to draw)",
	 .fewest = 2,
	 .most = 3,
	 .apply = meshFaces},
    {.name = "meshCentre",
	 .arguments = "mesh, vertices",
	 .about = "The middle [x, y, z] of some vertices of a mesh; null when it has none of them",
	 .fewest = 2,
	 .most = 2,
	 .apply = meshCentre},
    {.name = "meshModifiers",
	 .arguments = "mesh, modifiers",
	 .about = "The mesh with a stack of modifiers applied in order: { type: 'mirror', axis }, { type: 'array', count, "
	          "offset }, { type: 'subdivision', levels }",
	 .fewest = 2,
	 .most = 2,
	 .apply = meshModifiers},
    {.name = "meshEdges",
	 .arguments = "mesh, thickness, edges?",
	 .about = "Thin bars along a mesh's edges (every one, or a list of [a, b]): a wireframe to draw in any material",
	 .fewest = 2,
	 .most = 3,
	 .apply = meshEdges},
    {.name = "meshFlip",
	 .arguments = "mesh",
	 .about = "The mesh with every face turned the other way (its normals flipped)",
	 .fewest = 1,
	 .most = 1,
	 .apply = meshFlip},
    {.name = "meshMirror",
	 .arguments = "mesh, axis",
	 .about = "The mesh and its mirror image across the plane through the origin normal to 'x', 'y' or 'z'",
	 .fewest = 2,
	 .most = 2,
	 .apply = meshMirror},
    {.name = "meshArray",
	 .arguments = "mesh, count, offset",
	 .about = "Count copies of the mesh (1 to 64), each moved by [dx, dy, dz] from the last",
	 .fewest = 3,
	 .most = 3,
	 .apply = meshArray},
    {.name = "meshSubdivide",
	 .arguments = "mesh, levels",
	 .about = "The mesh subdivided (Catmull-Clark, 0 to 4 levels), shaded smooth",
	 .fewest = 2,
	 .most = 2,
	 .apply = meshSubdivide},
    {.name = "meshPick",
	 .arguments = "mesh, point, mode, node?",
	 .about = "What is at a picked world point: the nearest vertex, edge [a, b] or face by mode ('vertex', 'edge', "
	          "'face'), the point taken into the node's { translation, rotation, scale } first",
	 .fewest = 3,
	 .most = 4,
	 .apply = meshPick},
    {.name = "meshStats",
	 .arguments = "mesh",
	 .about = "{ vertices, edges, faces }: how many a mesh has",
	 .fewest = 1,
	 .most = 1,
	 .apply = meshStats},
    {.name = "gltfScene",
	 .arguments = "gltf, prefix",
	 .about = "A .gltf file's JSON (its buffers as data: URIs) as a scene's { nodes, meshes, materials }, the meshes "
	          "editable, every id prefixed; null when it can't be loaded",
	 .fewest = 1,
	 .most = 2,
	 .apply = gltfScene},
    {.name = "transformCompose",
	 .arguments = "outer, inner",
	 .about = "The inner { translation, rotation or rotationDegrees, scale } taken out of the outer's space: a "
	          "child's transform in its parent's, as { translation, rotationDegrees, scale }",
	 .fewest = 2,
	 .most = 2,
	 .apply = transformCompose},
    {.name = "transformRelative",
	 .arguments = "transform, outer",
	 .about = "A transform taken into the outer's space (what keeps a child in place under a new parent); null if "
	          "it can't be",
	 .fewest = 2,
	 .most = 2,
	 .apply = transformRelative},
});

} // namespace

std::span<const Function> meshFunctions() { return table; }

} // namespace affresco::format::detail
