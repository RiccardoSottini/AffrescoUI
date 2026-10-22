#pragma once

/// @file
/// @brief The PBR forward renderer, as a GpuSurface a screen shows in a `surface` node.
///
/// It reads its parameters with viewportSettingsFromJson() and sceneAtTime() (`scene`, `animations`, `time`,
/// `camera`, `selection`, `shading`, ...), and the meshes and textures the scene names from an Assets store the host
/// shares with it. Per frame: one shadow map (depth24) for the first shadow-casting directional or spot light; the
/// opaque meshes, culled against the view; the blended ones back to front; the floor grid and axes; the selection's
/// outline; light markers, the gizmo and the box being dragged. Colour is linear until the end: exposure, ACES tone
/// mapping, then sRGB. A material's `graph` is compiled by compileMaterialGraph() into a program of fixed snippets:
/// documents never supply shader source. Meshes and textures are uploaded on first use and the least recently used
/// are freed when over a budget of GPU memory.
///
/// On each press it also draws an id pass (each mesh node in its own colour) and reads the pixel under the pointer
/// into the shared ViewportProbe, which the host's picking can use along with a CPU ray (Picking.hpp).

#include "affresco/render/GpuSurface.hpp"
#include "affresco/scene3d/Scene.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace affresco::scene3d {

/// @brief How much GPU memory a renderer's meshes and textures may take.
struct GpuBudget {
	std::size_t bytes = std::size_t{512} << 20; ///< Above this, the least recently used are freed.
};

/// @brief What a renderer drew in its last frame.
struct RenderStats {
	std::size_t drawn = 0;     ///< Mesh nodes drawn.
	std::size_t culled = 0;    ///< Mesh nodes outside the view.
	std::size_t triangles = 0; ///< Triangles drawn (the shadow pass not counted).
	std::size_t gpuBytes = 0;  ///< Bytes of meshes and textures on the GPU.
	std::size_t evicted = 0;   ///< Meshes and textures freed so far to stay within the budget.
	std::uint64_t frames = 0;  ///< Frames drawn.
	std::string error;         ///< Why the scene couldn't be drawn, or empty.
};

/// @brief What the id pass found under the pointer at a press.
struct IdPick {
	float x = 0.0f;     ///< Where, in window units from the surface's left.
	float y = 0.0f;     ///< Where, in window units from its top.
	std::string node;   ///< The mesh node there, or empty for none.
	bool shift = false; ///< Whether Shift was held: add to the selection.
};

/// @brief Shared between a renderer and its host: what it drew and what its id pass found. Not thread-safe: used on
/// the thread that renders.
struct ViewportProbe {
	RenderStats stats;          ///< The last frame.
	std::optional<IdPick> pick; ///< The last press's id pass, if any.
};

/// @brief A renderer.
/// @param assets The meshes and textures, shared with the host (which may add to them between frames).
/// @param probe Receives the stats and the id pass's result; may be null.
/// @param budget The GPU memory budget.
/// @return The surface, to register with Screen::registerSurface() with SurfaceOptions::depth.
[[nodiscard]] std::unique_ptr<GpuSurface> makeSceneRenderer(std::shared_ptr<const Assets> assets,
                                                            std::shared_ptr<ViewportProbe> probe,
                                                            const GpuBudget& budget = {});

} // namespace affresco::scene3d
