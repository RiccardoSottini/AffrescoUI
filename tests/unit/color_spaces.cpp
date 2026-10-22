/// @file
/// @brief Unit tests for the colour-space maths: sRGB transfer, HSV, HSL, OKLab and OKLCH round trips over a grid of
/// colours, known reference values, and gamut mapping.

#include <affresco/render/ColorSpaces.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <limits>

using namespace affresco::color;

namespace {

/// @brief Whether two sRGB colours match within a tolerance.
/// @param left One colour.
/// @param right The other.
/// @param tolerance The largest allowed difference per component.
/// @return True when every component is within the tolerance.
bool near(Rgb left, Rgb right, double tolerance = 1e-6) {
	return std::abs(left.red - right.red) <= tolerance && std::abs(left.green - right.green) <= tolerance &&
	       std::abs(left.blue - right.blue) <= tolerance;
}

/// @brief Calls a check for every colour in an 11-step grid over the sRGB cube.
/// @param check Called with each colour.
template <typename Check> void forEachGridColor(Check check) {
	for(int red = 0; red <= 10; ++red) {
		for(int green = 0; green <= 10; ++green) {
			for(int blue = 0; blue <= 10; ++blue) {
				check(Rgb{red / 10.0, green / 10.0, blue / 10.0});
			}
		}
	}
}

} // namespace

TEST_CASE("sRGB transfer round-trips and matches known points") {
	CHECK(srgbToLinear(0.0) == doctest::Approx(0.0));
	CHECK(srgbToLinear(1.0) == doctest::Approx(1.0));
	CHECK(srgbToLinear(0.5) == doctest::Approx(0.214041).epsilon(1e-5));
	for(int step = 0; step <= 100; ++step) {
		const double value = step / 100.0;
		CHECK(linearToSrgb(srgbToLinear(value)) == doctest::Approx(value).epsilon(1e-9));
	}
	CHECK(near(linearToSrgb(srgbToLinear(Rgb{0.2, 0.5, 0.9})), Rgb{0.2, 0.5, 0.9}));
}

TEST_CASE("HSV and HSL round-trip over the sRGB cube") {
	forEachGridColor([](Rgb color) {
		CHECK(near(fromHsv(toHsv(color)), color));
		CHECK(near(fromHsl(toHsl(color)), color));
	});
}

TEST_CASE("HSV and HSL match known values") {
	const Hsv red = toHsv({1.0, 0.0, 0.0});
	CHECK(red.hue == doctest::Approx(0.0));
	CHECK(red.saturation == doctest::Approx(1.0));
	CHECK(red.value == doctest::Approx(1.0));
	CHECK(toHsv({0.0, 1.0, 0.0}).hue == doctest::Approx(120.0));
	CHECK(toHsv({0.0, 0.0, 1.0}).hue == doctest::Approx(240.0));
	const Hsl grey = toHsl({0.5, 0.5, 0.5});
	CHECK(grey.saturation == doctest::Approx(0.0));
	CHECK(grey.lightness == doctest::Approx(0.5));
	CHECK(near(fromHsl({120.0, 1.0, 0.25}), Rgb{0.0, 0.5, 0.0}));
	CHECK(near(fromHsv({-60.0, 1.0, 1.0}), Rgb{1.0, 0.0, 1.0})); // Hues wrap.
	CHECK(near(fromHsv({420.0, 1.0, 1.0}), Rgb{1.0, 1.0, 0.0}));
}

TEST_CASE("OKLab and OKLCH round-trip over the sRGB cube") {
	forEachGridColor([](Rgb color) {
		CHECK(near(fromOkLab(toOkLab(color)), color, 1e-6));
		CHECK(near(fromOkLch(toOkLch(color)), color, 1e-6));
	});
}

TEST_CASE("OKLab matches reference values") {
	const OkLab white = toOkLab(Rgb{1.0, 1.0, 1.0});
	CHECK(white.lightness == doctest::Approx(1.0).epsilon(1e-4));
	CHECK(std::abs(white.a) < 1e-4);
	CHECK(std::abs(white.b) < 1e-4);
	const OkLab red = toOkLab(Rgb{1.0, 0.0, 0.0});
	CHECK(red.lightness == doctest::Approx(0.62796).epsilon(1e-3));
	CHECK(red.a == doctest::Approx(0.22486).epsilon(1e-3));
	CHECK(red.b == doctest::Approx(0.12585).epsilon(1e-3));
	const OkLch blue = toOkLch(Rgb{0.0, 0.0, 1.0});
	CHECK(blue.lightness == doctest::Approx(0.45201).epsilon(1e-3));
	CHECK(blue.chroma == doctest::Approx(0.31321).epsilon(1e-3));
	CHECK(blue.hue == doctest::Approx(264.052).epsilon(1e-3));
	CHECK(toOkLch(Rgb{0.4, 0.4, 0.4}).chroma == doctest::Approx(0.0));
}

TEST_CASE("gamut mapping keeps in-gamut colours and lowers the chroma of the rest") {
	CHECK(inGamut({0.0, 0.5, 1.0}));
	CHECK_FALSE(inGamut({1.2, 0.5, 0.0}));
	CHECK(near(clampRgb({-1.0, 2.0, std::numeric_limits<double>::quiet_NaN()}), Rgb{0.0, 1.0, 0.0}));
	const OkLch inside = toOkLch(Rgb{0.3, 0.6, 0.2});
	CHECK(near(gamutMapOkLch(inside), Rgb{0.3, 0.6, 0.2}, 1e-6));
	const OkLch vivid{0.7, 0.4, 150.0};
	CHECK_FALSE(inGamut(fromOkLch(vivid)));
	const Rgb mapped = gamutMapOkLch(vivid);
	CHECK(inGamut(mapped));
	const OkLch back = toOkLch(mapped);
	CHECK(back.lightness == doctest::Approx(0.7).epsilon(1e-3));
	CHECK(back.hue == doctest::Approx(150.0).epsilon(1e-2));
	CHECK(back.chroma < 0.4);
	CHECK(inGamut(gamutMapOkLch({1.5, 0.3, 30.0})));
	CHECK(inGamut(gamutMapOkLch({0.5, std::numeric_limits<double>::quiet_NaN(), 30.0})));
}
