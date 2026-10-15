#pragma once

namespace sgl {

enum class Unit {
	Pixel,
	Percent, // relative to the matching dimension of the parent panel
};

struct Length {
	float value = 0.0f;
	Unit unit = Unit::Pixel;

	// Converts to pixels; `reference` is the parent dimension a percentage refers to
	[[nodiscard]] constexpr float resolve(float reference) const noexcept {
		return unit == Unit::Pixel ? value : (reference / 100.0f) * value;
	}

	constexpr bool operator==(const Length&) const noexcept = default;
};

[[nodiscard]] constexpr Length px(float value) noexcept { return {value, Unit::Pixel}; }
[[nodiscard]] constexpr Length pct(float value) noexcept { return {value, Unit::Percent}; }

namespace literals {

// clang-format off
constexpr Length operator""_px(long double value) noexcept { return px(static_cast<float>(value)); }
constexpr Length operator""_px(unsigned long long value) noexcept { return px(static_cast<float>(value)); }
constexpr Length operator""_pct(long double value) noexcept { return pct(static_cast<float>(value)); }
constexpr Length operator""_pct(unsigned long long value) noexcept { return pct(static_cast<float>(value)); }
// clang-format on

} // namespace literals

} // namespace sgl
