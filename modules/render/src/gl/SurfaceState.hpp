#pragma once

/// @file
/// @brief detail::SurfaceState: what a SurfaceView shares with the renderer that draws its surface.

#include "affresco/render/GpuSurface.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace affresco::detail {

/// @brief A SurfaceView's surface as the renderer sees it. The view owns it; the renderer holds it weakly, so it frees
/// the GL side of a view that is gone. Main thread.
struct SurfaceState {
	static constexpr std::size_t maxInput = 256; ///< Pointer events kept between renders; older ones are dropped.

	std::string name;             ///< The registered name.
	SurfaceFactory factory;       ///< Makes the surface.
	SurfaceOptions options;       ///< What its framebuffer needs.
	json params = json::object(); ///< The document's parameters.
	bool continuous = false;      ///< Redrawn every frame.
	std::uint64_t version = 1;    ///< Grows on every change to draw: the renderer draws again when it differs.
	std::uint64_t surface = 1;    ///< Grows when the surface itself changes (another name): the renderer remakes it.
	std::vector<SurfacePointer> input; ///< Since the last render, in pixels; the renderer clears it.
	Vec2 size;                         ///< The panel's size, in window units, when last painted.
	float scale = 1.0f;                ///< Pixels per window unit, when last painted.
	double time = 0.0;                 ///< The window clock's seconds at the last redraw asked for.
	bool failed = false;               ///< The surface failed: it draws nothing.
	SurfaceView* view = nullptr;       ///< The view, while it lives.
};

} // namespace affresco::detail
