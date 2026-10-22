#pragma once

/// @file
/// @brief The canvas's programs (private to the imaging module): compositing a layer, and showing the composite.

#include <string_view>

namespace affresco::imaging::canvas {

/// @brief The texture units the layer program reads.
enum Unit : int {
	sourceUnit = 0,   ///< The layer's tile, or a group's pass.
	maskUnit = 1,     ///< The mask's tile.
	backdropUnit = 2, ///< The copy of the pass below.
	baseUnit = 3,     ///< The clipping base.
	tableUnit = 4,    ///< The adjustment's table.
};

/// @brief What a layer program's fragment reads as its colour.
enum SourceKind : int {
	tileSource = 0,     ///< A tile, at the cell's pixel.
	colorSource = 1,    ///< One colour.
	passSource = 2,     ///< A pass, at the region's pixel (a group).
	adjustedSource = 3, ///< The backdrop, adjusted.
};

/// @brief Where a layer program's coverage comes from.
enum MaskKind : int {
	noMask = 0,    ///< Covered everywhere.
	tileMask = 1,  ///< A mask8 tile's red.
	valueMask = 2, ///< One value (a transparent mask tile is 0).
};

/// @brief The layer program's adjustKind past the AdjustmentShader values: a threshold at the uniform level.
inline constexpr int thresholdAdjust = 3;

/// @brief What a layer program writes.
enum Stage : int {
	blendStage = 0,    ///< The layer blended onto the backdrop.
	baseStage = 1,     ///< Its own coverage, the next clipped layers' base.
	coverageStage = 2, ///< The mask's coverage alone (the selection).
};

/// @brief Compiles and links the layer program.
/// @param prelude The version line.
/// @return The program.
/// @throws std::runtime_error with the log when it doesn't compile or link.
[[nodiscard]] unsigned layerProgram(std::string_view prelude);

/// @brief Compiles and links the display program.
/// @param prelude The version line.
/// @return The program.
/// @throws std::runtime_error with the log when it doesn't compile or link.
[[nodiscard]] unsigned displayProgram(std::string_view prelude);

} // namespace affresco::imaging::canvas
