#pragma once

/// @file
/// @brief detail::FontInspector: what tests see of a font's insides, beyond its public API.

#include "affresco/render/Font.hpp"

#include <cstddef>
#include <string_view>

namespace affresco::detail {

/// @brief Reaches a font's fast path for plain text (text/PlainText.hpp), so tests can compare it with shaping. Not
/// for applications: the header is private to the render module and its tests, and needs neither FreeType nor
/// HarfBuzz. Defined in Font.cpp.
struct FontInspector {
	/// @brief Turns a font's fast path off, so that every word is shaped by HarfBuzz, or on again.
	/// @param font The font.
	/// @param enabled Whether plain words skip shaping.
	static void setPlainTextEnabled(const Font& font, bool enabled);
	/// @brief How many characters of a font's first face are plain.
	/// @param font The font.
	[[nodiscard]] static std::size_t plainCharacters(const Font& font);
	/// @brief Whether a font lays a word out without shaping it.
	/// @param font The font.
	/// @param word The word: no spaces.
	[[nodiscard]] static bool isPlain(const Font& font, std::u32string_view word);
};

} // namespace affresco::detail
