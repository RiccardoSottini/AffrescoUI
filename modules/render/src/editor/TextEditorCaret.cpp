/// @file
/// @brief TextEditor's caret styles: a thin bar before the character, a box over it, or a bar under it.

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace affresco {

void TextEditor::setCaretStyle(CaretStyle style) {
	if(style == caretStyle_) return;
	caretStyle_ = style;
	invalidate();
}

void TextEditor::paintCaret(Painter& painter, const Font& font, const Layout& layout, Position at, float x,
                            float top) const {
	const float lh = layout.lineHeight;
	if(caretStyle_ == CaretStyle::Line) {
		painter.fillRect({x, top + 1.0f, x + 2.0f, top + lh - 1.0f}, caretColor_);
		return;
	}
	// As wide as the character after it: a space at the line's end, and a tab or a control character as one
	const std::u32string& line = lines_[at.line];
	const char32_t under = at.column < line.size() ? line[at.column] : U' ';
	const bool shown = under > U' ' && under != U'\x7f';
	const float width = std::max(std::round(font.glyph(shown ? under : U' ').advance), 2.0f);
	if(caretStyle_ == CaretStyle::Underline) {
		painter.fillRect({x, top + lh - 3.0f, x + width, top + lh - 1.0f}, caretColor_);
		return;
	}
	painter.fillRect({x, top, x + width, top + lh}, caretColor_);
	// The character shows through in the background's colour, or the text's under a see-through background
	const Color through = color().a > 0.5f ? color().withAlpha(1.0f) : textColor_;
	if(shown) painter.drawText(font, std::u32string(1, under), {x, top + layout.baseline}, through);
}

} // namespace affresco
