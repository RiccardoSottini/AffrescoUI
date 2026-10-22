/// @file
/// @brief What the renderer draws over the scene (Renderer.hpp): the floor grid and axes, the selection's outline, the
/// gizmo, light and camera markers, and the box being dragged.

#include "RendererInternal.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace affresco::scene3d::detail {

namespace {

constexpr Vec4 activeColor{1.0f, 0.85f, 0.2f, 1.0f}; ///< The handle being dragged.
constexpr std::array<Vec4, 3> axisColors{Vec4{0.94f, 0.3f, 0.33f, 1.0f}, Vec4{0.55f, 0.82f, 0.2f, 1.0f},
                                         Vec4{0.27f, 0.55f, 0.95f, 1.0f}}; ///< x, y and z.
constexpr Vec4 markerColor{0.95f, 0.85f, 0.45f, 0.9f};                     ///< Lights and cameras.
constexpr int ringSegments = 48;                                           ///< Per rotate ring.

/// @brief Coloured line segments, as the line program takes them.
class Lines {
public:
	/// @brief Adds a segment. @param a From. @param b To. @param color Its colour.
	void add(Vec3 a, Vec3 b, Vec4 color) {
		for(const Vec3 p : {a, b})
			for(const float value : {p.x, p.y, p.z, color.x, color.y, color.z, color.w}) vertices_.push_back(value);
	}
	/// @brief Adds a circle. @param center Its centre. @param u One radius. @param v The other, at right angles.
	/// @param color Its colour.
	void circle(Vec3 center, Vec3 u, Vec3 v, Vec4 color) {
		for(int i = 0; i < ringSegments; ++i) {
			const auto at = [&](int k) {
				const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(k) / ringSegments;
				return center + u * std::cos(angle) + v * std::sin(angle);
			};
			add(at(i), at(i + 1), color);
		}
	}
	/// @brief The vertices. @return x, y, z, r, g, b, a each.
	[[nodiscard]] const std::vector<float>& vertices() const noexcept { return vertices_; }

private:
	std::vector<float> vertices_; ///< See vertices().
};

/// @brief Whether a gizmo handle is the one highlighted. @param gizmo The settings. @param handle It.
/// @return true if so.
bool highlighted(const GizmoSettings& gizmo, GizmoAxis handle) noexcept { return gizmo.active == handle; }

/// @brief A handle's colour. @param settings The gizmo's settings. @param handle It. @param base Its own colour.
/// @return The colour.
Vec4 handleColor(const GizmoSettings& settings, GizmoAxis handle, Vec4 base) noexcept {
	return highlighted(settings, handle) ? activeColor : base;
}

/// @brief Adds the gizmo's lines. @param lines The list. @param gizmo It. @param settings Its settings.
/// @param toEye From its origin to the eye, normalized.
void addGizmo(Lines& lines, const Gizmo& gizmo, const GizmoSettings& settings, Vec3 toEye) {
	constexpr std::array<GizmoAxis, 3> axes{GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z};
	constexpr std::array<GizmoAxis, 3> planes{GizmoAxis::XY, GizmoAxis::YZ, GizmoAxis::ZX};
	const Vec3 o = gizmo.origin;
	const float s = gizmo.size;
	for(int i = 0; i < 3; ++i) {
		const Vec3 a = gizmo.axis(i);
		const Vec3 u = gizmo.axis((i + 1) % 3);
		const Vec3 v = gizmo.axis((i + 2) % 3);
		const Vec4 color =
		    handleColor(settings, axes[static_cast<std::size_t>(i)], axisColors[static_cast<std::size_t>(i)]);
		if(gizmo.mode == GizmoMode::Rotate) {
			lines.circle(o, u * s, v * s, color);
			continue;
		}
		lines.add(o, o + a * s, color);
		const Vec3 tip = o + a * s;
		const float r = s * 0.05f;
		if(gizmo.mode == GizmoMode::Translate)
			for(const Vec3 side : {u, -u, v, -v}) lines.add(tip, tip - a * (r * 3.0f) + side * r, color);
		else lines.circle(tip, u * r, v * r, color);
		const Vec4 plane =
		    handleColor(settings, planes[static_cast<std::size_t>(i)], Vec4{color.x, color.y, color.z, 0.8f});
		const std::array<Vec3, 4> corners{o + a * (0.2f * s) + u * (0.2f * s), o + a * (0.45f * s) + u * (0.2f * s),
		                                  o + a * (0.45f * s) + u * (0.45f * s), o + a * (0.2f * s) + u * (0.45f * s)};
		for(std::size_t k = 0; k < 4; ++k) lines.add(corners[k], corners[(k + 1) % 4], plane);
	}
	if(gizmo.mode != GizmoMode::Rotate) {
		const Vec3 side = normalize(cross(toEye, std::abs(toEye.y) < 0.99f ? Vec3{0, 1, 0} : Vec3{1, 0, 0}));
		const Vec3 up = cross(side, toEye);
		lines.circle(o, side * (s * 0.12f), up * (s * 0.12f),
		             handleColor(settings, GizmoAxis::View, Vec4{0.9f, 0.9f, 0.92f, 0.9f}));
	}
}

/// @brief Adds a marker for each light and camera. @param lines The list. @param view The frame.
void addMarkers(Lines& lines, const FrameView& view) {
	const float height = static_cast<float>(view.height) / view.scale;
	for(const Node& node : view.scene.nodes) {
		if(!node.light && !node.camera) continue;
		const auto world = view.worlds.find(node.id);
		if(world == view.worlds.end() || !world->second.second) continue;
		const auto [position, direction] = placement(world->second.first);
		const float s = worldSizeForPixels(view.settings.camera, position, height, 14.0f);
		for(int i = 0; i < 3; ++i) {
			Vec3 offset;
			offset[i] = s * 0.5f;
			lines.add(position - offset, position + offset, markerColor);
		}
		const bool aimed =
		    node.light.transform([](const Light& light) { return light.type != LightType::Point; }).value_or(false);
		if(node.camera || aimed) lines.add(position, position + direction * (s * 4.0f), markerColor);
	}
}

/// @brief A matrix from window units (y down) over the viewport to clip space. @param view The frame.
/// @return The matrix.
Mat4 windowToClip(const FrameView& view) noexcept {
	Mat4 m;
	m.at(0, 0) = 2.0f * view.scale / static_cast<float>(view.width);
	m.at(1, 1) = -2.0f * view.scale / static_cast<float>(view.height);
	m.at(0, 3) = -1.0f;
	m.at(1, 3) = 1.0f;
	return m;
}

} // namespace

void SceneRenderer::drawGrid(const FrameView& view) {
	if(!view.settings.grid && !view.settings.axes) return;
	const auto inverse = view.viewProjection.inverse();
	if(!inverse) return;
	const float distance = std::max(view.settings.camera.distance, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	grid_.use();
	setMatrix(grid_.uniform("inverseViewProjection"), *inverse);
	setMatrix(grid_.uniform("viewProjection"), view.viewProjection);
	const Vec3 color = view.settings.gridColor;
	glUniform3f(grid_.uniform("gridColor"), color.x, color.y, color.z);
	glUniform1f(grid_.uniform("cell"), std::pow(10.0f, std::floor(std::log10(distance / 4.0f + 1e-6f))));
	glUniform1f(grid_.uniform("fade"), std::max(distance * 8.0f, 10.0f));
	glUniform1i(grid_.uniform("showGrid"), view.settings.grid ? 1 : 0);
	glUniform1i(grid_.uniform("showAxes"), view.settings.axes ? 1 : 0);
	glBindVertexArray(emptyArray_);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glDisable(GL_BLEND);
}

void SceneRenderer::drawOutline(SurfaceFrame& frame, const FrameView& view) {
	if(view.settings.shading == Shading::Wireframe) return;
	const auto selected = std::ranges::count_if(view.inView, [&](std::size_t i) { return view.items[i].selected; });
	if(selected == 0) return;
	const SurfacePass mask = frame.pass("outline", {frame.width, frame.height, PassFormat::rgba8});
	glBindFramebuffer(GL_FRAMEBUFFER, mask.framebuffer);
	glViewport(0, 0, mask.width, mask.height);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_MAX);
	flat_.use();
	setMatrix(flat_.uniform("viewProjection"), view.viewProjection);
	for(const std::size_t index : view.inView) {
		const DrawItem& item = view.items[index];
		if(!item.selected) continue;
		glUniform4f(flat_.uniform("flatColor"), 1.0f, item.active ? 1.0f : 0.0f, 0.0f, 1.0f);
		drawPlain(item, flat_, false);
	}
	glBlendEquation(GL_FUNC_ADD);
	glBindFramebuffer(GL_FRAMEBUFFER, frame.framebuffer);
	glViewport(0, 0, frame.width, frame.height);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	outline_.use();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mask.texture);
	glUniform1i(outline_.uniform("mask"), 0);
	const Vec3 color = view.settings.outline;
	glUniform3f(outline_.uniform("outlineColor"), color.x, color.y, color.z);
	glUniform1f(outline_.uniform("radius"), view.scale >= 1.5f ? 2.0f : 1.5f);
	glBindVertexArray(emptyArray_);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDisable(GL_BLEND);
}

void SceneRenderer::drawLines(const FrameView& view) {
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	Lines world;
	addMarkers(world, view);
	if(const auto gizmo = gizmoFor(view.scene, view.settings, static_cast<float>(view.height) / view.scale))
		addGizmo(world, *gizmo, view.settings.gizmo, normalize(view.eye - gizmo->origin));
	drawLineList(world.vertices(), view.viewProjection);
	if(const auto& box = view.settings.box) {
		Lines flat;
		const auto [x0, y0, x1, y1] = *box;
		const Vec4 color{view.settings.outline.x, view.settings.outline.y, view.settings.outline.z, 1.0f};
		const std::array<Vec3, 4> corners{Vec3{x0, y0, 0}, Vec3{x1, y0, 0}, Vec3{x1, y1, 0}, Vec3{x0, y1, 0}};
		for(std::size_t k = 0; k < 4; ++k) flat.add(corners[k], corners[(k + 1) % 4], color);
		drawLineList(flat.vertices(), windowToClip(view));
	}
	glDisable(GL_BLEND);
}

void SceneRenderer::drawLineList(const std::vector<float>& vertices, const Mat4& viewProjection) {
	if(vertices.empty()) return;
	line_.use();
	setMatrix(line_.uniform("viewProjection"), viewProjection);
	glBindVertexArray(lineArray_);
	glBindBuffer(GL_ARRAY_BUFFER, lineBuffer_);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(),
	             GL_STREAM_DRAW);
	glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 7));
}

} // namespace affresco::scene3d::detail
