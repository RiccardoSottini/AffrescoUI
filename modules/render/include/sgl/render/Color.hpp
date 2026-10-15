#pragma once

/// @file
/// @brief Color: an RGBA colour with hex parsing and formatting, and named colours.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sgl {

/// @brief An RGBA colour, each channel in [0, 1].
struct Color {
	float r = 0.0f; ///< Red.
	float g = 0.0f; ///< Green.
	float b = 0.0f; ///< Blue.
	float a = 1.0f; ///< Alpha: 0 is fully transparent.

	/// @brief Makes an opaque colour from a hex literal.
	/// @param rgb 0xRRGGBB, e.g. `Color::hex(0x6767ff)`.
	/// @return The colour.
	[[nodiscard]] static constexpr Color hex(std::uint32_t rgb) noexcept {
		return {channel(rgb >> 16), channel(rgb >> 8), channel(rgb), 1.0f};
	}

	/// @brief Parses a hex colour.
	/// @param text "#rgb", "#rrggbb" or "#rrggbbaa"; the '#' is optional.
	/// @return The colour, or std::nullopt if malformed.
	[[nodiscard]] static std::optional<Color> parseHex(std::string_view text);

	/// @brief Formats the colour.
	/// @return "#rrggbb", or "#rrggbbaa" when not fully opaque.
	[[nodiscard]] std::string toHex() const;

	/// @brief Converts to 8-bit channels, rounded (what an 8-bit framebuffer stores).
	/// @return {r, g, b, a}, each clamped to [0, 255].
	[[nodiscard]] std::array<std::uint8_t, 4> toBytes() const noexcept;

	constexpr bool operator==(const Color&) const noexcept = default;

private:
	/// @brief Converts the low byte of a value to a channel.
	/// @param value Holds the channel in its low 8 bits.
	/// @return The channel, in [0, 1].
	static constexpr float channel(std::uint32_t value) noexcept { return static_cast<float>(value & 0xffu) / 255.0f; }
};

/// @brief Named colours.
namespace colors {

inline constexpr Color black{0.0f, 0.0f, 0.0f, 1.0f};               ///< #000000.
inline constexpr Color white{1.0f, 1.0f, 1.0f, 1.0f};               ///< #ffffff.
inline constexpr Color red{1.0f, 0.0f, 0.0f, 1.0f};                 ///< #ff0000.
inline constexpr Color green{0.0f, 1.0f, 0.0f, 1.0f};               ///< #00ff00.
inline constexpr Color blue{0.0f, 0.0f, 1.0f, 1.0f};                ///< #0000ff.
inline constexpr Color transparent{0.0f, 0.0f, 0.0f, 0.0f};         ///< Draws nothing.
inline constexpr Color panel{0.941176f, 0.941176f, 0.941176f, 1.0f}; ///< Default panel background.

} // namespace colors

} // namespace sgl
