#pragma once

/// @file
/// @brief The renderer's class, shared by Renderer.cpp (the scene, the id pass and the shadow map) and
/// RendererOverlays.cpp (the grid, the outline and the lines).

#include "GlProgram.hpp"
#include "GpuResources.hpp"
#include "affresco/scene3d/Renderer.hpp"
#include "affresco/scene3d/Viewport.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::scene3d::detail {

/// @brief One mesh node to draw.
struct DrawItem {
	const Node* node = nullptr;           ///< The node.
	const Material* material = nullptr;   ///< Its material, or null for the default.
	Mat4 world;                           ///< Its world matrix.
	std::shared_ptr<const MeshData> data; ///< Its mesh's data.
	Aabb bounds;                          ///< In world space.
	std::uint32_t index = 0;              ///< Its number in the id pass (from 1).
	bool selected = false;                ///< In the selection.
	bool active = false;                  ///< The selection's last.
	bool blended = false;                 ///< Drawn after the opaque ones, back to front.
	float distance = 0.0f;                ///< From the eye to its bounds' centre.
};

/// @brief What one frame shows.
struct FrameView {
	Scene scene;                                                      ///< At the frame's time.
	ViewportSettings settings;                                        ///< The camera, the selection and the rest.
	std::map<std::string, std::pair<Mat4, bool>, std::less<>> worlds; ///< World matrices and visibility, by node.
	std::vector<DrawItem> items;                                      ///< Every visible mesh node, in scene order.
	std::vector<std::size_t> inView; ///< The items in the view: the opaque in scene order, then the blended far first.
	std::size_t culled = 0;          ///< Items outside the view.
	std::map<std::string, const json*, std::less<>> graphs; ///< Material graphs by material id, in the parameters.
	Aabb bounds;                                            ///< Of every item.
	Mat4 viewProjection;                                    ///< The camera's.
	Vec3 eye;                                               ///< The camera's position.
	int width = 1;                                          ///< In pixels.
	int height = 1;                                         ///< In pixels.
	float scale = 1.0f;                                     ///< Pixels per window unit.
};

/// @brief The shadow map drawn this frame, if any.
struct ShadowMap {
	int light = -1;       ///< The light's index in the uniforms, or -1 for none.
	Mat4 matrix;          ///< World to the light's clip space.
	unsigned texture = 0; ///< The depth texture.
};

/// @brief The renderer behind makeSceneRenderer().
class SceneRenderer final : public GpuSurface {
public:
	/// @brief See makeSceneRenderer(). @param assets The store. @param probe The probe, or null.
	/// @param budget The GPU memory budget.
	SceneRenderer(std::shared_ptr<const Assets> assets, std::shared_ptr<ViewportProbe> probe, const GpuBudget& budget);
	~SceneRenderer() override = default;
	SceneRenderer(const SceneRenderer&) = delete;
	SceneRenderer& operator=(const SceneRenderer&) = delete;

	void prepare(const SurfaceSetup& setup) override;
	void render(SurfaceFrame& frame) override;
	void release() override;

private:
	/// @brief Reads the parameters and lays out the scene. @param frame The frame. @return What it shows.
	/// @throws std::exception If the scene is malformed.
	[[nodiscard]] FrameView makeView(const SurfaceFrame& frame) const;
	/// @brief Draws the id pass and reads the node under a press. @param view The frame.
	/// @param pointer The press. @param frame The frame (its framebuffer is cleared again after).
	void pick(const FrameView& view, const SurfacePointer& pointer, const SurfaceFrame& frame);
	/// @brief Draws the shadow map for the first light that casts one. @param frame The frame.
	/// @param view What it shows. @return The map, or none.
	[[nodiscard]] ShadowMap drawShadow(SurfaceFrame& frame, const FrameView& view);
	/// @brief Draws the meshes shaded by material or solid. @param view The frame. @param shadow The shadow map.
	void drawSurfaces(const FrameView& view, const ShadowMap& shadow);
	/// @brief Draws one mesh with the PBR program. @param item It. @param program The program, in use.
	void drawShaded(const DrawItem& item, const GlProgram& program);
	/// @brief Sets the lights' uniforms. @param view The frame. @param program The program, in use.
	/// @return The number of lights.
	static int setLights(const FrameView& view, const GlProgram& program);
	/// @brief The program for a material: its graph's if it has a valid one, else the default.
	/// @param graph The material's graph, or null. @return The program.
	[[nodiscard]] const GlProgram& programFor(const json* graph);
	/// @brief Sets the uniforms every mesh in a frame shares. @param view The frame. @param program In use.
	/// @param shadow The shadow map.
	static void setShared(const FrameView& view, const GlProgram& program, const ShadowMap& shadow);
	/// @brief Draws the meshes' edges. @param view The frame.
	void drawWireframe(const FrameView& view);
	/// @brief Draws a mesh with one program's position attribute only. @param item It. @param program In use.
	/// @param edges Its edges rather than its triangles.
	void drawPlain(const DrawItem& item, const GlProgram& program, bool edges);

	// RendererOverlays.cpp
	/// @brief Draws the floor grid and axes. @param view The frame.
	void drawGrid(const FrameView& view);
	/// @brief Draws the selection's outline. @param frame The frame. @param view What it shows.
	void drawOutline(SurfaceFrame& frame, const FrameView& view);
	/// @brief Draws the gizmo, light and camera markers and the box. @param view The frame.
	void drawLines(const FrameView& view);
	/// @brief Uploads and draws coloured lines. @param vertices x, y, z, r, g, b, a each.
	/// @param viewProjection Their transform.
	void drawLineList(const std::vector<float>& vertices, const Mat4& viewProjection);

	std::shared_ptr<const Assets> assets_;                 ///< The meshes and textures.
	std::shared_ptr<ViewportProbe> probe_;                 ///< Receives stats and picks, or null.
	GpuResources resources_;                               ///< Uploaded meshes and textures.
	std::string prelude_;                                  ///< The shaders' first line.
	GlProgram pbr_;                                        ///< The default material program.
	GlProgram flat_;                                       ///< One colour: the id pass, the mask, the wireframe.
	GlProgram depth_;                                      ///< The shadow map.
	GlProgram grid_;                                       ///< The floor grid.
	GlProgram outline_;                                    ///< The outline from the mask.
	GlProgram line_;                                       ///< Coloured lines.
	std::map<std::string, GlProgram, std::less<>> graphs_; ///< Material graph programs by the graph's JSON.
	unsigned emptyArray_ = 0;                              ///< For fullscreen triangles.
	unsigned lineArray_ = 0;                               ///< For lines.
	unsigned lineBuffer_ = 0;                              ///< Their vertices.
	std::uint64_t frames_ = 0;                             ///< Frames drawn.
	RenderStats stats_;                                    ///< This frame's.
};

/// @brief Uploads a matrix uniform. @param location It. @param matrix The matrix.
void setMatrix(int location, const Mat4& matrix) noexcept;
/// @brief The colour that numbers a node in the id pass. @param index From 1. @return RGB in 0..1.
[[nodiscard]] Vec3 idColor(std::uint32_t index) noexcept;
/// @brief An sRGB colour 0..1 to linear. @param color It. @return Linear.
[[nodiscard]] Vec3 srgbToLinear(Vec3 color) noexcept;

} // namespace affresco::scene3d::detail
