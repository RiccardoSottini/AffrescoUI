#pragma once

/// @file
/// @brief Pixels of the imaging module: the formats a tile holds, a pixel as floats, and half-float conversion.

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace affresco::imaging {

/// @brief Pixels per side of a tile: every image is a sparse grid of tiles of this size.
inline constexpr int tileSize = 256;

/// @brief Pixels in a tile.
inline constexpr std::size_t tilePixels = std::size_t{tileSize} * std::size_t{tileSize};

/// @brief How a tile stores its pixels: straight (not premultiplied) alpha, top row first.
enum class PixelFormat : std::uint8_t {
	rgba8,   ///< Red, green, blue and alpha, 8 bits each.
	rgba16f, ///< Red, green, blue and alpha, half floats (IEEE 754 binary16) each.
	mask8,   ///< One channel of 8 bits: masks and selections (read as Pixel::alpha).
};

/// @brief Bytes per pixel of a format.
/// @param format The format.
/// @return 4, 8 or 1.
[[nodiscard]] constexpr std::size_t bytesPerPixel(PixelFormat format) noexcept {
	switch(format) {
	case PixelFormat::rgba8:
		return 4;
	case PixelFormat::rgba16f:
		return 8;
	case PixelFormat::mask8:
		return 1;
	}
	return 4;
}

/// @brief A pixel as floats, straight alpha: 0 to 1, or beyond in rgba16f.
struct Pixel {
	float red = 0.0f;   ///< Red.
	float green = 0.0f; ///< Green.
	float blue = 0.0f;  ///< Blue.
	float alpha = 0.0f; ///< Opacity, or a mask's coverage.

	/// @brief Whether two pixels are the same.
	[[nodiscard]] friend constexpr bool operator==(const Pixel&, const Pixel&) noexcept = default;
};

/// @brief Converts a float to a half float, rounding to nearest even; beyond the range becomes infinity.
/// @param value The float.
/// @return Its bits as binary16.
[[nodiscard]] constexpr std::uint16_t halfFromFloat(float value) noexcept {
	const auto bits = std::bit_cast<std::uint32_t>(value);
	const auto sign = static_cast<std::uint16_t>((bits >> 16) & 0x8000u);
	const std::uint32_t exponent = (bits >> 23) & 0xffu;
	std::uint32_t mantissa = bits & 0x7fffffu;
	if(exponent == 0xffu) // infinity or NaN
		return static_cast<std::uint16_t>(sign | 0x7c00u | (mantissa != 0 ? 0x200u : 0u));
	const int halfExponent = static_cast<int>(exponent) - 127 + 15;
	if(halfExponent >= 31) return static_cast<std::uint16_t>(sign | 0x7c00u);
	if(halfExponent <= 0) { // subnormal or zero
		if(halfExponent < -10) return sign;
		mantissa |= 0x800000u;
		const auto shift = static_cast<std::uint32_t>(14 - halfExponent);
		std::uint32_t half = mantissa >> shift;
		const std::uint32_t rest = mantissa & ((1u << shift) - 1u), middle = 1u << (shift - 1u);
		if(rest > middle || (rest == middle && (half & 1u) != 0)) ++half;
		return static_cast<std::uint16_t>(sign | half);
	}
	std::uint32_t half = (static_cast<std::uint32_t>(halfExponent) << 10) | (mantissa >> 13);
	const std::uint32_t rest = mantissa & 0x1fffu;
	if(rest > 0x1000u || (rest == 0x1000u && (half & 1u) != 0)) ++half; // may carry into the exponent: right
	return static_cast<std::uint16_t>(sign | half);
}

/// @brief Converts a half float to a float, exactly.
/// @param half Its bits as binary16.
/// @return The float.
[[nodiscard]] constexpr float floatFromHalf(std::uint16_t half) noexcept {
	const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16;
	std::uint32_t exponent = (half >> 10) & 0x1fu;
	std::uint32_t mantissa = half & 0x3ffu;
	if(exponent == 0x1fu) return std::bit_cast<float>(sign | 0x7f800000u | (mantissa << 13));
	if(exponent == 0) {
		if(mantissa == 0) return std::bit_cast<float>(sign);
		exponent = 1;
		while((mantissa & 0x400u) == 0) {
			mantissa <<= 1;
			--exponent;
		}
		mantissa &= 0x3ffu;
		return std::bit_cast<float>(sign | ((exponent + 112u) << 23) | (mantissa << 13));
	}
	return std::bit_cast<float>(sign | ((exponent + 112u) << 23) | (mantissa << 13));
}

/// @brief A float from 0 to 1 as a byte, rounded; beyond the range is clamped.
/// @param value The float.
/// @return 0 to 255.
[[nodiscard]] constexpr std::uint8_t byteFromUnit(float value) noexcept {
	if(!(value > 0.0f)) return 0; // also NaN
	if(value >= 1.0f) return 255;
	return static_cast<std::uint8_t>(value * 255.0f + 0.5f);
}

/// @brief A byte as a float from 0 to 1.
/// @param value The byte.
/// @return value / 255.
[[nodiscard]] constexpr float unitFromByte(std::uint8_t value) noexcept { return static_cast<float>(value) / 255.0f; }

/// @brief A pixel with each channel clamped to 0 to 1 (NaN becomes 0).
/// @param pixel The pixel.
/// @return It, clamped.
[[nodiscard]] constexpr Pixel clamped(const Pixel& pixel) noexcept {
	const auto unit = [](float value) { return value > 0.0f ? std::min(value, 1.0f) : 0.0f; };
	return {unit(pixel.red), unit(pixel.green), unit(pixel.blue), unit(pixel.alpha)};
}

} // namespace affresco::imaging
