#pragma once

/// @file
/// @brief format::readTokens(): the "tokens" section, design values (colours, spacing, radii, type sizes) named once
/// and used as "@name" everywhere, switched by the themes.

#include "NodeKeys.hpp"

#include "affresco/render/Color.hpp"

#include <map>
#include <string>
#include <string_view>

namespace affresco::format {

/// @brief A value with variants per theme.
template <typename Value> struct Themed {
	Value base;                                       ///< The first value: used without a matching theme.
	std::map<std::string, Value, std::less<>> themes; ///< "light" -> value, "dark" -> value.

	/// @brief The value in a theme.
	/// @param theme The theme.
	/// @return Its variant, or the base value.
	[[nodiscard]] const Value& in(std::string_view theme) const {
		const auto variant = themes.find(theme);
		return variant != themes.end() ? variant->second : base;
	}
};

/// @brief Named colours, by name.
using ColorTable = std::map<std::string, Themed<Color>, std::less<>>;
/// @brief Named numbers (spacing, radii, type sizes), by name.
using NumberTable = std::map<std::string, Themed<float>, std::less<>>;

/// @brief Reads "tokens": each one a number, a colour ("#rrggbb", "transparent"), an alias ("@other", of a token or of
/// "colors") or a per-theme object ({"light": ..., "dark": ...}) of those. Colour tokens join `colors`, so "@name"
/// finds both. The installed library's tokens (components/Library.hpp) come under them: those the document doesn't name itself.
/// @param document The document.
/// @param colors The colours of "colors": the colour tokens are added to them.
/// @return The number tokens.
/// @throws DocumentError If a token is malformed, mixes numbers and colours, has a name `colors` has too, or its
/// aliases go deeper than 16 (or around in a circle).
NumberTable readTokens(const json& document, ColorTable& colors);

} // namespace affresco::format
