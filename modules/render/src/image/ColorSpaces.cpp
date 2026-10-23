/// @file
/// @brief Colour-space conversions: sRGB transfer curve, HSV, HSL, OKLab (Björn Ottosson's matrices), OKLCH and
/// gamut mapping by chroma bisection.

#include "affresco/render/ColorSpaces.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <numbers>

namespace affresco::color {

namespace {

/// @brief Below this chroma or saturation the hue is meaningless and reported as 0.
constexpr double tinyChroma = 1e-9;

/// @brief Clamps to 0..1, mapping not a number to 0.
/// @param value The value.
/// @return The clamped value.
[[nodiscard]] double unit(double value) noexcept { return std::isnan(value) ? 0.0 : std::clamp(value, 0.0, 1.0); }

/// @brief Wraps a hue into 0..360.
/// @param hue Degrees, any value (not a number becomes 0).
/// @return The same angle within 0..360.
[[nodiscard]] double wrapHue(double hue) noexcept {
	if(!std::isfinite(hue)) return 0.0;
	const double wrapped = std::fmod(hue, 360.0);
	return wrapped < 0.0 ? wrapped + 360.0 : wrapped;
}

/// @brief The hue of an RGB colour, shared by HSV and HSL.
/// @param srgb The colour.
/// @param largest Its largest component.
/// @param spread Its largest minus its smallest component (above 0).
/// @return Degrees, 0..360.
[[nodiscard]] double hueOf(Rgb srgb, double largest, double spread) noexcept {
	double sector = 0.0;
	if(largest == srgb.red) {
		sector = (srgb.green - srgb.blue) / spread;
	} else if(largest == srgb.green) {
		sector = 2.0 + (srgb.blue - srgb.red) / spread;
	} else {
		sector = 4.0 + (srgb.red - srgb.green) / spread;
	}
	return wrapHue(sector * 60.0);
}

/// @brief The RGB of a pure hue scaled by chroma, plus a common offset (shared by HSV and HSL).
/// @param hue Degrees.
/// @param chroma Largest minus smallest component.
/// @param offset Added to every component.
/// @return The colour.
[[nodiscard]] Rgb fromHueChroma(double hue, double chroma, double offset) noexcept {
	const double sector = wrapHue(hue) / 60.0;
	const double middle = chroma * (1.0 - std::abs(std::fmod(sector, 2.0) - 1.0));
	Rgb out;
	switch(static_cast<int>(sector) % 6) {
	case 0:
		out = {chroma, middle, 0.0};
		break;
	case 1:
		out = {middle, chroma, 0.0};
		break;
	case 2:
		out = {0.0, chroma, middle};
		break;
	case 3:
		out = {0.0, middle, chroma};
		break;
	case 4:
		out = {middle, 0.0, chroma};
		break;
	default:
		out = {chroma, 0.0, middle};
		break;
	}
	return {out.red + offset, out.green + offset, out.blue + offset};
}

/// @brief Three components, for the OKLab matrices.
using Triple = std::array<double, 3>;
/// @brief A 3x3 matrix, by rows.
using Matrix = std::array<Triple, 3>;

/// @brief Multiplies a matrix by a column.
/// @param matrix The matrix.
/// @param column The column.
/// @return The product.
[[nodiscard]] constexpr Triple apply(const Matrix& matrix, const Triple& column) noexcept {
	Triple out{};
	for(std::size_t row = 0; row < 3; ++row) {
		out[row] = matrix[row][0] * column[0] + matrix[row][1] * column[1] + matrix[row][2] * column[2];
	}
	return out;
}

/// @brief The inverse of a 3x3 matrix (by cofactors), so each OKLab matrix and its inverse agree to the last bit.
/// @param m The matrix, which must be invertible.
/// @return Its inverse.
[[nodiscard]] constexpr Matrix inverse(const Matrix& m) noexcept {
	const Matrix cofactors{{
	    {m[1][1] * m[2][2] - m[1][2] * m[2][1], m[0][2] * m[2][1] - m[0][1] * m[2][2],
		 m[0][1] * m[1][2] - m[0][2] * m[1][1]},
	    {m[1][2] * m[2][0] - m[1][0] * m[2][2], m[0][0] * m[2][2] - m[0][2] * m[2][0],
		 m[0][2] * m[1][0] - m[0][0] * m[1][2]},
	    {m[1][0] * m[2][1] - m[1][1] * m[2][0], m[0][1] * m[2][0] - m[0][0] * m[2][1],
		 m[0][0] * m[1][1] - m[0][1] * m[1][0]},
	}};
	const double determinant = m[0][0] * cofactors[0][0] + m[0][1] * cofactors[1][0] + m[0][2] * cofactors[2][0];
	Matrix out{};
	for(std::size_t row = 0; row < 3; ++row) {
		for(std::size_t column = 0; column < 3; ++column) out[row][column] = cofactors[row][column] / determinant;
	}
	return out;
}

/// @brief Linear sRGB to the LMS cone response (Björn Ottosson's OKLab M1).
constexpr Matrix linearToCone{{
    {0.4122214708, 0.5363325363, 0.0514459929},
    {0.2119034982, 0.6806995451, 0.1073969566},
    {0.0883024619, 0.2817188376, 0.6299787005},
}};
/// @brief The cube-rooted cone response to OKLab (Björn Ottosson's OKLab M2).
constexpr Matrix coneToLab{{
    {0.2104542553, 0.7936177850, -0.0040720468},
    {1.9779984951, -2.4285922050, 0.4505937099},
    {0.0259040371, 0.7827717662, -0.8086757660},
}};
/// @brief OKLab back to the cube-rooted cone response.
constexpr Matrix labToCone = inverse(coneToLab);
/// @brief The cone response back to linear sRGB.
constexpr Matrix coneToLinear = inverse(linearToCone);

} // namespace

double srgbToLinear(double encoded) noexcept {
	const double magnitude = std::abs(encoded);
	const double linear = magnitude <= 0.04045 ? magnitude / 12.92 : std::pow((magnitude + 0.055) / 1.055, 2.4);
	return std::copysign(linear, encoded);
}

double linearToSrgb(double linear) noexcept {
	const double magnitude = std::abs(linear);
	const double encoded = magnitude <= 0.0031308 ? magnitude * 12.92 : 1.055 * std::pow(magnitude, 1.0 / 2.4) - 0.055;
	return std::copysign(encoded, linear);
}

Rgb srgbToLinear(Rgb srgb) noexcept {
	return {srgbToLinear(srgb.red), srgbToLinear(srgb.green), srgbToLinear(srgb.blue)};
}

Rgb linearToSrgb(Rgb linear) noexcept {
	return {linearToSrgb(linear.red), linearToSrgb(linear.green), linearToSrgb(linear.blue)};
}

Hsv toHsv(Rgb srgb) noexcept {
	srgb = clampRgb(srgb);
	const double largest = std::max({srgb.red, srgb.green, srgb.blue});
	const double spread = largest - std::min({srgb.red, srgb.green, srgb.blue});
	if(spread <= tinyChroma) return {0.0, 0.0, largest};
	return {hueOf(srgb, largest, spread), spread / largest, largest};
}

Rgb fromHsv(Hsv hsv) noexcept {
	const double value = unit(hsv.value);
	const double chroma = value * unit(hsv.saturation);
	return fromHueChroma(hsv.hue, chroma, value - chroma);
}

Hsl toHsl(Rgb srgb) noexcept {
	srgb = clampRgb(srgb);
	const double largest = std::max({srgb.red, srgb.green, srgb.blue});
	const double smallest = std::min({srgb.red, srgb.green, srgb.blue});
	const double spread = largest - smallest;
	const double lightness = (largest + smallest) / 2.0;
	if(spread <= tinyChroma) return {0.0, 0.0, lightness};
	const double saturation = spread / (1.0 - std::abs(2.0 * lightness - 1.0));
	return {hueOf(srgb, largest, spread), unit(saturation), lightness};
}

Rgb fromHsl(Hsl hsl) noexcept {
	const double lightness = unit(hsl.lightness);
	const double chroma = (1.0 - std::abs(2.0 * lightness - 1.0)) * unit(hsl.saturation);
	return fromHueChroma(hsl.hue, chroma, lightness - chroma / 2.0);
}

OkLab toOkLab(Rgb srgb) noexcept {
	const Rgb linear = srgbToLinear(srgb);
	const Triple cone = apply(linearToCone, {linear.red, linear.green, linear.blue});
	const Triple lab = apply(coneToLab, {std::cbrt(cone[0]), std::cbrt(cone[1]), std::cbrt(cone[2])});
	return {lab[0], lab[1], lab[2]};
}

Rgb fromOkLab(OkLab lab) noexcept {
	const Triple cone = apply(labToCone, {lab.lightness, lab.a, lab.b});
	const Triple linear =
	    apply(coneToLinear, {cone[0] * cone[0] * cone[0], cone[1] * cone[1] * cone[1], cone[2] * cone[2] * cone[2]});
	return linearToSrgb(Rgb{linear[0], linear[1], linear[2]});
}

OkLch toOkLch(OkLab lab) noexcept {
	const double chroma = std::hypot(lab.a, lab.b);
	if(chroma <= 1e-7) return {lab.lightness, 0.0, 0.0};
	return {lab.lightness, chroma, wrapHue(std::atan2(lab.b, lab.a) * 180.0 / std::numbers::pi)};
}

OkLab toOkLab(OkLch lch) noexcept {
	const double radians = wrapHue(lch.hue) * std::numbers::pi / 180.0;
	const double chroma = std::max(0.0, std::isnan(lch.chroma) ? 0.0 : lch.chroma);
	return {lch.lightness, chroma * std::cos(radians), chroma * std::sin(radians)};
}

OkLch toOkLch(Rgb srgb) noexcept { return toOkLch(toOkLab(srgb)); }

Rgb fromOkLch(OkLch lch) noexcept { return fromOkLab(toOkLab(lch)); }

bool inGamut(Rgb srgb, double tolerance) noexcept {
	const auto within = [tolerance](double component) {
		return component >= -tolerance && component <= 1.0 + tolerance;
	};
	return within(srgb.red) && within(srgb.green) && within(srgb.blue);
}

Rgb clampRgb(Rgb srgb) noexcept { return {unit(srgb.red), unit(srgb.green), unit(srgb.blue)}; }

Rgb gamutMapOkLch(OkLch lch) noexcept {
	lch.lightness = unit(lch.lightness);
	const Rgb direct = fromOkLch(lch);
	if(inGamut(direct)) return clampRgb(direct);
	double low = 0.0;
	double high = std::isnan(lch.chroma) ? 0.0 : std::max(0.0, lch.chroma);
	for(int step = 0; step < 32; ++step) {
		const double middle = (low + high) / 2.0;
		if(inGamut(fromOkLch({lch.lightness, middle, lch.hue}))) {
			low = middle;
		} else {
			high = middle;
		}
	}
	return clampRgb(fromOkLch({lch.lightness, low, lch.hue}));
}

} // namespace affresco::color
