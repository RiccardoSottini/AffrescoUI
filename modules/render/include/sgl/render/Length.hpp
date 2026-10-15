#pragma once

/// @file
/// @brief Length: a size or offset in pixels or percent of the parent, with `_px` and `_pct` literals.

namespace sgl {

/// @brief The unit of a Length.
enum class Unit {
	Pixel,   ///< Window units.
	Percent, ///< Relative to the matching dimension of the parent panel.
};

/// @brief A size or offset in pixels or percent of the parent, e.g. `50_pct` or `40_px`.
struct Length {
	float value = 0.0f;      ///< The amount.
	Unit unit = Unit::Pixel; ///< Its unit.

	/// @brief Converts to pixels.
	/// @param reference The parent dimension a percentage refers to.
	/// @return The length in pixels.
	[[nodiscard]] constexpr float resolve(float reference) const noexcept {
		return unit == Unit::Pixel ? value : (reference / 100.0f) * value;
	}

	constexpr bool operator==(const Length&) const noexcept = default;
};

/// @brief Makes a length in pixels.
/// @param value Pixels.
/// @return The length.
[[nodiscard]] constexpr Length px(float value) noexcept { return {value, Unit::Pixel}; }

/// @brief Makes a length in percent of the parent.
/// @param value Percent (100 is the parent's size).
/// @return The length.
[[nodiscard]] constexpr Length pct(float value) noexcept { return {value, Unit::Percent}; }

/// @brief User-defined literals for lengths.
namespace literals {

// clang-format off
/// @brief A length in pixels, e.g. `12.5_px`.
constexpr Length operator""_px(long double value) noexcept { return px(static_cast<float>(value)); }
/// @brief A length in pixels, e.g. `10_px`.
constexpr Length operator""_px(unsigned long long value) noexcept { return px(static_cast<float>(value)); }
/// @brief A length in percent of the parent, e.g. `33.3_pct`.
constexpr Length operator""_pct(long double value) noexcept { return pct(static_cast<float>(value)); }
/// @brief A length in percent of the parent, e.g. `50_pct`.
constexpr Length operator""_pct(unsigned long long value) noexcept { return pct(static_cast<float>(value)); }
// clang-format on

} // namespace literals

} // namespace sgl
