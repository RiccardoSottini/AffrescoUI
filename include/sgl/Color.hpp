#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sgl {

// RGBA color, each channel in [0, 1]
struct Color {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;

	// From a 0xRRGGBB literal, e.g. Color::hex(0x6767ff)
	[[nodiscard]] static constexpr Color hex(std::uint32_t rgb) noexcept {
		return {channel(rgb >> 16), channel(rgb >> 8), channel(rgb), 1.0f};
	}

	// Parses "#rgb", "#rrggbb" or "#rrggbbaa" (the '#' is optional); std::nullopt if malformed
	[[nodiscard]] static std::optional<Color> parseHex(std::string_view text);

	// "#rrggbb", or "#rrggbbaa" when not fully opaque
	[[nodiscard]] std::string toHex() const;

	// 8-bit RGBA, rounded to the nearest value (what an 8-bit framebuffer stores)
	[[nodiscard]] std::array<std::uint8_t, 4> toBytes() const noexcept;

	constexpr bool operator==(const Color&) const noexcept = default;

private:
	static constexpr float channel(std::uint32_t value) noexcept { return static_cast<float>(value & 0xffu) / 255.0f; }
};

namespace colors {

inline constexpr Color black{0.0f, 0.0f, 0.0f, 1.0f};
inline constexpr Color white{1.0f, 1.0f, 1.0f, 1.0f};
inline constexpr Color red{1.0f, 0.0f, 0.0f, 1.0f};
inline constexpr Color green{0.0f, 1.0f, 0.0f, 1.0f};
inline constexpr Color blue{0.0f, 0.0f, 1.0f, 1.0f};
inline constexpr Color transparent{0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr Color panel{0.941176f, 0.941176f, 0.941176f, 1.0f}; // default panel background

} // namespace colors

} // namespace sgl
