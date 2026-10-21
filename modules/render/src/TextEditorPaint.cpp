/// @file
/// @brief TextEditor drawing: the visible lines in runs of one colour, the selection, the gutter, the caret and the
/// scroll bar.

#include "affresco/render/Painter.hpp"
#include "affresco/render/TextEditor.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace affresco {

void TextEditor::colorLine(std::u32string_view, int&, std::vector<Color>& colors) const {
	std::ranges::fill(colors, textColor_);
}

void TextEditor::paint(Painter& painter) const {
	Panel::paint(painter);
	const Font* f = font();
	if(!f) return;

	const Rect& box = frame();
	const float lh = lineHeight();
	const auto first = static_cast<std::size_t>(std::max(std::floor(scroll_.y / lh) - 1.0f, 0.0f));
	const Layout layout{.lineHeight = lh,
	                    .gutter = gutterWidth(),
	                    .textLeft = std::round(box.left + gutterWidth() - scroll_.x),
	                    .firstTop = box.top + textPadding - scroll_.y,
	                    .baseline = std::round((lh - f->lineHeight()) / 2.0f + f->ascent()),
	                    .first = first,
	                    .last = std::min(lines_.size(), first + visibleLines() + 3)};

	// The colouring state (e.g. an open block comment) may come from above the visible lines: each line's is kept, so
	// they are coloured from the last one known
	if(states_.empty()) states_.push_back(0);
	std::size_t line = std::min(states_.size() - 1, layout.first);
	int state = states_[line];
	std::vector<Color> colors;
	std::vector<std::pair<std::size_t, float>> runs;
	for(; line < layout.last; ++line) {
		const std::u32string& text = lines_[line];
		colors.resize(text.size());
		colorLine(text, state, colors);
		if(line + 1 == states_.size()) states_.push_back(state);
		if(line >= layout.first) paintLine(painter, *f, layout, line, colors, runs);
	}

	paintGutter(painter, *f, layout);
	paintCaretAndScrollBar(painter, layout);
}

float TextEditor::lineTop(const Layout& layout, std::size_t line) noexcept {
	return std::round(layout.firstTop + static_cast<float>(line) * layout.lineHeight);
}

void TextEditor::paintLine(Painter& painter, const Font& font, const Layout& layout, std::size_t line,
                           const std::vector<Color>& colors, std::vector<std::pair<std::size_t, float>>& runs) const {
	const Rect& box = frame();
	const float top = lineTop(layout, line), lh = layout.lineHeight;
	const std::u32string& text = lines_[line];

	if(line == caret_.line && !hasSelection()) painter.fillRect({box.left, top, box.right, top + lh}, lineColor_);
	paintSelection(painter, font, layout, line);

	// Runs of one colour, each shaped on its own, where the whole line's shaping puts them (so the caret matches).
	// Their widths are read first: drawing a run shapes it, which may drop the line's shaping from the cache
	const std::vector<float>& advances = font.shape(text).advances;
	if(advances.size() < text.size()) return; // out of memory: Font::shape() gave nothing
	runs.clear();
	for(std::size_t start = 0; start < text.size();) {
		std::size_t end = start + 1;
		while(end < text.size() && colors[end] == colors[start]) ++end;
		float width = 0.0f;
		for(std::size_t i = start; i < end; ++i) width += advances[i];
		runs.emplace_back(end, width);
		start = end;
	}
	float pen = layout.textLeft;
	std::size_t start = 0;
	for(const auto& [end, width] : runs) {
		if(pen + width >= box.left + layout.gutter && pen <= box.right)
			painter.drawText(font, std::u32string_view(text).substr(start, end - start), {pen, top + layout.baseline},
			                 colors[start]);
		pen += width;
		start = end;
	}
}

void TextEditor::paintSelection(Painter& painter, const Font& font, const Layout& layout, std::size_t line) const {
	const auto [from, to] = selection();
	if(!hasSelection() || line < from.line || line > to.line) return;

	const float top = lineTop(layout, line);
	const float left = line == from.line ? columnX(line, from.column) : 0.0f;
	// A selected line break shows as a space's width
	const float right =
	    line == to.line ? columnX(line, to.column) : font.measure(lines_[line]) + font.glyph(U' ').advance;
	painter.fillRect({layout.textLeft + std::min(left, right), top, layout.textLeft + std::max(left, right),
	                  top + layout.lineHeight},
	                 selectionColor_);
}

void TextEditor::paintGutter(Painter& painter, const Font& font, const Layout& layout) const {
	// The gutter covers text scrolled under it
	const Rect& box = frame();
	if(color().a > 0.0f) painter.fillRect({box.left, box.top, box.left + layout.gutter - 8.0f, box.bottom}, color());
	if(!lineNumbers_) return;
	for(std::size_t line = layout.first; line < layout.last; ++line) {
		const std::u32string number = detail::fromUtf8(std::to_string(line + 1));
		const float x = std::round(box.left + layout.gutter - 20.0f - font.measure(number));
		painter.drawText(font, number, {x, lineTop(layout, line) + layout.baseline},
		                 line == caret_.line ? activeLineNumberColor_ : lineNumberColor_);
	}
}

void TextEditor::paintCaretAndScrollBar(Painter& painter, const Layout& layout) const {
	const Rect& box = frame();
	const float lh = layout.lineHeight;
	if(hasFocus()) {
		const float x = std::round(layout.textLeft + columnX(caret_.line, caret_.column));
		const float top = lineTop(layout, caret_.line);
		if(x >= box.left + layout.gutter - 8.0f)
			painter.fillRect({x, top + 1.0f, x + 2.0f, top + lh - 1.0f}, caretColor_);
	}

	// A scroll bar thumb as long as the visible share of the text
	const float content = static_cast<float>(lines_.size()) * lh + box.height() - lh;
	if(static_cast<float>(lines_.size()) * lh + textPadding > box.height()) {
		const float length = std::max(box.height() * box.height() / content, 16.0f);
		const float top = box.top + scroll_.y / (content - box.height()) * (box.height() - length);
		painter.fillRect({box.right - scrollBarWidth - 2.0f, top, box.right - 2.0f, top + length},
		                 lineNumberColor_.withAlpha(0.9f));
	}
}

} // namespace affresco
