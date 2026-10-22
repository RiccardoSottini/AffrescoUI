#pragma once

/// @file
/// @brief Tempo's visualiser: a GPU surface (affresco::GpuSurface) that draws a spectrum of bars for the track
/// playing, made up from the time and the level the screen passes (no audio is analysed).

#include <affresco/Screen.hpp>

namespace examples::tempo {

/// @brief Registers "tempo.visualiser" for "surface" nodes.
///
/// Its parameters: "time" (seconds into the track), "level" (0 to 1; 0 draws the bars at rest), "bars" (24, 4 to
/// 96), "color" and "background" ([red, green, blue] from 0 to 1). The same parameters always draw the same picture.
/// @param screen The screen.
void registerVisualiser(affresco::Screen& screen);

} // namespace examples::tempo
