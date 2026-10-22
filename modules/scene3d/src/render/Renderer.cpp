/// @file
/// @brief The PBR forward renderer (Renderer.hpp): the scene, the id pass and the shadow map. The grid, the outline
/// and the lines are in RendererOverlays.cpp.

#include "RendererInternal.hpp"
#include "RendererShaders.hpp"

#include "affresco/scene3d/MaterialGraph.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <set>
#include <utility>

namespace affresco::scene3d {

namespace detail {

namespace {

constexpr std::size_t maxLights = 8;         ///< Lights the PBR program takes.
constexpr std::size_t maxGraphPrograms = 64; ///< Material graph programs kept.
constexpr int shadowSize = 2048;             ///< The shadow map's side, in pixels.

/// @brief A light in world space.
struct SceneLight {
	Light light;    ///< What it is.
	Vec3 position;  ///< Where.
	Vec3 direction; ///< Where it points (directional and spot).
};

/// @brief The scene's visible lights, at most maxLights; a soft directional light if it has none.
/// @param view The frame. @return The lights.
std::vector<SceneLight> lightsOf(const FrameView& view) {
	std::vector<SceneLight> lights;
	for(const Node& node : view.scene.nodes) {
		if(!node.light || lights.size() == maxLights) continue;
		const auto world = view.worlds.find(node.id);
		if(world == view.worlds.end() || !world->second.second) continue;
		const auto [position, direction] = placement(world->second.first);
		lights.push_back({*node.light, position, direction});
	}
	if(lights.empty()) {
		SceneLight studio{Light{}, Vec3{}, normalize(Vec3{-0.4f, -0.8f, -0.45f})};
		studio.light.intensity = 3.0f;
		lights.push_back(studio);
	}
	return lights;
}

/// @brief The determinant of a matrix's upper 3x3: negative when it mirrors. @param m It. @return The determinant.
float determinant3(const Mat4& m) noexcept {
	return m.at(0, 0) * (m.at(1, 1) * m.at(2, 2) - m.at(1, 2) * m.at(2, 1)) -
	       m.at(0, 1) * (m.at(1, 0) * m.at(2, 2) - m.at(1, 2) * m.at(2, 0)) +
	       m.at(0, 2) * (m.at(1, 0) * m.at(2, 1) - m.at(1, 1) * m.at(2, 0));
}

/// @brief The matrix from world space to a light's clip space for its shadow map.
/// @param light The light (directional or spot). @param bounds What it must cover. @return The matrix.
Mat4 shadowMatrixFor(const SceneLight& light, const Aabb& bounds) noexcept {
	const Vec3 center = bounds.center();
	const float radius = std::max(length(bounds.halfExtent()), 0.01f);
	if(light.light.type == LightType::Directional) {
		const Vec3 eye = center - light.direction * (2.0f * radius);
		return Mat4::orthographic(radius, radius, radius * 0.5f, radius * 3.5f) *
		       Mat4::lookAt(eye, center, Vec3{0.0f, 1.0f, 0.0f});
	}
	const float reach = light.light.range > 0.0f ? light.light.range : length(center - light.position) + radius * 2.0f;
	const float angle = std::clamp(light.light.outerCone * 2.0f, 0.05f, 3.0f);
	return Mat4::perspective(angle, 1.0f, std::max(reach * 0.002f, 0.02f), reach) *
	       Mat4::lookAt(light.position, light.position + light.direction, Vec3{0.0f, 1.0f, 0.0f});
}

/// @brief Binds a texture to a unit if the store has it. @param unit 1..4. @param id Its id.
/// @param resources The uploads. @param assets The store. @param frame This frame. @return Whether it was bound.
bool bindMap(int unit, const std::string& id, GpuResources& resources, const Assets& assets, std::uint64_t frame) {
	const unsigned texture = resources.texture(id, assets, frame);
	if(texture == 0) return false;
	glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
	glBindTexture(GL_TEXTURE_2D, texture);
	return true;
}

/// @brief Sets culling and winding for a mesh. @param item It.
void setFaces(const DrawItem& item) noexcept {
	if(item.material != nullptr && item.material->doubleSided) glDisable(GL_CULL_FACE);
	else {
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	glFrontFace(determinant3(item.world) < 0.0f ? GL_CW : GL_CCW);
}

/// @brief Reads the frame's scene, at the settings' time, and its materials' graphs.
/// @param view The frame, whose settings are read. @param params The frame's parameters (its animations).
/// @param scene The scene's JSON, which the graphs point into.
void readScene(FrameView& view, const json& params, const json& scene) {
	const auto animations = params.find("animations");
	view.scene = sceneAtTime(scene, animations != params.end() ? *animations : json(), view.settings.time);
	const auto materials = scene.find("materials");
	if(materials == scene.end() || !materials->is_array()) return;
	for(const json& material : *materials)
		if(material.is_object() && material.value("id", json()).is_string() && material.contains("graph"))
			view.graphs.emplace(material["id"].get<std::string>(), &material["graph"]);
}

/// @brief Makes a draw item of every visible mesh node with a mesh in the store, in scene order, and their bounds.
/// @param view The frame, with its scene, worlds and eye. @param assets The store.
void collectItems(FrameView& view, const Assets& assets) {
	const std::set<std::string, std::less<>> selected(view.settings.selection.begin(), view.settings.selection.end());
	const std::string active = view.settings.selection.empty() ? std::string() : view.settings.selection.back();
	for(const Node& node : view.scene.nodes) {
		const auto world = view.worlds.find(node.id);
		if(node.mesh.empty() || world == view.worlds.end() || !world->second.second) continue;
		auto data = assets.mesh(node.mesh);
		if(!data || data->indices.empty()) continue;
		DrawItem item{&node, view.scene.material(node.material), world->second.first, data,
		              data->bounds.transformed(world->second.first)};
		item.index = static_cast<std::uint32_t>(view.items.size() + 1);
		item.selected = selected.contains(node.id);
		item.active = node.id == active;
		item.blended = item.material != nullptr && item.material->alphaMode == AlphaMode::Blend;
		item.distance = length(item.bounds.center() - view.eye);
		view.bounds.extend(item.bounds);
		view.items.push_back(std::move(item));
	}
}

/// @brief Culls the items to the camera's frustum and orders the rest: the opaque in scene order, then the blended
/// far first, so that they blend over what is behind them.
/// @param view The frame, with its items and view-projection.
void orderInView(FrameView& view) {
	const Frustum frustum = Frustum::fromMatrix(view.viewProjection);
	for(std::size_t i = 0; i < view.items.size(); ++i)
		if(frustum.intersects(view.items[i].bounds)) view.inView.push_back(i);
		else ++view.culled;
	std::stable_sort(view.inView.begin(), view.inView.end(), [&](std::size_t a, std::size_t b) {
		const DrawItem& first = view.items[a];
		const DrawItem& second = view.items[b];
		if(first.blended != second.blended) return !first.blended;
		return first.blended && first.distance > second.distance;
	});
}

} // namespace

void setMatrix(int location, const Mat4& matrix) noexcept {
	glUniformMatrix4fv(location, 1, GL_FALSE, matrix.values.data());
}

Vec3 idColor(std::uint32_t index) noexcept {
	return {static_cast<float>(index & 255u) / 255.0f, static_cast<float>((index >> 8) & 255u) / 255.0f,
	        static_cast<float>((index >> 16) & 255u) / 255.0f};
}

Vec3 srgbToLinear(Vec3 color) noexcept {
	const auto channel = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
	return {channel(color.x), channel(color.y), channel(color.z)};
}

SceneRenderer::SceneRenderer(std::shared_ptr<const Assets> assets, std::shared_ptr<ViewportProbe> probe,
                             const GpuBudget& budget)
    : assets_(std::move(assets)), probe_(std::move(probe)), resources_(budget.bytes) {
	if(!assets_) assets_ = std::make_shared<const Assets>();
}

void SceneRenderer::prepare(const SurfaceSetup& setup) {
	if(pbr_.valid()) return; // nothing depends on the size
	prelude_ = std::string(setup.prelude);
	using namespace shaders;
	pbr_ = GlProgram(prelude_, {meshVertex}, {pbrHead, pbrMain});
	flat_ = GlProgram(prelude_, {positionVertex}, {flatFragment});
	depth_ = GlProgram(prelude_, {positionVertex}, {depthFragment});
	grid_ = GlProgram(prelude_, {fullscreenVertex}, {gridFragment});
	outline_ = GlProgram(prelude_, {fullscreenVertex}, {outlineFragment});
	line_ = GlProgram(prelude_, {lineVertex}, {lineFragment});
	glGenVertexArrays(1, &emptyArray_);
	glGenVertexArrays(1, &lineArray_);
	glGenBuffers(1, &lineBuffer_);
	glBindVertexArray(lineArray_);
	glBindBuffer(GL_ARRAY_BUFFER, lineBuffer_);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
	                      reinterpret_cast<const void*>(3 * sizeof(float))); // NOLINT(performance-no-int-to-ptr)
	glBindVertexArray(0);
}

void SceneRenderer::release() {
	resources_.release();
	for(GlProgram* program : {&pbr_, &flat_, &depth_, &grid_, &outline_, &line_}) program->release();
	graphs_.clear();
	glDeleteVertexArrays(1, &emptyArray_);
	glDeleteVertexArrays(1, &lineArray_);
	glDeleteBuffers(1, &lineBuffer_);
	emptyArray_ = lineArray_ = lineBuffer_ = 0;
}

FrameView SceneRenderer::makeView(const SurfaceFrame& frame) const {
	FrameView view;
	const json& params = frame.params();
	view.settings = viewportSettingsFromJson(params);
	if(const auto scene = params.find("scene"); scene != params.end()) readScene(view, params, *scene);
	view.worlds = view.scene.worldMatrices();
	view.width = std::max(frame.width, 1);
	view.height = std::max(frame.height, 1);
	view.scale = frame.scale > 0.0f ? frame.scale : 1.0f;
	view.viewProjection =
	    view.settings.camera.viewProjection(static_cast<float>(view.width) / static_cast<float>(view.height));
	view.eye = view.settings.camera.eye();
	collectItems(view, *assets_);
	orderInView(view);
	return view;
}

void SceneRenderer::render(SurfaceFrame& frame) {
	stats_ = {};
	stats_.frames = ++frames_;
	if(graphs_.size() > maxGraphPrograms) graphs_.clear();
	FrameView view;
	try {
		view = makeView(frame);
	} catch(const std::exception& error) {
		stats_.error = error.what();
		view.settings = viewportSettingsFromJson(frame.params());
	}
	const SurfacePointer* press = nullptr;
	for(const SurfacePointer& pointer : frame.input)
		if(pointer.kind == PointerKind::press) press = &pointer;
	ShadowMap shadow;
	if(stats_.error.empty()) {
		if(press != nullptr) pick(view, *press, frame);
		if(view.settings.shading == Shading::Material) shadow = drawShadow(frame, view);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, frame.framebuffer);
	glViewport(0, 0, frame.width, frame.height);
	const Vec3 background = view.settings.background;
	glClearColor(background.x, background.y, background.z, 1.0f);
	glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if(stats_.error.empty()) {
		if(view.settings.shading == Shading::Wireframe) drawWireframe(view);
		else drawSurfaces(view, shadow);
		drawGrid(view);
		drawOutline(frame, view);
		glBindFramebuffer(GL_FRAMEBUFFER, frame.framebuffer);
		glViewport(0, 0, frame.width, frame.height);
		drawLines(view);
	}
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	resources_.trim(frames_);
	stats_.drawn = view.inView.size();
	stats_.culled = view.culled;
	stats_.gpuBytes = resources_.bytes();
	stats_.evicted = resources_.evicted();
	if(probe_) probe_->stats = stats_;
}

void SceneRenderer::pick(const FrameView& view, const SurfacePointer& pointer, const SurfaceFrame& frame) {
	glBindFramebuffer(GL_FRAMEBUFFER, frame.framebuffer);
	glViewport(0, 0, frame.width, frame.height);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	flat_.use();
	setMatrix(flat_.uniform("viewProjection"), view.viewProjection);
	for(const std::size_t index : view.inView) {
		const DrawItem& item = view.items[index];
		const Vec3 color = idColor(item.index);
		glUniform4f(flat_.uniform("flatColor"), color.x, color.y, color.z, 1.0f);
		drawPlain(item, flat_, false);
	}
	const int x = std::clamp(static_cast<int>(pointer.position.x), 0, std::max(frame.width - 1, 0));
	const int y = std::clamp(frame.height - 1 - static_cast<int>(pointer.position.y), 0, std::max(frame.height - 1, 0));
	std::array<unsigned char, 4> pixel{};
	glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
	const std::uint32_t number = pixel[0] | (std::uint32_t{pixel[1]} << 8) | (std::uint32_t{pixel[2]} << 16);
	IdPick result{pointer.position.x / view.scale, pointer.position.y / view.scale, {}, pointer.mods.shift()};
	if(number >= 1 && number <= view.items.size()) result.node = view.items[number - 1].node->id;
	if(probe_) probe_->pick = std::move(result);
}

ShadowMap SceneRenderer::drawShadow(SurfaceFrame& frame, const FrameView& view) {
	const std::vector<SceneLight> lights = lightsOf(view);
	const auto caster = std::ranges::find_if(lights, [](const SceneLight& light) {
		return light.light.castsShadow && light.light.type != LightType::Point;
	});
	if(caster == lights.end() || view.bounds.empty()) return {};
	ShadowMap shadow{static_cast<int>(caster - lights.begin()), shadowMatrixFor(*caster, view.bounds), 0};
	const SurfacePass pass = frame.pass("shadow", {shadowSize, shadowSize, PassFormat::depth24});
	shadow.texture = pass.texture;
	glBindFramebuffer(GL_FRAMEBUFFER, pass.framebuffer);
	glViewport(0, 0, pass.width, pass.height);
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDisable(GL_CULL_FACE);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);
	depth_.use();
	setMatrix(depth_.uniform("viewProjection"), shadow.matrix);
	for(const DrawItem& item : view.items)
		if(!item.blended) drawPlain(item, depth_, false);
	glDisable(GL_POLYGON_OFFSET_FILL);
	return shadow;
}

const GlProgram& SceneRenderer::programFor(const json* graph) {
	if(graph == nullptr || !graph->is_object()) return pbr_;
	const std::string key = graph->dump();
	auto found = graphs_.find(key);
	if(found == graphs_.end()) {
		GlProgram program;
		const MaterialGraphResult compiled = compileMaterialGraph(*graph);
		if(compiled.ok()) try {
				program = GlProgram(prelude_, {shaders::meshVertex},
				                    {"#define GRAPH\n", shaders::pbrHead, compiled.glsl, shaders::pbrMain});
			} catch(const std::exception&) {
				program = GlProgram(); // a graph the checks passed but the driver refused: the default material
			}
		found = graphs_.emplace(key, std::move(program)).first;
	}
	return found->second.valid() ? found->second : pbr_;
}

void SceneRenderer::setShared(const FrameView& view, const GlProgram& program, const ShadowMap& shadow) const {
	setMatrix(program.uniform("viewProjection"), view.viewProjection);
	glUniform3f(program.uniform("eye"), view.eye.x, view.eye.y, view.eye.z);
	glUniform1f(program.uniform("exposure"), view.settings.exposure);
	glUniform1i(program.uniform("solid"), view.settings.shading == Shading::Solid ? 1 : 0);
	glUniform1i(program.uniform("baseColorMap"), 1);
	glUniform1i(program.uniform("metallicRoughnessMap"), 2);
	glUniform1i(program.uniform("normalMap"), 3);
	glUniform1i(program.uniform("emissiveMap"), 4);
	glUniform1i(program.uniform("shadowMap"), 5);
	glUniform1i(program.uniform("shadowLight"), shadow.light);
	setMatrix(program.uniform("shadowMatrix"), shadow.matrix);
	glUniform1i(program.uniform("lightCount"), setLights(view, program));
}

int SceneRenderer::setLights(const FrameView& view, const GlProgram& program) const {
	const std::vector<SceneLight> lights = lightsOf(view);
	std::array<float, maxLights * 4> positions{}, directions{}, cones{};
	std::array<float, maxLights * 3> colors{};
	for(std::size_t i = 0; i < lights.size(); ++i) {
		const SceneLight& light = lights[i];
		const float type = static_cast<float>(light.light.type); // 0 directional, 1 point, 2 spot
		const Vec3 color = light.light.color * light.light.intensity;
		std::ranges::copy(std::array{light.position.x, light.position.y, light.position.z, type}, &positions[i * 4]);
		std::ranges::copy(std::array{light.direction.x, light.direction.y, light.direction.z, light.light.range},
		                  &directions[i * 4]);
		std::ranges::copy(std::array{color.x, color.y, color.z}, &colors[i * 3]);
		cones[i * 2] = std::cos(light.light.outerCone);
		cones[i * 2 + 1] = std::max(std::cos(light.light.innerCone), cones[i * 2] + 1e-4f);
	}
	const auto count = static_cast<GLsizei>(lights.size());
	glUniform4fv(program.uniform("lightPosition"), count, positions.data());
	glUniform4fv(program.uniform("lightDirection"), count, directions.data());
	glUniform3fv(program.uniform("lightColor"), count, colors.data());
	glUniform2fv(program.uniform("lightCone"), count, cones.data());
	return static_cast<int>(lights.size());
}

void SceneRenderer::drawSurfaces(const FrameView& view, const ShadowMap& shadow) {
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, shadow.texture);
	const GlProgram* current = nullptr;
	for(const std::size_t index : view.inView) {
		const DrawItem& item = view.items[index];
		const json* graph = nullptr;
		if(item.material != nullptr && view.settings.shading == Shading::Material)
			if(const auto found = view.graphs.find(item.material->id); found != view.graphs.end())
				graph = found->second;
		const GlProgram& program = programFor(graph);
		if(&program != current) {
			program.use();
			setShared(view, program, shadow);
			current = &program;
		}
		if(item.blended) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			glDepthMask(GL_FALSE);
		}
		drawShaded(item, program);
		stats_.triangles += item.data->triangleCount();
	}
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
}

void SceneRenderer::drawShaded(const DrawItem& item, const GlProgram& program) {
	const GpuMesh* mesh = resources_.mesh(item.node->mesh, *assets_, frames_, false);
	if(mesh == nullptr) return;
	static const Material fallback{.baseColor = {0.8f, 0.8f, 0.8f, 1.0f}};
	const Material& material = item.material != nullptr ? *item.material : fallback;
	int maps = 0;
	if(mesh->buffers[2] != 0) {
		maps |= bindMap(1, material.baseColorTexture, resources_, *assets_, frames_) ? 1 : 0;
		maps |= bindMap(2, material.metallicRoughnessTexture, resources_, *assets_, frames_) ? 2 : 0;
		maps |= bindMap(3, material.normalTexture, resources_, *assets_, frames_) ? 4 : 0;
		maps |= bindMap(4, material.emissiveTexture, resources_, *assets_, frames_) ? 8 : 0;
	}
	const Vec4& base = material.baseColor;
	glUniform4f(program.uniform("baseColorFactor"), base.x, base.y, base.z, base.w);
	glUniform1f(program.uniform("metallicFactor"), material.metallic);
	glUniform1f(program.uniform("roughnessFactor"), material.roughness);
	glUniform3f(program.uniform("emissiveFactor"), material.emissive.x, material.emissive.y, material.emissive.z);
	glUniform1i(program.uniform("alphaMode"), static_cast<int>(material.alphaMode));
	glUniform1f(program.uniform("alphaCutoff"), material.alphaCutoff);
	glUniform1i(program.uniform("maps"), maps);
	setMatrix(program.uniform("model"), item.world);
	setMatrix(program.uniform("normalMatrix"), item.world.inverse().value_or(item.world).transposed());
	setFaces(item);
	glBindVertexArray(mesh->vertexArray);
	if(mesh->buffers[2] == 0) glVertexAttrib2f(2, 0.0f, 0.0f);
	glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, nullptr);
	if(item.blended) {
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}
}

void SceneRenderer::drawWireframe(const FrameView& view) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	flat_.use();
	setMatrix(flat_.uniform("viewProjection"), view.viewProjection);
	const Vec3 outline = view.settings.outline;
	for(const std::size_t index : view.inView) {
		const DrawItem& item = view.items[index];
		if(item.selected) glUniform4f(flat_.uniform("flatColor"), outline.x, outline.y, outline.z, 1.0f);
		else glUniform4f(flat_.uniform("flatColor"), 0.62f, 0.63f, 0.66f, 0.85f);
		drawPlain(item, flat_, true);
		stats_.triangles += item.data->triangleCount();
	}
	glDisable(GL_BLEND);
}

void SceneRenderer::drawPlain(const DrawItem& item, const GlProgram& program, bool edges) {
	const GpuMesh* mesh = resources_.mesh(item.node->mesh, *assets_, frames_, edges);
	if(mesh == nullptr) return;
	setMatrix(program.uniform("model"), item.world);
	glBindVertexArray(edges ? mesh->edgeArray : mesh->vertexArray);
	if(edges) glDrawElements(GL_LINES, mesh->edgeCount, GL_UNSIGNED_INT, nullptr);
	else glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, nullptr);
}

} // namespace detail

std::unique_ptr<GpuSurface> makeSceneRenderer(std::shared_ptr<const Assets> assets,
                                              std::shared_ptr<ViewportProbe> probe, const GpuBudget& budget) {
	return std::make_unique<detail::SceneRenderer>(std::move(assets), std::move(probe), budget);
}

} // namespace affresco::scene3d
