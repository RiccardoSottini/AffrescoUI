#pragma once

/// @file
/// @brief Gradients' five shapes and presets (the stops are Adjustment.hpp's, shared with gradient maps), and the
/// fills that lay them or a flat colour over a layer as one step, within the selection: the gradient tool, the paint
/// bucket and Edit > Fill.

#include "affresco/imaging/Adjustment.hpp"
#include "affresco/imaging/Selection.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::imaging {

/// @brief How a gradient spreads from its start to its end.
enum class GradientShape : std::uint8_t {
	linear,    ///< Along the line.
	radial,    ///< Out from the start in circles.
	angle,     ///< Around the start, anticlockwise from the line.
	reflected, ///< Along the line both ways from the start.
	diamond,   ///< Out from the start in squares turned to the line.
};

/// @brief A gradient's shapes' names as stored and named in screens: "linear", "radial", …
[[nodiscard]] std::span<const std::string_view> gradientShapeNames() noexcept;

/// @brief A named gradient, as the gradient editor lists them.
struct GradientPreset {
	std::string_view name; ///< Its name.
	Gradient gradient;     ///< Its stops.
};

/// @brief The built-in presets: black to white, the spectrum, copper and a fade to transparent.
[[nodiscard]] std::span<const GradientPreset> gradientPresets();

/// @brief A gradient's position at a point.
/// @param shape The shape.
/// @param from The start.
/// @param to The end.
/// @param x The point across.
/// @param y The point down.
/// @return 0 to 1.
[[nodiscard]] float gradientPosition(GradientShape shape, Point from, Point to, float x, float y) noexcept;

/// @brief Lays a gradient over a layer as one step (within the selection when there is one).
/// @param document The document.
/// @param target The image.
/// @param gradient The gradient.
/// @param shape Its shape.
/// @param from Where it starts.
/// @param to Where it ends.
/// @param opacity 0 to 1.
/// @throws DocumentError when the target can't be painted.
void drawGradient(Document& document, std::uint64_t target, const Gradient& gradient, GradientShape shape, Point from,
                  Point to, float opacity);

/// @brief Lays a colour over a coverage of a layer as one step (within the selection when there is one).
/// @param document The document.
/// @param target The image.
/// @param coverage Where, and how much.
/// @param colour The colour.
/// @param opacity 0 to 1.
/// @param name The step's name.
/// @throws DocumentError when the target can't be painted.
void fillCoverage(Document& document, std::uint64_t target, const Coverage& coverage, const Pixel& colour,
                  float opacity, const std::string& name);

/// @brief The paint bucket: fills the pixels like the one clicked (the magic wand's) with a colour, one step.
/// @param document The document.
/// @param target The image.
/// @param x The pixel across.
/// @param y The pixel down.
/// @param colour The colour.
/// @param options The tolerance, contiguity and layers sampled.
/// @param opacity 0 to 1.
/// @throws DocumentError when the target can't be painted, or the region is beyond floodPixels.
void fillAt(Document& document, std::uint64_t target, int x, int y, const Pixel& colour, const SampleOptions& options,
            float opacity);

/// @brief Edit > Fill: fills the selection, or everything without one, with a colour, one step.
/// @param document The document.
/// @param target The image.
/// @param colour The colour.
/// @param opacity 0 to 1.
/// @throws DocumentError when the target can't be painted.
void fillSelection(Document& document, std::uint64_t target, const Pixel& colour, float opacity);

} // namespace affresco::imaging
