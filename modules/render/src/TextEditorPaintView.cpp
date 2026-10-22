/// @file
/// @brief TextEditor drawing under and over the text: background decorations, find matches and brackets; squiggles,
/// inline text, indentation guides, folded lines' ellipses, the extra carets and the IME composition.

#include "affresco/render/Painter.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

namespace {

/// @brief The columns of a decoration on one row.
/// @param d The decoration.
/// @param line The row's line.
/// @param start The row's first column.
/// @param end One past its last.
std::pair<std::size_t, std::size_t> decorationColumns(const EditorDecoration& d, std::size_t line, std::size_t start,
                                                      std::size_t end) noexcept {
	const std::size_t from = line == d.line ? std::max(d.column, start) : start;
	const std::size_t to = line == d.endLine ? std::min(d.endColumn, end) : end;
	return {from, to};
}

} // namespace

void TextEditor::forRowsOf(const Layout& layout, std::size_t from, std::size_t to,
                           const std::function<void(std::size_t, RowSpan)>& body) const {
	for(std::size_t row = std::max(rowOf({from, 0}), layout.first); row < layout.last; ++row) {
		const RowSpan span = rowSpan(row);
		if(span.line > to) return;
		if(span.line >= from) body(row, span);
	}
}

void TextEditor::paintUnder(Painter& painter, const Font& /*font*/, const Layout& layout) const {
	const Rect& box = frame();
	const std::size_t firstLine = rowSpan(layout.first).line, lastLine = rowSpan(layout.last - 1).line;
	const auto visible = std::ranges::lower_bound(decorations_, firstLine - std::min(firstLine, decorationSpan_), {},
	                                              &EditorDecoration::line);
	for(auto it = visible; it != decorations_.end() && it->line <= lastLine; ++it) {
		const EditorDecoration& d = *it;
		if(d.kind != EditorDecoration::Kind::Background || d.endLine < firstLine) continue;
		const bool wholeLines = d.column == 0 && d.endColumn == SIZE_MAX;
		forRowsOf(layout, d.line, d.endLine, [&](std::size_t row, RowSpan span) {
			const float top = lineTop(layout, row);
			if(wholeLines)
				return painter.fillRect({box.left + layout.gutter - 8.0f, top, box.right, top + layout.lineHeight},
				                        d.color);
			const auto [from, to] = decorationColumns(d, span.line, span.start, span.end);
			if(from < to) paintColumns(painter, layout, span, row, {from, to}, d.color);
		});
	}

	findVisibleMatches(firstLine, lastLine + 1);
	for(const VisibleMatch& m : visibleMatches_)
		forRowsOf(layout, m.line, m.line, [&](std::size_t row, RowSpan span) {
			const std::size_t from = std::max(m.column, span.start), to = std::min(m.column + m.length, span.end);
			if(from < to) paintColumns(painter, layout, span, row, {from, to}, matchColor_);
		});

	if(const auto brackets = matchingBrackets())
		for(const Position p : {brackets->first, brackets->second})
			forRowsOf(layout, p.line, p.line, [&](std::size_t row, RowSpan span) {
				if(p.column >= span.start && p.column < span.end)
					paintColumns(painter, layout, span, row, {p.column, p.column + 1}, bracketColor_);
			});
}

void TextEditor::paintAbove(Painter& painter, const Font& font, const Layout& layout) const {
	const float lh = layout.lineHeight;
	const float space = font.glyph(U' ').advance;
	// x of a column on its row, relative to the text's start
	const auto rowX = [this](RowSpan span, std::size_t column) {
		return columnX(span.line, column) - (wrap_ ? columnX(span.line, span.start) : 0.0f);
	};
	for(std::size_t row = layout.first; row < layout.last; ++row) {
		const RowSpan span = rowSpan(row);
		const float top = lineTop(layout, row);
		// Indentation guides, on a line's first row, at each level but the deepest
		if(guides_ && span.start == 0)
			for(std::size_t column = 0; column < indentOf(span.line) && column < span.end; column += tabSize) {
				if(column == 0) continue;
				const float x = std::round(layout.textLeft + static_cast<float>(column) * space);
				painter.fillRect({x, top, x + 1.0f, top + lh}, guideColor_);
			}
		// A folded line ends in three dots
		if(viewActive() && span.end == lines_[span.line].size() && nextShown(span.line) > span.line + 1) {
			const float x = layout.textLeft + rowX(span, span.end) + space;
			for(int dot = 0; dot < 3; ++dot)
				painter.fillRect({x + static_cast<float>(dot) * 4.0f, top + lh - 8.0f, x + static_cast<float>(dot) * 4.0f + 2.0f,
				                  top + lh - 6.0f},
				                 lineNumberColor_);
		}
	}

	const std::size_t firstLine = rowSpan(layout.first).line, lastLine = rowSpan(layout.last - 1).line;
	const auto visible = std::ranges::lower_bound(decorations_, firstLine - std::min(firstLine, decorationSpan_), {},
	                                              &EditorDecoration::line);
	for(auto it = visible; it != decorations_.end() && it->line <= lastLine; ++it) {
		const EditorDecoration& d = *it;
		if(d.kind == EditorDecoration::Kind::Squiggle)
			forRowsOf(layout, d.line, d.endLine, [&](std::size_t row, RowSpan span) {
				const auto [from, to] = decorationColumns(d, span.line, span.start, span.end);
				const float left = layout.textLeft + rowX(span, from), right = layout.textLeft + rowX(span, to);
				const float bottom = lineTop(layout, row) + lh - 3.0f;
				// A zigzag of short steps, two pixels each
				for(float x = left, up = 0.0f; x < right; x += 2.0f, up = 1.0f - up)
					painter.fillRect({x, bottom - up, std::min(x + 2.0f, right), bottom - up + 1.0f}, d.color);
			});
		else if(d.kind == EditorDecoration::Kind::Inline && !d.text.empty())
			forRowsOf(layout, d.line, d.line, [&](std::size_t row, RowSpan span) {
				if(span.end != lines_[span.line].size()) return; // after the line's last row
				const float x = layout.textLeft + rowX(span, span.end) + 2.0f * space;
				painter.drawText(font, d.text, {std::round(x), lineTop(layout, row) + layout.baseline}, d.color);
			});
	}

	if(hasFocus())
		for(const EditorSelection& s : cursors_) {
			const std::size_t row = rowOf(s.caret);
			if(row < layout.first || row >= layout.last || hidden(s.caret.line)) continue;
			const float x = std::round(layout.textLeft + xOnRow(s.caret)), top = lineTop(layout, row);
			painter.fillRect({x, top + 1.0f, x + 2.0f, top + lh - 1.0f}, caretColor_);
		}

	if(composition_.empty()) return;
	const std::size_t row = rowOf(caret_);
	if(row < layout.first || row >= layout.last) return;
	const float x = std::round(layout.textLeft + xOnRow(caret_)), top = lineTop(layout, row);
	const float width = font.measure(composition_);
	painter.fillRect({x, top, x + width, top + lh}, color());
	painter.drawText(font, composition_, {x, top + layout.baseline}, textColor_);
	painter.fillRect({x, top + lh - 3.0f, x + width, top + lh - 2.0f}, textColor_);
}

} // namespace affresco
