/// @file
/// @brief Turning a loaded, validated glTF file into a Scene: materials, meshes, nodes, lights, cameras, animations.

#include "GltfInternal.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace affresco::scene3d::detail {

namespace {

/// @brief A name, or a made-up one if the file has none. @param name The file's, or null. @param kind E.g. "Node".
/// @param index Its index. @return The name.
[[nodiscard]] std::string nameOr(const char* name, std::string_view kind, std::size_t index) {
	return name != nullptr && *name != '\0' ? std::string(name) : std::string(kind) + " " + std::to_string(index);
}

/// @brief Whether some floats are all finite. @param values Them. @return true if so.
[[nodiscard]] bool allFinite(std::span<const float> values) noexcept {
	return std::ranges::all_of(values, [](float value) { return std::isfinite(value); });
}

/// @brief Whether an accessor can be read; warns if not. @param context The load. @param accessor It, or null.
/// @param where Names it in the warning. @return true if it can be read.
[[nodiscard]] bool readable(GltfContext& context, const cgltf_accessor* accessor, const std::string& where) {
	if(accessor == nullptr) return false;
	if(accessorCompressed(*accessor)) {
		warn(context, where + " is skipped: meshopt compression (EXT_meshopt_compression) is not supported");
		return false;
	}
	if(!accessorInBounds(*accessor)) {
		warn(context, where + " is skipped: its buffer isn't loaded");
		return false;
	}
	return true;
}

/// @brief An accessor's elements as floats. @param accessor It. @return count * components floats.
[[nodiscard]] std::vector<float> floats(const cgltf_accessor& accessor) {
	std::vector<float> values(accessor.count * cgltf_num_components(accessor.type));
	if(!values.empty()) static_cast<void>(cgltf_accessor_unpack_floats(&accessor, values.data(), values.size()));
	return values;
}

/// @brief The accessor of an attribute. @param primitive It. @param type The kind. @param index Its set (0).
/// @return It, or null.
[[nodiscard]] const cgltf_accessor* attribute(const cgltf_primitive& primitive, cgltf_attribute_type type,
                                              int index = 0) noexcept {
	for(std::size_t i = 0; i < primitive.attributes_count; ++i)
		if(primitive.attributes[i].type == type && primitive.attributes[i].index == index)
			return primitive.attributes[i].data;
	return nullptr;
}

/// @brief Fails when a running total passes its limit. @param total It. @param limit The limit. @param what Names it.
void checkTotal(std::size_t total, std::size_t limit, const char* what) {
	if(total > limit) throw GltfError("glTF: more than " + std::to_string(limit) + " " + what);
}

/// @brief Whether a vertex attribute is there, of a type, one per vertex, and readable.
/// @param context The load. @param accessor The attribute, or null. @param type Its type. @param count The vertices.
/// @param what Names it in a warning. @return Whether to read it.
[[nodiscard]] bool usable(GltfContext& context, const cgltf_accessor* accessor, cgltf_type type, std::size_t count,
                          const std::string& what) {
	return accessor != nullptr && accessor->type == type && accessor->count == count &&
	       readable(context, accessor, what);
}

/// @brief Reads a primitive's indices, or numbers its vertices in order when it has none.
/// @param context The load. @param primitive It. @param count Its vertices. @param where Names it in warnings.
/// @param indices Receives the indices. @return false when its indices are unreadable (a warning says why).
[[nodiscard]] bool readIndices(GltfContext& context, const cgltf_primitive& primitive, std::size_t count,
                               const std::string& where, std::vector<std::uint32_t>& indices) {
	if(primitive.indices == nullptr) {
		indices.resize(count);
		std::ranges::iota(indices, std::uint32_t{0});
		return true;
	}
	if(!readable(context, primitive.indices, where + " indices")) return false;
	indices.resize(primitive.indices->count);
	for(std::size_t i = 0; i < indices.size(); ++i)
		indices[i] = static_cast<std::uint32_t>(cgltf_accessor_read_index(primitive.indices, i));
	return true;
}

/// @brief Reads a primitive's vertices and indices; the counts are checked first.
/// @param primitive It. @param position Its positions, readable. @param totals Vertices and indices so far.
/// @param limits The limits. @param context The load. @param where Names it in warnings.
/// @return The mesh, or nothing (a warning says why).
[[nodiscard]] std::optional<MeshData> readPrimitive(GltfContext& context, const cgltf_primitive& primitive,
                                                    const cgltf_accessor& position,
                                                    std::pair<std::size_t, std::size_t>& totals,
                                                    const std::string& where) {
	const std::size_t count = position.count;
	totals.first += count;
	totals.second += primitive.indices != nullptr ? primitive.indices->count : count;
	checkTotal(totals.first, context.limits.vertices, "vertices");
	checkTotal(totals.second, context.limits.indices, "indices");
	MeshData mesh;
	const auto points = floats(position);
	for(std::size_t i = 0; i + 2 < points.size(); i += 3)
		mesh.positions.push_back({points[i], points[i + 1], points[i + 2]});
	const auto* normal = attribute(primitive, cgltf_attribute_type_normal);
	if(usable(context, normal, cgltf_type_vec3, count, where + " normals")) {
		const auto values = floats(*normal);
		for(std::size_t i = 0; i + 2 < values.size(); i += 3)
			mesh.normals.push_back({values[i], values[i + 1], values[i + 2]});
	}
	const auto* uv = attribute(primitive, cgltf_attribute_type_texcoord);
	if(usable(context, uv, cgltf_type_vec2, count, where + " texture coordinates")) {
		const auto values = floats(*uv);
		for(std::size_t i = 0; i + 1 < values.size(); i += 2) mesh.texCoords.push_back({values[i], values[i + 1]});
	}
	if(!readIndices(context, primitive, count, where, mesh.indices)) return std::nullopt;
	return mesh;
}

/// @brief Converts one primitive into an asset.
/// @param context The load. @param primitive It. @param id Its mesh id. @param totals Vertices and indices so far.
/// @return true if it was added.
bool convertPrimitive(GltfContext& context, const cgltf_primitive& primitive, const std::string& id,
                      std::pair<std::size_t, std::size_t>& totals) {
	const std::string where = "the primitive " + id;
	if(primitive.has_draco_mesh_compression) {
		warn(context, where + " is skipped: Draco compression (KHR_draco_mesh_compression) is not supported");
		return false;
	}
	if(primitive.type != cgltf_primitive_type_triangles) {
		warn(context, where + " is skipped: only triangles are read");
		return false;
	}
	const auto* position = attribute(primitive, cgltf_attribute_type_position);
	if(position == nullptr || position->type != cgltf_type_vec3 || !readable(context, position, where)) return false;
	auto mesh = readPrimitive(context, primitive, *position, totals, where);
	if(!mesh) return false;
	if(mesh->normals.empty()) mesh->computeNormals();
	mesh->computeBounds();
	if(const std::string fault = mesh->validate(); !fault.empty() || mesh->indices.size() % 3 != 0) {
		warn(context, where + " is skipped: " + (fault.empty() ? std::string("a partial triangle") : fault));
		return false;
	}
	context.out.assets.setMesh(id, std::move(*mesh));
	return true;
}

/// @brief A node's transform; the identity, with a warning, if it isn't finite or can't be decomposed.
/// @param context The load. @param node The node. @param id Its id. @return The transform.
[[nodiscard]] Transform nodeTransform(GltfContext& context, const cgltf_node& node, const std::string& id) {
	Transform transform;
	if(node.has_matrix) {
		Mat4 matrix;
		std::copy_n(node.matrix, 16, matrix.values.begin());
		if(const auto decomposed = Transform::decompose(matrix); decomposed && allFinite(node.matrix))
			return *decomposed;
		warn(context, "the node " + id + " has a matrix that isn't a transform: it is placed at the origin");
		return transform;
	}
	if(!allFinite(node.translation) || !allFinite(node.rotation) || !allFinite(node.scale)) {
		warn(context, "the node " + id + " has a transform that isn't finite: it is placed at the origin");
		return transform;
	}
	if(node.has_translation) transform.translation = {node.translation[0], node.translation[1], node.translation[2]};
	if(node.has_rotation)
		transform.rotation = normalize(Quat{node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]});
	if(node.has_scale) transform.scale = {node.scale[0], node.scale[1], node.scale[2]};
	return transform;
}

/// @brief A KHR_lights_punctual light. @param light The file's. @return Ours.
[[nodiscard]] Light convertLight(const cgltf_light& light) noexcept {
	Light out;
	out.type = light.type == cgltf_light_type_point  ? LightType::Point
	           : light.type == cgltf_light_type_spot ? LightType::Spot
	                                                 : LightType::Directional;
	out.color = {light.color[0], light.color[1], light.color[2]};
	out.intensity = light.intensity;
	out.range = light.range;
	out.innerCone = light.spot_inner_cone_angle;
	out.outerCone = light.spot_outer_cone_angle;
	return out;
}

/// @brief A camera's lens. @param camera The file's. @return Ours.
[[nodiscard]] Lens convertCamera(const cgltf_camera& camera) noexcept {
	Lens lens;
	if(camera.type == cgltf_camera_type_orthographic) {
		lens.orthographic = true;
		lens.halfHeight = camera.data.orthographic.ymag;
		lens.nearPlane = camera.data.orthographic.znear;
		lens.farPlane = camera.data.orthographic.zfar;
	} else if(camera.type == cgltf_camera_type_perspective) {
		lens.fieldOfView = camera.data.perspective.yfov;
		lens.nearPlane = camera.data.perspective.znear;
		if(camera.data.perspective.has_zfar) lens.farPlane = camera.data.perspective.zfar;
	}
	return lens;
}

/// @brief The track of one animation channel, or nothing (a warning says why).
/// @param context The load. @param channel It. @param keys The keys so far.
/// @return The track.
[[nodiscard]] std::optional<Track> convertChannel(GltfContext& context, const cgltf_animation_channel& channel,
                                                  std::size_t& keys) {
	if(channel.target_node == nullptr) return std::nullopt;
	if(channel.target_path == cgltf_animation_path_type_weights) {
		warn(context, "morph target weights are not animated");
		return std::nullopt;
	}
	const cgltf_animation_sampler& sampler = *channel.sampler;
	if(!readable(context, sampler.input, "an animation channel") ||
	   !readable(context, sampler.output, "an animation channel"))
		return std::nullopt;
	Track track;
	track.node = makeId(context, "node", static_cast<std::size_t>(channel.target_node - context.data.nodes));
	track.path = channel.target_path == cgltf_animation_path_type_rotation ? TrackPath::Rotation
	             : channel.target_path == cgltf_animation_path_type_scale  ? TrackPath::Scale
	                                                                       : TrackPath::Translation;
	track.interpolation = sampler.interpolation == cgltf_interpolation_type_step           ? Interpolation::Step
	                      : sampler.interpolation == cgltf_interpolation_type_cubic_spline ? Interpolation::Cubic
	                                                                                       : Interpolation::Linear;
	keys += sampler.input->count + sampler.output->count;
	checkTotal(keys, context.limits.keys, "animation keys");
	track.times = floats(*sampler.input);
	track.values = floats(*sampler.output);
	if(const std::string fault = track.validate(); !fault.empty()) {
		warn(context, "an animation channel of " + track.node + " is skipped: " + fault);
		return std::nullopt;
	}
	return track;
}

} // namespace

void convertMaterials(GltfContext& context) {
	for(std::size_t i = 0; i < context.data.materials_count; ++i) {
		const cgltf_material& source = context.data.materials[i];
		Material material;
		material.id = makeId(context, "material", i);
		material.name = nameOr(source.name, "Material", i);
		if(source.has_pbr_metallic_roughness) {
			const auto& pbr = source.pbr_metallic_roughness;
			const auto* c = pbr.base_color_factor;
			material.baseColor = {c[0], c[1], c[2], c[3]};
			material.metallic = std::clamp(pbr.metallic_factor, 0.0f, 1.0f);
			material.roughness = std::clamp(pbr.roughness_factor, 0.0f, 1.0f);
			material.baseColorTexture = textureId(context, pbr.base_color_texture.texture, true);
			material.metallicRoughnessTexture = textureId(context, pbr.metallic_roughness_texture.texture, false);
		}
		const float strength = source.has_emissive_strength ? source.emissive_strength.emissive_strength : 1.0f;
		material.emissive =
		    Vec3{source.emissive_factor[0], source.emissive_factor[1], source.emissive_factor[2]} * strength;
		material.normalTexture = textureId(context, source.normal_texture.texture, false);
		material.emissiveTexture = textureId(context, source.emissive_texture.texture, true);
		material.alphaMode = source.alpha_mode == cgltf_alpha_mode_blend  ? AlphaMode::Blend
		                     : source.alpha_mode == cgltf_alpha_mode_mask ? AlphaMode::Mask
		                                                                  : AlphaMode::Opaque;
		material.alphaCutoff = source.alpha_cutoff;
		material.doubleSided = source.double_sided != 0;
		context.out.scene.materials.push_back(std::move(material));
	}
}

void convertMeshes(GltfContext& context) {
	std::pair<std::size_t, std::size_t> totals;
	context.meshParts.resize(context.data.meshes_count);
	for(std::size_t m = 0; m < context.data.meshes_count; ++m) {
		const cgltf_mesh& mesh = context.data.meshes[m];
		for(std::size_t p = 0; p < mesh.primitives_count; ++p) {
			const cgltf_primitive& primitive = mesh.primitives[p];
			const std::string id = makeId(context, "mesh", m) + "." + std::to_string(p);
			if(!convertPrimitive(context, primitive, id, totals)) continue;
			const std::string material =
			    primitive.material != nullptr
			        ? makeId(context, "material", static_cast<std::size_t>(primitive.material - context.data.materials))
			        : std::string();
			context.meshParts[m].emplace_back(id, material);
		}
	}
}

void convertNodes(GltfContext& context) {
	auto& nodes = context.out.scene.nodes;
	for(std::size_t i = 0; i < context.data.nodes_count; ++i) {
		const cgltf_node& source = context.data.nodes[i];
		Node node;
		node.id = makeId(context, "node", i);
		node.name = nameOr(source.name, "Node", i);
		if(source.parent != nullptr)
			node.parent = makeId(context, "node", static_cast<std::size_t>(source.parent - context.data.nodes));
		node.transform = nodeTransform(context, source, node.id);
		if(source.light != nullptr) node.light = convertLight(*source.light);
		if(source.camera != nullptr) node.camera = convertCamera(*source.camera);
		const auto* parts = source.mesh != nullptr
		                        ? &context.meshParts[static_cast<std::size_t>(source.mesh - context.data.meshes)]
		                        : nullptr;
		if(parts != nullptr && !parts->empty()) std::tie(node.mesh, node.material) = parts->front();
		nodes.push_back(node);
		for(std::size_t k = 1; parts != nullptr && k < parts->size(); ++k) {
			Node part;
			part.id = node.id + "." + std::to_string(k);
			part.name = node.name + "." + std::to_string(k);
			part.parent = node.id;
			std::tie(part.mesh, part.material) = (*parts)[k];
			nodes.push_back(std::move(part));
		}
	}
}

void convertAnimations(GltfContext& context) {
	std::size_t keys = 0;
	for(std::size_t a = 0; a < context.data.animations_count; ++a) {
		const cgltf_animation& source = context.data.animations[a];
		AnimationClip clip{nameOr(source.name, "Animation", a), {}};
		for(std::size_t c = 0; c < source.channels_count; ++c)
			if(auto track = convertChannel(context, source.channels[c], keys)) clip.tracks.push_back(std::move(*track));
		if(!clip.tracks.empty()) context.out.animations.push_back(std::move(clip));
	}
}

} // namespace affresco::scene3d::detail
