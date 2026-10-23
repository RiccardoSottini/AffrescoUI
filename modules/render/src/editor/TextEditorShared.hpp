#pragma once

/// @file
/// @brief What TextEditor.cpp (editing) and TextEditorInput.cpp (the keys and the pointer) share and keep out of the
/// public header: the word rules and the modifiers' meanings.

#include "affresco/render/Input.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace affresco::editor {

/// @brief Whether a code point is part of a word (letters, digits, underscore).
/// @param c The code point.
/// @return true if it is.
[[nodiscard]] inline bool isWord(char32_t c) noexcept {
	return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_' ||
	       (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7);
}

/// @brief Whether a code point is a space or a tab.
/// @param c The code point.
/// @return true if it is.
[[nodiscard]] inline bool isBlank(char32_t c) noexcept { return c == U' ' || c == U'\t'; }

/// @brief Whether the command modifier is held: Command on macOS, Control elsewhere (either is taken).
/// @param mods The modifiers.
/// @return true if it is.
[[nodiscard]] inline bool commandHeld(Mods mods) noexcept { return mods.super() || mods.control(); }

/// @brief Whether the modifiers move by words: Option, or Control without Command.
/// @param mods The modifiers.
/// @return true if they do.
[[nodiscard]] inline bool wordHeld(Mods mods) noexcept { return mods.alt() || (mods.control() && !mods.super()); }

/// @brief The word around a column: the word characters before and after it.
/// @param text The line.
/// @param column The column, at most the line's length.
/// @return The word's first column and the column after it; equal if there is none.
[[nodiscard]] inline std::pair<std::size_t, std::size_t> wordAround(const std::u32string& text,
                                                                    std::size_t column) noexcept {
	std::size_t start = column, end = column;
	while(start > 0 && isWord(text[start - 1])) --start;
	while(end < text.size() && isWord(text[end])) ++end;
	return {start, end};
}

} // namespace affresco::editor
