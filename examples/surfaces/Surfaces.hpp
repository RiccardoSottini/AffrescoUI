#pragma once

/// @file
/// @brief Three GPU surfaces (affresco::GpuSurface) for the "surface" node type: a shader drawn over the whole
/// surface, a depth-tested spinning cube that a drag turns, and a two-pass blur. They show how a host adds native
/// drawing: implement GpuSurface, register it with Screen::registerSurface().

#include <affresco/Screen.hpp>

#include <optional>

namespace examples {

/// @brief Registers "plasma", "cube" and "blur".
///
/// Their parameters: plasma takes "speed" (1); cube takes "speed" (1, turns per ten seconds) and "angle" (degrees,
/// which fixes it); blur takes "radius" (4, in pixels). Each takes "time" (seconds), which fixes its clock.
/// @param screen The screen.
/// @param fixedTime If given, the time every surface draws at, whatever the window's clock says (the rendering
/// check's pictures stay the same from run to run).
void registerSurfaces(affresco::Screen& screen, std::optional<double> fixedTime = std::nullopt);

} // namespace examples
