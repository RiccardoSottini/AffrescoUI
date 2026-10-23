#pragma once

/// @file
/// @brief Gradient: a linear or radial gradient of colour stops (as in CSS), what panels fill with and the
/// GradientEditor edits, and its maths: normalising, sampling and the position of a point.

#include "affresco/render/Color.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace affresco {

/// @brief One colour stop of a gradient.
struct GradientStop {
	double at = 0.0; ///< Where it is along the gradient, 0..1.
	Color color;     ///< Its colour (straight alpha).

	/// @brief Equal when both members are equal.
	bool operator==(const GradientStop&) const noexcept = default;
};

/// @brief The most stops a gradient keeps.
inline constexpr std::size_t maxGradientStops = 32;

/// @brief A gradient: its kind, its angle and its stops, sorted by position.
struct Gradient {
	/// @brief Its shape.
	enum class Kind {
		Linear, ///< Along a line through the centre, at angle().
		Radial, ///< Outwards from the centre to the farthest corner.
	};

	Kind kind = Kind::Linear;          ///< Its shape.
	double angle = 90.0;               ///< The direction of a linear gradient, in degrees (0 up, 90 right, like CSS).
	std::vector<GradientStop> stops{}; ///< The stops, sorted by at (see normalizeGradient()).

	/// @brief Equal when every member is equal.
	bool operator==(const Gradient&) const = default;
};

/// @brief Brings a gradient within bounds: positions clamped to 0..1 and sorted, at most maxGradientStops (the first
/// ones), at least two (black to white when there were none, the one colour twice when there was one), the angle
/// wrapped to 0..360.
/// @param gradient The gradient, changed in place.
void normalizeGradient(Gradient& gradient);

/// @brief The colour of a gradient at a position, mixing the stops on each side in sRGB (as CSS does).
/// @param stops The stops, sorted by position.
/// @param at The position, 0..1.
/// @return The colour; transparent when there are no stops.
[[nodiscard]] Color sampleGradient(std::span<const GradientStop> stops, double at) noexcept;

/// @brief The position along a gradient of a point of a square it fills.
/// @param gradient Its kind and angle.
/// @param x The point across, 0..1.
/// @param y The point down, 0..1.
/// @return The position, 0..1.
[[nodiscard]] double gradientPosition(const Gradient& gradient, double x, double y) noexcept;

} // namespace affresco
