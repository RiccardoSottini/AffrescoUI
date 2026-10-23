/// @file
/// @brief Scenes and animation clips as JSON (SceneJson.hpp).

#include "affresco/scene3d/SceneJson.hpp"

#include <array>
#include <cmath>
#include <string>

namespace affresco::scene3d {

namespace {

/// @brief Throws a DocumentError naming a place. @param where The place. @param what The fault.
[[noreturn]] void fail(std::string_view where, std::string_view what) {
	throw DocumentError(std::string(where) + ": " + std::string(what));
}

/// @brief A finite number member, or a default. @param object The object. @param key The member. @param fallback If
/// absent. @param where Names the object. @return The number. @throws DocumentError If not a finite number.
float number(const json& object, const char* key, float fallback, std::string_view where) {
	const auto found = object.find(key);
	if(found == object.end()) return fallback;
	if(!found->is_number() || !std::isfinite(found->get<double>()))
		fail(std::string(where) + "." + key, "expected a finite number");
	return found->get<float>();
}

/// @brief A string member, or empty. @param object The object. @param key The member. @param where Names the
/// object. @return The string. @throws DocumentError If not a string.
std::string text(const json& object, const char* key, std::string_view where) {
	const auto found = object.find(key);
	if(found == object.end() || found->is_null()) return {};
	if(!found->is_string()) fail(std::string(where) + "." + key, "expected a string");
	return found->get<std::string>();
}

/// @brief A boolean member, or a default. @param object The object. @param key The member. @param fallback If
/// absent. @param where Names the object. @return It. @throws DocumentError If not a boolean.
bool flag(const json& object, const char* key, bool fallback, std::string_view where) {
	const auto found = object.find(key);
	if(found == object.end()) return fallback;
	if(!found->is_boolean()) fail(std::string(where) + "." + key, "expected true or false");
	return found->get<bool>();
}

/// @brief Up to 4 finite numbers from an array of exactly @p count. @param value It. @param count How many.
/// @param where Names it. @return The numbers. @throws DocumentError If malformed.
std::array<float, 4> numbers(const json& value, std::size_t count, std::string_view where) {
	if(!value.is_array() || value.size() != count)
		fail(where, "expected an array of " + std::to_string(count) + " numbers");
	std::array<float, 4> result{};
	for(std::size_t i = 0; i < count; ++i) {
		if(!value[i].is_number() || !std::isfinite(value[i].get<double>())) fail(where, "expected finite numbers");
		result[i] = value[i].get<float>();
	}
	return result;
}

const std::array<const char*, 3> lightTypes{"directional", "point", "spot"}; ///< By LightType.
const std::array<const char*, 3> alphaModes{"opaque", "mask", "blend"};      ///< By AlphaMode.

/// @brief An enumerator by name. @param names The names, by value. @param name The name. @param where Names it.
/// @return The index. @throws DocumentError If unknown.
std::size_t named(const std::array<const char*, 3>& names, const std::string& name, std::string_view where) {
	for(std::size_t i = 0; i < names.size(); ++i)
		if(name == names[i]) return i;
	fail(where, "unknown value '" + name + "'");
}

/// @brief A light from JSON. @param value It. @param where Names it. @return The light.
Light lightFromJson(const json& value, const std::string& where) {
	if(!value.is_object()) fail(where, "expected an object");
	Light light;
	light.type = static_cast<LightType>(named(lightTypes, text(value, "type", where), where + ".type"));
	if(value.contains("color")) light.color = vec3FromJson(value["color"], where + ".color");
	light.intensity = number(value, "intensity", light.intensity, where);
	light.range = number(value, "range", light.range, where);
	light.innerCone = number(value, "innerCone", light.innerCone, where);
	light.outerCone = number(value, "outerCone", light.outerCone, where);
	light.castsShadow = flag(value, "castsShadow", light.castsShadow, where);
	return light;
}

/// @brief A lens from JSON. @param value It. @param where Names it. @return The lens.
Lens lensFromJson(const json& value, const std::string& where) {
	if(!value.is_object()) fail(where, "expected an object");
	Lens lens;
	lens.orthographic = flag(value, "orthographic", lens.orthographic, where);
	lens.fieldOfView = number(value, "fieldOfView", lens.fieldOfView, where);
	lens.halfHeight = number(value, "halfHeight", lens.halfHeight, where);
	lens.nearPlane = number(value, "near", lens.nearPlane, where);
	lens.farPlane = number(value, "far", lens.farPlane, where);
	if(lens.nearPlane <= 0.0f || lens.farPlane <= lens.nearPlane || lens.fieldOfView <= 0.0f ||
	   lens.fieldOfView >= 3.1f || lens.halfHeight <= 0.0f)
		fail(where, "the lens is out of range");
	return lens;
}

/// @brief A node from JSON. @param value It. @param where Names it. @return The node.
Node nodeFromJson(const json& value, const std::string& where) {
	if(!value.is_object()) fail(where, "expected an object");
	Node node;
	node.id = text(value, "id", where);
	node.name = text(value, "name", where);
	node.parent = text(value, "parent", where);
	if(value.contains("translation"))
		node.transform.translation = vec3FromJson(value["translation"], where + ".translation");
	if(value.contains("rotation")) node.transform.rotation = quatFromJson(value["rotation"], where + ".rotation");
	if(value.contains("rotationDegrees")) { // what people type: Euler angles, x then y then z
		const Vec3 angles = vec3FromJson(value["rotationDegrees"], where + ".rotationDegrees");
		node.transform.rotation = Quat::fromEuler({radians(angles.x), radians(angles.y), radians(angles.z)});
	}
	if(value.contains("scale")) node.transform.scale = vec3FromJson(value["scale"], where + ".scale");
	node.mesh = text(value, "mesh", where);
	node.material = text(value, "material", where);
	if(value.contains("light") && !value["light"].is_null())
		node.light = lightFromJson(value["light"], where + ".light");
	if(value.contains("camera") && !value["camera"].is_null())
		node.camera = lensFromJson(value["camera"], where + ".camera");
	node.visible = flag(value, "visible", true, where);
	return node;
}

/// @brief A material from JSON. @param value It. @param where Names it. @return The material.
Material materialFromJson(const json& value, const std::string& where) {
	if(!value.is_object()) fail(where, "expected an object");
	Material material;
	material.id = text(value, "id", where);
	material.name = text(value, "name", where);
	if(value.contains("baseColor")) {
		const auto color = numbers(value["baseColor"], 4, where + ".baseColor");
		material.baseColor = {color[0], color[1], color[2], color[3]};
	}
	material.metallic = std::clamp(number(value, "metallic", material.metallic, where), 0.0f, 1.0f);
	material.roughness = std::clamp(number(value, "roughness", material.roughness, where), 0.0f, 1.0f);
	if(value.contains("emissive")) material.emissive = vec3FromJson(value["emissive"], where + ".emissive");
	if(value.contains("alphaMode"))
		material.alphaMode = static_cast<AlphaMode>(named(alphaModes, text(value, "alphaMode", where), where));
	material.alphaCutoff = number(value, "alphaCutoff", material.alphaCutoff, where);
	material.doubleSided = flag(value, "doubleSided", material.doubleSided, where);
	if(const auto textures = value.find("textures"); textures != value.end() && textures->is_object()) {
		material.baseColorTexture = text(*textures, "baseColor", where + ".textures");
		material.metallicRoughnessTexture = text(*textures, "metallicRoughness", where + ".textures");
		material.normalTexture = text(*textures, "normal", where + ".textures");
		material.emissiveTexture = text(*textures, "emissive", where + ".textures");
	}
	return material;
}

/// @brief A track from JSON. @param value It. @param where Names it. @return The track.
Track trackFromJson(const json& value, const std::string& where) {
	if(!value.is_object()) fail(where, "expected an object");
	Track track;
	track.node = text(value, "node", where);
	const auto path = trackPathNamed(text(value, "path", where));
	const auto interpolation = interpolationNamed(value.contains("interpolation") ? text(value, "interpolation", where)
	                                                                              : std::string("linear"));
	if(!path || !interpolation) fail(where, "unknown path or interpolation");
	track.path = *path;
	track.interpolation = *interpolation;
	for(const char* key : {"times", "values"}) {
		const auto found = value.find(key);
		if(found == value.end() || !found->is_array()) fail(where + "." + key, "expected an array of numbers");
		auto& into = std::string_view(key) == "times" ? track.times : track.values;
		for(const json& item : *found) {
			if(!item.is_number()) fail(where + "." + key, "expected numbers");
			into.push_back(item.get<float>());
		}
	}
	if(const std::string fault = track.validate(); !fault.empty()) fail(where, fault);
	return track;
}

} // namespace

json toJson(Vec3 v) { return json::array({v.x, v.y, v.z}); }

Vec3 vec3FromJson(const json& value, std::string_view where) {
	const auto n = numbers(value, 3, where);
	return {n[0], n[1], n[2]};
}

json toJson(Quat q) { return json::array({q.x, q.y, q.z, q.w}); }

Quat quatFromJson(const json& value, std::string_view where) {
	const auto n = numbers(value, 4, where);
	const Quat q{n[0], n[1], n[2], n[3]};
	if(q.x == 0.0f && q.y == 0.0f && q.z == 0.0f && q.w == 0.0f) fail(where, "a rotation can't be all zeros");
	return normalize(q);
}

json toJson(const Node& node) {
	json value{{"id", node.id}, {"name", node.name}};
	if(!node.parent.empty()) value["parent"] = node.parent;
	value["translation"] = toJson(node.transform.translation);
	value["rotation"] = toJson(node.transform.rotation);
	value["scale"] = toJson(node.transform.scale);
	if(!node.mesh.empty()) value["mesh"] = node.mesh;
	if(!node.material.empty()) value["material"] = node.material;
	if(node.light) {
		const Light& light = *node.light;
		value["light"] = {{"type", lightTypes[static_cast<std::size_t>(light.type)]},
		                  {"color", toJson(light.color)},
		                  {"intensity", light.intensity},
		                  {"range", light.range},
		                  {"innerCone", light.innerCone},
		                  {"outerCone", light.outerCone},
		                  {"castsShadow", light.castsShadow}};
	}
	if(node.camera)
		value["camera"] = {{"orthographic", node.camera->orthographic},
		                   {"fieldOfView", node.camera->fieldOfView},
		                   {"halfHeight", node.camera->halfHeight},
		                   {"near", node.camera->nearPlane},
		                   {"far", node.camera->farPlane}};
	value["visible"] = node.visible;
	return value;
}

json toJson(const Material& material) {
	const Vec4& color = material.baseColor;
	json value{{"id", material.id},
	           {"name", material.name},
	           {"baseColor", json::array({color.x, color.y, color.z, color.w})},
	           {"metallic", material.metallic},
	           {"roughness", material.roughness},
	           {"emissive", toJson(material.emissive)},
	           {"alphaMode", alphaModes[static_cast<std::size_t>(material.alphaMode)]},
	           {"alphaCutoff", material.alphaCutoff},
	           {"doubleSided", material.doubleSided}};
	json textures = json::object();
	if(!material.baseColorTexture.empty()) textures["baseColor"] = material.baseColorTexture;
	if(!material.metallicRoughnessTexture.empty()) textures["metallicRoughness"] = material.metallicRoughnessTexture;
	if(!material.normalTexture.empty()) textures["normal"] = material.normalTexture;
	if(!material.emissiveTexture.empty()) textures["emissive"] = material.emissiveTexture;
	if(!textures.empty()) value["textures"] = std::move(textures);
	return value;
}

json toJson(const Scene& scene) {
	json nodes = json::array(), materials = json::array();
	for(const Node& node : scene.nodes) nodes.push_back(toJson(node));
	for(const Material& material : scene.materials) materials.push_back(toJson(material));
	return {{"nodes", std::move(nodes)}, {"materials", std::move(materials)}};
}

Scene sceneFromJson(const json& value, const SceneLimits& limits) {
	if(!value.is_object()) fail("scene", "expected an object");
	Scene scene;
	for(const char* key : {"nodes", "materials"}) {
		const auto found = value.find(key);
		if(found == value.end()) continue;
		if(!found->is_array()) fail(std::string("scene.") + key, "expected an array");
		const std::size_t limit = std::string_view(key) == "nodes" ? limits.nodes : limits.materials;
		if(found->size() > limit) fail(std::string("scene.") + key, "too many");
		for(std::size_t i = 0; i < found->size(); ++i) {
			const std::string where = std::string("scene.") + key + "[" + std::to_string(i) + "]";
			if(std::string_view(key) == "nodes") scene.nodes.push_back(nodeFromJson((*found)[i], where));
			else scene.materials.push_back(materialFromJson((*found)[i], where));
		}
	}
	if(const std::string fault = scene.validate(limits); !fault.empty()) fail("scene", fault);
	return scene;
}

json toJson(const std::vector<AnimationClip>& clips) {
	json result = json::array();
	for(const AnimationClip& clip : clips) {
		json tracks = json::array();
		for(const Track& track : clip.tracks)
			tracks.push_back({{"node", track.node},
			                  {"path", trackPathName(track.path)},
			                  {"interpolation", interpolationName(track.interpolation)},
			                  {"times", track.times},
			                  {"values", track.values}});
		result.push_back({{"name", clip.name}, {"tracks", std::move(tracks)}});
	}
	return result;
}

std::vector<AnimationClip> clipsFromJson(const json& value, std::size_t maxKeys) {
	if(!value.is_array()) fail("clips", "expected an array");
	std::vector<AnimationClip> clips;
	std::size_t keys = 0;
	for(std::size_t i = 0; i < value.size(); ++i) {
		const std::string where = "clips[" + std::to_string(i) + "]";
		if(!value[i].is_object()) fail(where, "expected an object");
		AnimationClip clip;
		clip.name = text(value[i], "name", where);
		const auto tracks = value[i].find("tracks");
		if(tracks == value[i].end() || !tracks->is_array()) fail(where + ".tracks", "expected an array");
		for(std::size_t t = 0; t < tracks->size(); ++t) {
			keys += (*tracks)[t].is_object() && (*tracks)[t].contains("times") ? (*tracks)[t]["times"].size() : 0;
			if(keys > maxKeys) fail(where, "too many keys");
			clip.tracks.push_back(trackFromJson((*tracks)[t], where + ".tracks[" + std::to_string(t) + "]"));
		}
		clips.push_back(std::move(clip));
	}
	return clips;
}

} // namespace affresco::scene3d
