/// @file
/// @brief The imaging module's blend modes and adjustments (the CPU reference; the GPU is compared with it in
/// tests/unit/imaging_gpu.cpp).

#include "affresco/imaging/Adjustment.hpp"
#include "affresco/imaging/Blend.hpp"

#include <doctest/doctest.h>

#include <cmath>

using namespace affresco::imaging;

namespace {

/// @brief Whether two colours are within a tolerance, channel by channel.
/// @param left One.
/// @param right The other.
/// @param tolerance The largest difference.
[[nodiscard]] bool near(const Pixel& left, const Pixel& right, float tolerance = 1e-4f) {
	return std::abs(left.red - right.red) <= tolerance && std::abs(left.green - right.green) <= tolerance &&
	       std::abs(left.blue - right.blue) <= tolerance && std::abs(left.alpha - right.alpha) <= tolerance;
}

/// @brief The luminosity the non-separable modes keep.
/// @param pixel The colour.
[[nodiscard]] float luminosity(const Pixel& pixel) {
	return 0.3f * pixel.red + 0.59f * pixel.green + 0.11f * pixel.blue;
}

} // namespace

TEST_CASE("imaging blend: the twelve modes have names both ways") {
	CHECK(blendModeNames().size() == 12);
	for(std::size_t index = 0; index < blendModeCount; ++index) {
		const auto mode = static_cast<BlendMode>(index);
		CHECK(blendModeOf(blendModeName(mode)) == mode);
	}
	CHECK_FALSE(blendModeOf("dissolve").has_value());
	CHECK(blendShaderSource().find("vec4 blendPixels(") != std::string_view::npos);
}

TEST_CASE("imaging blend: the separable modes' identities") {
	const Pixel colour{0.2f, 0.5f, 0.8f, 1.0f};
	const Pixel white{1.0f, 1.0f, 1.0f, 1.0f}, black{0.0f, 0.0f, 0.0f, 1.0f};
	CHECK(near(blendColors(BlendMode::multiply, colour, white), {0.2f, 0.5f, 0.8f, 1.0f}));
	CHECK(near(blendColors(BlendMode::screen, colour, black), {0.2f, 0.5f, 0.8f, 1.0f}));
	CHECK(near(blendColors(BlendMode::difference, colour, colour), {0.0f, 0.0f, 0.0f, 1.0f}));
	CHECK(near(blendColors(BlendMode::darken, colour, Pixel{0.5f, 0.5f, 0.5f, 1.0f}), {0.2f, 0.5f, 0.5f, 1.0f}));
	CHECK(near(blendColors(BlendMode::lighten, colour, Pixel{0.5f, 0.5f, 0.5f, 1.0f}), {0.5f, 0.5f, 0.8f, 1.0f}));
	// Overlay over black is black; soft light with grey changes nothing
	CHECK(near(blendColors(BlendMode::overlay, black, colour), {0.0f, 0.0f, 0.0f, 1.0f}));
	CHECK(near(blendColors(BlendMode::softLight, colour, Pixel{0.5f, 0.5f, 0.5f, 1.0f}), {0.2f, 0.5f, 0.8f, 1.0f}));
}

TEST_CASE("imaging blend: the non-separable modes keep the luminosity they should") {
	const Pixel below{0.8f, 0.3f, 0.1f, 1.0f}, above{0.1f, 0.4f, 0.9f, 1.0f};
	CHECK(luminosity(blendColors(BlendMode::color, below, above)) == doctest::Approx(luminosity(below)).epsilon(1e-3));
	CHECK(luminosity(blendColors(BlendMode::hue, below, above)) == doctest::Approx(luminosity(below)).epsilon(1e-3));
	CHECK(luminosity(blendColors(BlendMode::luminosity, below, above)) ==
	      doctest::Approx(luminosity(above)).epsilon(1e-3));
	const Pixel grey = blendColors(BlendMode::saturation, below, Pixel{0.5f, 0.5f, 0.5f, 1.0f});
	CHECK(grey.red == doctest::Approx(grey.blue));
}

TEST_CASE("imaging blend: alpha, opacity and transparent backdrops") {
	const Pixel below{1.0f, 0.0f, 0.0f, 1.0f}, above{0.0f, 0.0f, 1.0f, 1.0f};
	CHECK(near(blendPixels(below, above, BlendMode::normal, 0.0f), below));
	CHECK(near(blendPixels(below, above, BlendMode::normal, 1.0f), above));
	CHECK(near(blendPixels(below, above, BlendMode::normal, 0.5f), {0.5f, 0.0f, 0.5f, 1.0f}));
	// Nothing below: any mode shows the layer as it is
	for(std::size_t index = 0; index < blendModeCount; ++index)
		CHECK(near(blendPixels(Pixel{}, above, static_cast<BlendMode>(index), 1.0f), above));
	const Pixel half = blendPixels(Pixel{}, Pixel{0.0f, 1.0f, 0.0f, 0.5f}, BlendMode::multiply, 1.0f);
	CHECK(near(half, {0.0f, 1.0f, 0.0f, 0.5f}));
	CHECK(near(blendPixels(Pixel{}, Pixel{}, BlendMode::screen, 1.0f), Pixel{}));
}

TEST_CASE("imaging adjustments: identities, invert, threshold and the gradient map") {
	const Pixel colour{0.25f, 0.5f, 0.75f, 0.6f};
	for(const AdjustmentKind kind : {AdjustmentKind::levels, AdjustmentKind::curves, AdjustmentKind::hueSaturation,
	                                 AdjustmentKind::colorBalance, AdjustmentKind::brightnessContrast}) {
		Adjustment adjustment;
		adjustment.kind = kind;
		CHECK(near(adjustPixel(adjustment, colour), colour, 2e-3f));
	}
	const Adjustment invert;
	CHECK(near(adjustPixel(invert, adjustPixel(invert, colour)), colour));
	CHECK(near(adjustPixel(invert, colour), {0.75f, 0.5f, 0.25f, 0.6f}));
	Adjustment threshold;
	threshold.kind = AdjustmentKind::threshold;
	CHECK(near(adjustPixel(threshold, colour), {0.0f, 0.0f, 0.0f, 0.6f}));
	threshold.level = 0.3f;
	CHECK(near(adjustPixel(threshold, colour), {1.0f, 1.0f, 1.0f, 0.6f}));
	Adjustment map;
	map.kind = AdjustmentKind::gradientMap;
	const Pixel grey = adjustPixel(map, colour);
	CHECK(grey.red == doctest::Approx(luminosity(colour)).epsilon(1e-3));
	CHECK(grey.red == doctest::Approx(grey.blue));
}

TEST_CASE("imaging adjustments: settings are clamped and tables follow the formulas") {
	Adjustment levels;
	levels.kind = AdjustmentKind::levels;
	levels.inputBlack = 0.2f, levels.inputWhite = 0.8f, levels.gamma = 1000.0f;
	levels.normalize();
	CHECK(levels.gamma == 10.0f);
	const auto table = adjustmentTable(levels);
	CHECK(table[0] == 0);
	CHECK(table[std::size_t{255} * 4] == 255);
	CHECK(table[std::size_t{100} * 4] == byteFromUnit(adjustPixel(levels, {100.0f / 255.0f, 0.0f, 0.0f, 1.0f}).red));
	CHECK(adjustmentShader(levels) == AdjustmentShader::perChannel);

	Adjustment curves;
	curves.kind = AdjustmentKind::curves;
	curves.curve = {{1.0f, 0.0f}, {0.0f, 1.0f}, {0.5f, 0.5f}};
	curves.normalize();
	CHECK(curves.curve.front().first == 0.0f);
	CHECK(adjustPixel(curves, Pixel{0.0f, 0.0f, 0.0f, 1.0f}).red == doctest::Approx(1.0f));
	CHECK(adjustPixel(curves, Pixel{1.0f, 1.0f, 1.0f, 1.0f}).red == doctest::Approx(0.0f));

	Gradient gradient;
	gradient.stops.assign(40, GradientStop{2.0f, {2.0f, -1.0f, 0.5f, 1.0f}});
	gradient.normalize();
	CHECK(gradient.stops.size() == Gradient::maxStops);
	CHECK(gradient.stops.front().position == 1.0f);
	CHECK(near(gradient.at(0.0f), {1.0f, 0.0f, 0.5f, 1.0f}));
	CHECK(adjustmentKindOf("gradient-map") == AdjustmentKind::gradientMap);
	CHECK_FALSE(adjustmentKindOf("posterize").has_value());
}
