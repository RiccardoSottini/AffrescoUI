#pragma once

/// @file
/// @brief What a TextEditor shows besides its text: decorations, folded ranges and extra selections (multi-cursor).

#include "affresco/render/Color.hpp"
#include "affresco/render/TextModel.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace affresco {

/// @brief A mark on some lines of an editor: a background, a squiggle under the text, an icon in the gutter or text
/// after the line's end (e.g. a diagnostic, a changed line, a breakpoint, a blame note).
struct EditorDecoration {
	/// @brief What it draws.
	enum class Kind : std::uint8_t {
		Background, ///< Fills the lines (or the columns given, on one line) behind the text.
		Squiggle,   ///< A wavy line under the text.
		Gutter,     ///< An icon (or a dot) in the gutter, on the first line.
		Inline,     ///< Text after the end of the first line, in the colour given.
	};

	std::size_t line = 0;                 ///< The first line, from 0.
	std::size_t endLine = 0;              ///< The last line (not before line).
	std::size_t column = 0;               ///< Where it starts on the first line (Background and Squiggle).
	std::size_t endColumn = SIZE_MAX;     ///< Where it ends on the last line; SIZE_MAX: the line's end.
	Kind kind = Kind::Background;         ///< What it draws.
	Color color;                          ///< Its colour.
	char32_t icon = 0;                    ///< Gutter: the glyph drawn (0: a dot).
	std::u32string text;                  ///< Inline: the text drawn.

	/// @brief Whether two decorations are the same.
	bool operator==(const EditorDecoration&) const = default;
};

/// @brief Lines hidden under the line before them: line stays shown (with a marker), line + 1 to endLine are not.
struct FoldRange {
	std::size_t line = 0;    ///< The line that stays shown.
	std::size_t endLine = 0; ///< The last line hidden (after line).

	/// @brief Whether two ranges are the same.
	bool operator==(const FoldRange&) const = default;
};

/// @brief A selection: where it was started and where its caret is (the same place: just a caret).
struct EditorSelection {
	TextPosition anchor; ///< The fixed end.
	TextPosition caret;  ///< The moving end.

	/// @brief Whether two selections are the same.
	bool operator==(const EditorSelection&) const = default;
};

} // namespace affresco
