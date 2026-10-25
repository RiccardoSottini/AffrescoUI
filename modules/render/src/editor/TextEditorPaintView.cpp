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
/// @return Its first column on the row and the column after its last; empty if it is not on the row.
std::pair<std::size_t, std::size_t> decorationColumns(const EditorDecoration& d, std::size_t line, std::size_t start,
                                                      std::size_t end) noexcept {
	const std::size_t from = line == d.line ? std::max(d.column, start) : start;
	const std::size_t to = line == d.endLine ? std::min(d.endColumn, end) : end;
	return {from, to};
}

/// @brief The columns of a span of one line (a match, a bracket) on one of its rows.
/// @param column The span's first column.
/// @param length Its code points.
/// @param start The row's first column.
/// @param end One past its last.
/// @return Its first column on the row and the column after its last; empty if it is not on the row.
std::pair<std::size_t, std::size_t> spanColumns(std::size_t column, std::size_t length, std::size_t start,
                                                std::size_t end) noexcept {
	return {std::max(column, start), std::min(column + length, end)};
}

/// @brief Whether a decoration is drawn under the text and reaches the visible lines.
/// @param d The decoration.
/// @param firstLine The first visible line.
/// @return true if it is.
bool backgroundFrom(const EditorDecoration& d, std::size_t firstLine) noexcept {
	return d.kind == EditorDecoration::Kind::Background && d.endLine >= firstLine;
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
		if(!backgroundFrom(d, firstLine)) continue;
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
			const auto [from, to] = spanColumns(m.column, m.length, span.start, span.end);
			if(from < to) paintColumns(painter, layout, span, row, {from, to}, matchColor_, matchOutline_ ? -1.0f : 0.0f);
		});

	if(const auto brackets = matchingBrackets())
		for(const Position p : {brackets->first, brackets->second})
			forRowsOf(layout, p.line, p.line, [&](std::size_t row, RowSpan span) {
				const auto [from, to] = spanColumns(p.column, 1, span.start, span.end);
				if(from < to)
					paintColumns(painter, layout, span, row, {from, to}, bracketColor_,
					             bracketUnderline_ ? 2.0f : 0.0f);
			});
}

void TextEditor::paintAbove(Painter& painter, const Font& font, const Layout& layout) const {
	paintGuides(painter, font, layout);
	paintDecorationsAbove(painter, font, layout);
	paintCarets(painter, font, layout);
}

float TextEditor::rowX(RowSpan span, std::size_t column) const {
	return columnX(span.line, column) - (wrap_ ? columnX(span.line, span.start) : 0.0f);
}

void TextEditor::paintGuides(Painter& painter, const Font& font, const Layout& layout) const {
	const float lh = layout.lineHeight;
	const float space = font.glyph(U' ').advance;
	for(std::size_t row = layout.first; row < layout.last; ++row) {
		const RowSpan span = rowSpan(row);
		const float top = lineTop(layout, row);
		// Indentation guides, on a line's first row, at each level but the deepest
		if(guides_ && span.start == 0)
			for(std::size_t column = tabSize_; column < indentOf(span.line) && column < span.end; column += tabSize_) {
				const float x = std::round(layout.textLeft + static_cast<float>(column) * space);
				painter.fillRect({x, top, x + 1.0f, top + lh}, guideColor_);
			}
		// A folded line ends in three dots
		if(viewActive() && span.end == lines_[span.line].size() && nextShown(span.line) > span.line + 1) {
			const float x = layout.textLeft + rowX(span, span.end) + space;
			for(int dot = 0; dot < 3; ++dot)
				painter.fillRect({x + static_cast<float>(dot) * 4.0f, top + lh - 8.0f,
				                  x + static_cast<float>(dot) * 4.0f + 2.0f, top + lh - 6.0f},
				                 lineNumberColor_);
		}
	}
}

void TextEditor::paintDecorationsAbove(Painter& painter, const Font& font, const Layout& layout) const {
	const float lh = layout.lineHeight;
	const float space = font.glyph(U' ').advance;
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
				// NOLINTNEXTLINE(bugprone-float-loop-counter): steps along a row of pixels, ending at its right
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
}

void TextEditor::paintCarets(Painter& painter, const Font& font, const Layout& layout) const {
	const float lh = layout.lineHeight;
	if(hasFocus())
		for(const EditorSelection& s : cursors_) {
			const std::size_t row = rowOf(s.caret);
			if(row < layout.first || row >= layout.last || hidden(s.caret.line)) continue;
			const float x = std::round(layout.textLeft + xOnRow(s.caret)), top = lineTop(layout, row);
			paintCaret(painter, font, layout, s.caret, x, top);
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
