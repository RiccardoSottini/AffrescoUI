#pragma once

/// @file
/// @brief Adjustments: levels, curves, hue/saturation, colour balance, brightness/contrast, invert, threshold and
/// gradient map, applied to a pixel on the CPU or through a 256-entry table and a few numbers on the GPU; and the
/// gradients that gradient maps and the gradient tool use.

#include "affresco/imaging/Pixels.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::imaging {

/// @brief One colour of a gradient, at a position from 0 to 1.
struct GradientStop {
	float position = 0.0f; ///< Where, 0 to 1.
	Pixel color;           ///< Its colour, straight.

	/// @brief Whether two stops are the same.
	[[nodiscard]] friend bool operator==(const GradientStop&, const GradientStop&) = default;
};

/// @brief A gradient: stops in order of position, linearly interpolated (at most maxStops).
struct Gradient {
	static constexpr std::size_t maxStops = 32; ///< The most stops.
	std::vector<GradientStop> stops{{0.0f, {0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}}; ///< Stops.

	/// @brief The colour at a position, clamped to the ends.
	/// @param position 0 to 1.
	[[nodiscard]] Pixel at(float position) const noexcept;
	/// @brief Sorts the stops by position, clamps them to 0 to 1, and keeps at most maxStops (at least one).
	void normalize();
	/// @brief Whether two gradients are the same.
	[[nodiscard]] friend bool operator==(const Gradient&, const Gradient&) = default;
};

/// @brief What an adjustment does.
enum class AdjustmentKind : std::uint8_t {
	levels,             ///< Input black, white and gamma, then output black and white.
	curves,             ///< A curve through points, on every channel.
	hueSaturation,      ///< Hue turned, saturation and lightness changed.
	colorBalance,       ///< Midtones pushed towards red, green or blue.
	brightnessContrast, ///< Brightness and contrast.
	invert,             ///< The negative.
	threshold,          ///< Black or white, by luminosity.
	gradientMap,        ///< A gradient's colour, by luminosity.
};

/// @brief How many kinds there are.
inline constexpr std::size_t adjustmentKindCount = 8;

/// @brief The kinds' names: "levels", "curves", "hue-saturation", "color-balance", "brightness-contrast", "invert",
/// "threshold", "gradient-map".
[[nodiscard]] const std::array<std::string_view, adjustmentKindCount>& adjustmentKindNames() noexcept;
/// @brief The kind of a name, or nothing.
/// @param name The name.
[[nodiscard]] std::optional<AdjustmentKind> adjustmentKindOf(std::string_view name) noexcept;

/// @brief An adjustment and its settings (those of the other kinds are kept, unused).
struct Adjustment {
	static constexpr std::size_t maxCurvePoints = 16; ///< The most points of a curve.

	AdjustmentKind kind = AdjustmentKind::invert; ///< What it does.
	float inputBlack = 0.0f;                      ///< levels: the input that becomes black.
	float inputWhite = 1.0f;                      ///< levels: the input that becomes white.
	float gamma = 1.0f;                           ///< levels: the midtones' gamma, 0.1 to 10.
	float outputBlack = 0.0f;                     ///< levels: the darkest output.
	float outputWhite = 1.0f;                     ///< levels: the lightest output.
	std::vector<std::pair<float, float>> curve{{0.0f, 0.0f}, {1.0f, 1.0f}}; ///< curves: points (in, out), by in.
	float hue = 0.0f;                             ///< hueSaturation: degrees, -180 to 180.
	float saturation = 0.0f;                      ///< hueSaturation: -1 (grey) to 1 (twice).
	float lightness = 0.0f;                       ///< hueSaturation: -1 (black) to 1 (white).
	std::array<float, 3> balance{};               ///< colorBalance: cyan-red, magenta-green, yellow-blue, -1 to 1.
	float brightness = 0.0f;                      ///< brightnessContrast: -1 to 1.
	float contrast = 0.0f;                        ///< brightnessContrast: -1 to 1.
	float level = 0.5f;                           ///< threshold: the luminosity that becomes white.
	Gradient gradient;                            ///< gradientMap: from the darkest to the lightest.

	/// @brief Clamps every setting to its range and sorts the curve.
	void normalize();
	/// @brief Whether two adjustments are the same.
	[[nodiscard]] friend bool operator==(const Adjustment&, const Adjustment&) = default;
};

/// @brief How the GPU applies an adjustment: through its table per channel, through its table by luminosity, or as
/// a hue/saturation change.
enum class AdjustmentShader : std::uint8_t { perChannel, byLuminosity, hueSaturation };

/// @brief Which way the GPU applies an adjustment.
/// @param adjustment The adjustment.
[[nodiscard]] AdjustmentShader adjustmentShader(const Adjustment& adjustment) noexcept;
/// @brief The adjustment's table: 256 entries of red, green, blue and alpha bytes (per channel, or by luminosity).
/// @param adjustment The adjustment.
[[nodiscard]] std::array<std::uint8_t, 256 * 4> adjustmentTable(const Adjustment& adjustment);
/// @brief Applies an adjustment to a colour (the CPU reference); alpha is kept.
/// @param adjustment The adjustment.
/// @param pixel The colour.
[[nodiscard]] Pixel adjustPixel(const Adjustment& adjustment, const Pixel& pixel) noexcept;
/// @brief The GLSL of hue/saturation: `vec3 adjustHueSaturation(vec3 color, vec3 hueSaturationLightness)`, the
/// same as adjustPixel(), hue in degrees.
[[nodiscard]] std::string_view adjustmentShaderSource() noexcept;

} // namespace affresco::imaging
