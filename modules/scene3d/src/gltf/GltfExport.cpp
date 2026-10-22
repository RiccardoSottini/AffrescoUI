/// @file
/// @brief Writing a scene as a `.gltf` file with its one buffer embedded as a `data:` URI.

#include "affresco/core/Json.hpp"
#include "affresco/scene3d/Gltf.hpp"
#include "affresco/scene3d/SceneJson.hpp"

#include <cstring>
#include <map>

namespace affresco::scene3d {

namespace {

/// @brief glTF's numbers for what an accessor holds.
constexpr int floatComponent = 5126;       ///< FLOAT.
constexpr int unsignedIntComponent = 5125; ///< UNSIGNED_INT.
constexpr int arrayBuffer = 34962;         ///< ARRAY_BUFFER.
constexpr int elementArrayBuffer = 34963;  ///< ELEMENT_ARRAY_BUFFER.

/// @brief Bytes as base64. @param bytes Them. @return The text.
[[nodiscard]] std::string base64(const std::vector<std::uint8_t>& bytes) {
	static constexpr char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string text;
	text.reserve((bytes.size() + 2) / 3 * 4);
	for(std::size_t i = 0; i < bytes.size(); i += 3) {
		const std::size_t left = bytes.size() - i;
		const std::uint32_t chunk = (std::uint32_t{bytes[i]} << 16) |
		                            (left > 1 ? std::uint32_t{bytes[i + 1]} << 8 : 0) |
		                            (left > 2 ? std::uint32_t{bytes[i + 2]} : 0);
		text += digits[(chunk >> 18) & 63];
		text += digits[(chunk >> 12) & 63];
		text += left > 1 ? digits[(chunk >> 6) & 63] : '=';
		text += left > 2 ? digits[chunk & 63] : '=';
	}
	return text;
}

/// @brief The file being written: its JSON and its buffer.
/// @brief What an accessor's elements are: their glTF type, the 4-byte components in one, and the components' type.
struct Layout {
	const char* type;       ///< "SCALAR", "VEC2", "VEC3" or "VEC4".
	std::size_t components; ///< Per element.
	int componentType;      ///< FLOAT or UNSIGNED_INT.
};

constexpr Layout floats1{"SCALAR", 1, floatComponent};        ///< Times.
constexpr Layout floats2{"VEC2", 2, floatComponent};          ///< Texture coordinates.
constexpr Layout floats3{"VEC3", 3, floatComponent};          ///< Positions, normals, translations and scales.
constexpr Layout floats4{"VEC4", 4, floatComponent};          ///< Rotations.
constexpr Layout indices1{"SCALAR", 1, unsignedIntComponent}; ///< Triangle indices.

class Writer {
public:
	/// @brief Adds an accessor over new bytes.
	/// @param data The bytes' first element. @param count Elements. @param layout What an element is.
	/// @param target The view's target, or 0.
	/// @return The accessor's index, to which "min" and "max" may be added.
	std::size_t accessor(const void* data, std::size_t count, const Layout& layout, int target) {
		const auto [type, components, componentType] = layout;
		const std::size_t bytes = count * components * 4;
		const std::size_t offset = buffer_.size();
		buffer_.resize(offset + bytes);
		if(bytes != 0) std::memcpy(buffer_.data() + offset, data, bytes);
		json view{{"buffer", 0}, {"byteOffset", offset}, {"byteLength", bytes}};
		if(target != 0) view["target"] = target;
		root["bufferViews"].push_back(view);
		root["accessors"].push_back({{"bufferView", root["bufferViews"].size() - 1},
		                             {"componentType", componentType},
		                             {"count", count},
		                             {"type", type}});
		return root["accessors"].size() - 1;
	}

	/// @brief Closes the buffer and returns the text. @return The `.gltf` JSON.
	[[nodiscard]] std::string finish() {
		if(!buffer_.empty())
			root["buffers"] = json::array(
			    {{{"byteLength", buffer_.size()}, {"uri", "data:application/octet-stream;base64," + base64(buffer_)}}});
		return root.dump(1, '\t');
	}

	json root{{"asset", {{"version", "2.0"}, {"generator", "AffrescoUI scene3d"}}}}; ///< The JSON so far.

private:
	std::vector<std::uint8_t> buffer_; ///< The binary data, 4-byte elements so always aligned.
};

/// @brief Writes one mesh with one material as a glTF mesh. @param writer The file. @param mesh The data.
/// @param material The material's index, or -1. @return The mesh's index.
std::size_t writeMesh(Writer& writer, const MeshData& mesh, int material) {
	json attributes;
	const auto position = writer.accessor(mesh.positions.data(), mesh.positions.size(), floats3, arrayBuffer);
	writer.root["accessors"][position]["min"] = toJson(mesh.bounds.min);
	writer.root["accessors"][position]["max"] = toJson(mesh.bounds.max);
	attributes["POSITION"] = position;
	if(mesh.normals.size() == mesh.positions.size())
		attributes["NORMAL"] = writer.accessor(mesh.normals.data(), mesh.normals.size(), floats3, arrayBuffer);
	if(mesh.texCoords.size() == mesh.positions.size())
		attributes["TEXCOORD_0"] = writer.accessor(mesh.texCoords.data(), mesh.texCoords.size(), floats2, arrayBuffer);
	json primitive{
	    {"attributes", attributes},
	    {"indices", writer.accessor(mesh.indices.data(), mesh.indices.size(), indices1, elementArrayBuffer)}};
	if(material >= 0) primitive["material"] = material;
	writer.root["meshes"].push_back({{"primitives", json::array({primitive})}});
	return writer.root["meshes"].size() - 1;
}

/// @brief A material's JSON, naming its textures in the warnings (they aren't written).
/// @param material It. @param warnings Receives them.
/// @return The object.
[[nodiscard]] json materialJson(const Material& material, std::vector<std::string>& warnings) {
	const auto& c = material.baseColor;
	json out{{"name", material.name},
	         {"pbrMetallicRoughness",
	          {{"baseColorFactor", {c.x, c.y, c.z, c.w}},
	           {"metallicFactor", material.metallic},
	           {"roughnessFactor", material.roughness}}},
	         {"emissiveFactor", toJson(material.emissive)},
	         {"alphaMode", material.alphaMode == AlphaMode::Blend  ? "BLEND"
	                       : material.alphaMode == AlphaMode::Mask ? "MASK"
	                                                               : "OPAQUE"},
	         {"doubleSided", material.doubleSided}};
	if(material.alphaMode == AlphaMode::Mask) out["alphaCutoff"] = material.alphaCutoff;
	for(const auto* texture : {&material.baseColorTexture, &material.metallicRoughnessTexture, &material.normalTexture,
	                           &material.emissiveTexture})
		if(!texture->empty()) warnings.push_back("the texture " + *texture + " of " + material.id + " is not exported");
	return out;
}

/// @brief A light's KHR_lights_punctual JSON. @param light It. @return The object.
[[nodiscard]] json lightJson(const Light& light) {
	json out{{"type", light.type == LightType::Point  ? "point"
	                  : light.type == LightType::Spot ? "spot"
	                                                  : "directional"},
	         {"color", toJson(light.color)},
	         {"intensity", light.intensity}};
	if(light.range > 0.0f && light.type != LightType::Directional) out["range"] = light.range;
	if(light.type == LightType::Spot)
		out["spot"] = {{"innerConeAngle", light.innerCone}, {"outerConeAngle", light.outerCone}};
	return out;
}

/// @brief A lens as a glTF camera. @param lens It. @return The object.
[[nodiscard]] json cameraJson(const Lens& lens) {
	if(lens.orthographic)
		return {{"type", "orthographic"},
		        {"orthographic",
		         {{"xmag", lens.halfHeight},
		          {"ymag", lens.halfHeight},
		          {"znear", lens.nearPlane},
		          {"zfar", lens.farPlane}}}};
	return {{"type", "perspective"},
	        {"perspective", {{"yfov", lens.fieldOfView}, {"znear", lens.nearPlane}, {"zfar", lens.farPlane}}}};
}

/// @brief The JSON of one node, without its children. @param writer The file. @param node It.
/// @param mesh Its glTF mesh, or -1. @return The object.
[[nodiscard]] json nodeJson(Writer& writer, const Node& node, int mesh) {
	const auto& r = node.transform.rotation;
	json out{{"name", node.name},
	         {"translation", toJson(node.transform.translation)},
	         {"rotation", {r.x, r.y, r.z, r.w}},
	         {"scale", toJson(node.transform.scale)}};
	if(mesh >= 0) out["mesh"] = mesh;
	if(node.camera) {
		writer.root["cameras"].push_back(cameraJson(*node.camera));
		out["camera"] = writer.root["cameras"].size() - 1;
	}
	if(node.light) {
		auto& lights = writer.root["extensions"]["KHR_lights_punctual"]["lights"];
		lights.push_back(lightJson(*node.light));
		out["extensions"]["KHR_lights_punctual"]["light"] = lights.size() - 1;
	}
	return out;
}

/// @brief Writes the animations. @param writer The file. @param animations Them. @param nodes Node indices by id.
void writeAnimations(Writer& writer, const std::vector<AnimationClip>& animations,
                     const std::map<std::string, std::size_t, std::less<>>& nodes) {
	for(const auto& clip : animations) {
		json samplers = json::array();
		json channels = json::array();
		for(const auto& track : clip.tracks) {
			const auto node = nodes.find(track.node);
			if(node == nodes.end() || track.times.empty()) continue;
			const auto input = writer.accessor(track.times.data(), track.times.size(), floats1, 0);
			writer.root["accessors"][input]["min"] = {track.times.front()};
			writer.root["accessors"][input]["max"] = {track.times.back()};
			const std::size_t width = track.width();
			const auto output =
			    writer.accessor(track.values.data(), track.values.size() / width, width == 4 ? floats4 : floats3, 0);
			samplers.push_back({{"input", input},
			                    {"output", output},
			                    {"interpolation", track.interpolation == Interpolation::Step    ? "STEP"
			                                      : track.interpolation == Interpolation::Cubic ? "CUBICSPLINE"
			                                                                                    : "LINEAR"}});
			channels.push_back({{"sampler", samplers.size() - 1},
			                    {"target", {{"node", node->second}, {"path", trackPathName(track.path)}}}});
		}
		if(!channels.empty())
			writer.root["animations"].push_back({{"name", clip.name}, {"samplers", samplers}, {"channels", channels}});
	}
}

/// @brief Writes the materials. @param writer The file. @param scene Its materials.
/// @param skipped Receives what couldn't be written. @return Material indices by id.
[[nodiscard]] std::map<std::string, std::size_t, std::less<>> writeMaterials(Writer& writer, const Scene& scene,
                                                                             std::vector<std::string>& skipped) {
	std::map<std::string, std::size_t, std::less<>> materials;
	for(const auto& material : scene.materials) {
		materials.emplace(material.id, materials.size());
		writer.root["materials"].push_back(materialJson(material, skipped));
	}
	return materials;
}

/// @brief Links each written node to its parent. @param writer The file, with the nodes.
/// @param scene The nodes. @param nodes Node indices by id. @return The nodes without a parent.
[[nodiscard]] json linkNodes(Writer& writer, const Scene& scene,
                             const std::map<std::string, std::size_t, std::less<>>& nodes) {
	json roots = json::array();
	for(const auto& node : scene.nodes) {
		const auto parent = nodes.find(node.parent);
		if(parent == nodes.end()) roots.push_back(nodes.at(node.id));
		else writer.root["nodes"][parent->second]["children"].push_back(nodes.at(node.id));
	}
	return roots;
}

} // namespace

std::string exportGltf(const Scene& scene, const Assets& assets, const std::vector<AnimationClip>& animations,
                       std::vector<std::string>* warnings) {
	std::vector<std::string> skipped;
	Writer writer;
	const auto materials = writeMaterials(writer, scene, skipped);
	std::map<std::string, std::size_t, std::less<>> nodes;
	for(const auto& node : scene.nodes) nodes.emplace(node.id, nodes.size());
	std::map<std::pair<std::string, int>, std::size_t> meshes;
	for(const auto& node : scene.nodes) {
		int mesh = -1;
		if(const auto data = node.mesh.empty() ? nullptr : assets.mesh(node.mesh)) {
			const auto material = materials.find(node.material);
			const int index = material == materials.end() ? -1 : static_cast<int>(material->second);
			const auto key = std::make_pair(node.mesh, index);
			if(!meshes.contains(key)) meshes.emplace(key, writeMesh(writer, *data, index));
			mesh = static_cast<int>(meshes.at(key));
		} else if(!node.mesh.empty()) skipped.push_back("the mesh " + node.mesh + " of " + node.id + " is missing");
		writer.root["nodes"].push_back(nodeJson(writer, node, mesh));
	}
	const json roots = linkNodes(writer, scene, nodes);
	if(writer.root.contains("extensions")) writer.root["extensionsUsed"] = {"KHR_lights_punctual"};
	writer.root["scene"] = 0;
	writer.root["scenes"] = json::array({{{"nodes", roots}}});
	writeAnimations(writer, animations, nodes);
	if(warnings != nullptr) warnings->insert(warnings->end(), skipped.begin(), skipped.end());
	return writer.finish();
}

} // namespace affresco::scene3d
