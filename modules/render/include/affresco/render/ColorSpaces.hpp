#pragma once

/// @file
/// @brief Pure colour-space maths for the creative controls: sRGB and linear light, HSV, HSL, OKLab and OKLCH, with
/// gamut checks and clamping. Every component is a double; nothing here allocates or keeps state.

namespace affresco::color {

/// @brief A colour as red, green and blue, each 0..1 (sRGB-encoded or linear, as the function says).
struct Rgb {
	double red = 0.0;   ///< Red, 0..1.
	double green = 0.0; ///< Green, 0..1.
	double blue = 0.0;  ///< Blue, 0..1.
};

/// @brief Hue, saturation and value.
struct Hsv {
	double hue = 0.0;        ///< Hue in degrees, 0..360.
	double saturation = 0.0; ///< Saturation, 0..1.
	double value = 0.0;      ///< Value (brightness), 0..1.
};

/// @brief Hue, saturation and lightness.
struct Hsl {
	double hue = 0.0;        ///< Hue in degrees, 0..360.
	double saturation = 0.0; ///< Saturation, 0..1.
	double lightness = 0.0;  ///< Lightness, 0..1.
};

/// @brief OKLab: perceptual lightness and two opponent axes.
struct OkLab {
	double lightness = 0.0; ///< Lightness, 0..1.
	double a = 0.0;         ///< Green (negative) to red (positive), about -0.4..0.4.
	double b = 0.0;         ///< Blue (negative) to yellow (positive), about -0.4..0.4.
};

/// @brief OKLCH: OKLab in polar form.
struct OkLch {
	double lightness = 0.0; ///< Lightness, 0..1.
	double chroma = 0.0;    ///< Chroma, 0 and up (about 0.37 at most within sRGB).
	double hue = 0.0;       ///< Hue in degrees, 0..360.
};

/// @brief Decodes one sRGB component to linear light.
/// @param encoded The sRGB component, 0..1.
/// @return The linear component.
[[nodiscard]] double srgbToLinear(double encoded) noexcept;
/// @brief Encodes one linear-light component as sRGB.
/// @param linear The linear component, 0..1.
/// @return The sRGB component.
[[nodiscard]] double linearToSrgb(double linear) noexcept;
/// @brief Decodes an sRGB colour to linear light.
/// @param srgb The sRGB colour.
/// @return The linear colour.
[[nodiscard]] Rgb srgbToLinear(Rgb srgb) noexcept;
/// @brief Encodes a linear-light colour as sRGB.
/// @param linear The linear colour.
/// @return The sRGB colour.
[[nodiscard]] Rgb linearToSrgb(Rgb linear) noexcept;

/// @brief Converts sRGB to HSV.
/// @param srgb The colour.
/// @return Its HSV form (hue 0 for greys).
[[nodiscard]] Hsv toHsv(Rgb srgb) noexcept;
/// @brief Converts HSV to sRGB.
/// @param hsv The colour; the hue wraps, the rest is clamped.
/// @return Its sRGB form.
[[nodiscard]] Rgb fromHsv(Hsv hsv) noexcept;
/// @brief Converts sRGB to HSL.
/// @param srgb The colour.
/// @return Its HSL form (hue 0 for greys).
[[nodiscard]] Hsl toHsl(Rgb srgb) noexcept;
/// @brief Converts HSL to sRGB.
/// @param hsl The colour; the hue wraps, the rest is clamped.
/// @return Its sRGB form.
[[nodiscard]] Rgb fromHsl(Hsl hsl) noexcept;

/// @brief Converts sRGB to OKLab.
/// @param srgb The colour.
/// @return Its OKLab form.
[[nodiscard]] OkLab toOkLab(Rgb srgb) noexcept;
/// @brief Converts OKLab to sRGB, without clamping (components may leave 0..1 when out of gamut).
/// @param lab The colour.
/// @return Its sRGB form.
[[nodiscard]] Rgb fromOkLab(OkLab lab) noexcept;
/// @brief Converts OKLab to OKLCH.
/// @param lab The colour.
/// @return Its polar form (hue 0 when the chroma is about 0).
[[nodiscard]] OkLch toOkLch(OkLab lab) noexcept;
/// @brief Converts OKLCH to OKLab.
/// @param lch The colour.
/// @return Its rectangular form.
[[nodiscard]] OkLab toOkLab(OkLch lch) noexcept;
/// @brief Converts sRGB to OKLCH.
/// @param srgb The colour.
/// @return Its OKLCH form.
[[nodiscard]] OkLch toOkLch(Rgb srgb) noexcept;
/// @brief Converts OKLCH to sRGB, without clamping.
/// @param lch The colour.
/// @return Its sRGB form.
[[nodiscard]] Rgb fromOkLch(OkLch lch) noexcept;

/// @brief Whether a colour is within the sRGB gamut.
/// @param srgb The colour.
/// @param tolerance How far a component may leave 0..1 and still count.
/// @return True when every component is within 0..1 (give or take the tolerance).
[[nodiscard]] bool inGamut(Rgb srgb, double tolerance = 1e-6) noexcept;
/// @brief Clamps every component to 0..1 (not a number becomes 0).
/// @param srgb The colour.
/// @return The clamped colour.
[[nodiscard]] Rgb clampRgb(Rgb srgb) noexcept;
/// @brief Brings an OKLCH colour into the sRGB gamut by lowering its chroma, keeping its lightness and hue.
/// @param lch The colour; its lightness is clamped to 0..1 first.
/// @return An sRGB colour within the gamut, as close to the colour as the gamut allows.
[[nodiscard]] Rgb gamutMapOkLch(OkLch lch) noexcept;

} // namespace affresco::color
