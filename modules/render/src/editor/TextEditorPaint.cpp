/// @file
/// @brief TextEditor drawing: the visible rows in runs of one colour, the selections, the gutter, the caret and the
/// scroll bar (what goes under and over the text is in TextEditorPaintView.cpp).

#include "affresco/render/Painter.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <tuple>
#include <utility>

namespace affresco {
namespace {

/// @brief Where the colouring of the visible lines starts, and the state it carries from line to line.
struct ColorCursor {
	std::size_t line = 0; ///< The line coloured next.
	int state = 0;        ///< The state it starts with.
};

/// @brief Where the colouring starts: at the first visible line with a highlighter, which keeps its own state, or else
/// at the last line whose state is known (an open block comment may come from above the visible lines).
/// @param states The state each line starts with, for the lines coloured so far; seeded with the first line's.
/// @param firstLine The first visible line.
/// @param highlighted Whether a highlighter colours the lines.
/// @return The line and its state.
ColorCursor colorStart(std::vector<int>& states, std::size_t firstLine, bool highlighted) {
	if(states.empty()) states.push_back(0);
	if(highlighted) return {firstLine, states[0]};
	const std::size_t line = std::min(states.size() - 1, firstLine);
	return {line, states[line]};
}

/// @brief Steps the colouring past a line: over the folded lines after it, when a highlighter colours the lines or
/// when the state they end with is known, or else into them.
/// @param at The line just coloured and its state; moved on.
/// @param next The next line shown after it.
/// @param highlighted Whether a highlighter colours the lines.
/// @param states The state each line starts with, for the lines coloured so far.
void colorStep(ColorCursor& at, std::size_t next, bool highlighted, const std::vector<int>& states) noexcept {
	++at.line;
	if(next <= at.line) return;
	if(highlighted) at.line = next;
	else if(next < states.size()) at = {next, states[next]};
}

/// @brief Splits a row into runs of one colour, each placed where the whole line's shaping puts it (so the caret
/// matches): left to right, a run starts where the one before it ends; otherwise (bidirectional text) at its leftmost
/// caret.
/// @param colors The line's colours.
/// @param shaped The line's shaping, with an advance and a caret for every code point of the row.
/// @param columns The row's first column and the column after its last.
/// @param wrapped Whether lines wrap: a row's x is then from its first caret.
/// @param runs Cleared, then filled with each run's end, x and width.
void colorRuns(const std::vector<Color>& colors, const Font::ShapedText& shaped,
               std::pair<std::size_t, std::size_t> columns, bool wrapped,
               std::vector<std::tuple<std::size_t, float, float>>& runs) {
	const bool leftToRight = std::ranges::is_sorted(shaped.carets);
	const float origin = wrapped ? shaped.carets[columns.first] : 0.0f;
	runs.clear();
	float pen = 0.0f;
	for(std::size_t start = columns.first; start < columns.second;) {
		std::size_t end = start + 1;
		while(end < columns.second && colors[end] == colors[start]) ++end;
		float width = 0.0f, left = shaped.carets[start];
		for(std::size_t i = start; i < end; ++i) {
			width += shaped.advances[i];
			left = std::min(left, shaped.carets[i + 1]);
		}
		runs.emplace_back(end, leftToRight ? pen : left - origin, width);
		pen += width;
		start = end;
	}
}

/// @brief Where a gutter decoration goes on its line, in window units.
struct MarkPlace {
	float left = 0.0f;     ///< The gutter's left edge.
	float baseline = 0.0f; ///< The line's text baseline, for an icon.
	float middle = 0.0f;   ///< The line's middle, for a dot.
};

/// @brief Draws a gutter decoration: its icon, or else a dot.
/// @param painter The painter.
/// @param font The editor's font.
/// @param mark The decoration.
/// @param at Where it goes.
void paintGutterMark(Painter& painter, const Font& font, const EditorDecoration& mark, const MarkPlace& at) {
	if(mark.icon != 0)
		return painter.drawText(font, std::u32string_view(&mark.icon, 1), {at.left + 2.0f, at.baseline}, mark.color);
	painter.fillRoundedRect({at.left + 3.0f, at.middle - 3.0f, at.left + 9.0f, at.middle + 3.0f},
	                        CornerRadii::all(3.0f), mark.color);
}

/// @brief Whether a row is among those drawn.
/// @param row The row.
/// @param first The first row drawn.
/// @param last The row after the last one drawn.
/// @return true if it is.
constexpr bool drawnRow(std::size_t row, std::size_t first, std::size_t last) noexcept {
	return row >= first && row < last;
}

} // namespace

void TextEditor::colorLine(std::u32string_view /*line*/, int& /*state*/, std::vector<Color>& colors) const {
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
	                    .last = std::min(rowCount(), first + visibleLines() + 3)};
	if(layout.first >= layout.last) return paintGutter(painter, *f, layout);
	const std::size_t firstLine = rowSpan(layout.first).line, lastLine = rowSpan(layout.last - 1).line + 1;

	if(const std::size_t caretRow = rowOf(caret_); !hasSelection() && drawnRow(caretRow, layout.first, layout.last)) {
		const float top = lineTop(layout, caretRow);
		painter.fillRect({box.left, top, box.right, top + lh}, lineColor_);
	}
	// Rulers, the whole height of the text, under it
	const float space = f->glyph(U' ').advance;
	for(const std::size_t column : rulers_) {
		const float x = std::round(layout.textLeft + static_cast<float>(column) * space);
		if(x >= box.left + layout.gutter && x < box.right)
			painter.fillRect({x, box.top, x + 1.0f, box.bottom}, guideColor_);
	}
	paintUnder(painter, *f, layout);

	// Each line's colouring state is kept, so the lines are coloured from the last one known (folded lines whose state
	// is known are skipped); a highlighter keeps its own and colours only the lines drawn
	colorLines(firstLine, lastLine, [&](std::size_t line) {
		if(line >= firstLine && !hidden(line))
			for(std::size_t row = std::max(rowOf({line, 0}), layout.first); row < layout.last; ++row) {
				const RowSpan span = rowSpan(row);
				if(span.line != line) break;
				paintLine(painter, *f, layout, span, row);
			}
	});

	paintAbove(painter, *f, layout);
	paintGutter(painter, *f, layout);
	paintMinimap(painter, layout);
	paintCaretAndScrollBar(painter, layout);
}

float TextEditor::lineTop(const Layout& layout, std::size_t row) noexcept {
	return std::round(layout.firstTop + static_cast<float>(row) * layout.lineHeight);
}

void TextEditor::paintLine(Painter& painter, const Font& font, const Layout& layout, RowSpan span,
                           std::size_t row) const {
	const Rect& box = frame();
	const float top = lineTop(layout, row);
	const std::u32string& text = lines_[span.line];

	paintSelection(painter, font, layout, span, row, selection());
	// The extra cursors are sorted and apart: those that reach this line come together
	const auto reaches = std::ranges::lower_bound(cursors_, Position{span.line, 0}, {},
	                                              [](const EditorSelection& s) { return std::max(s.anchor, s.caret); });
	for(auto it = reaches; it != cursors_.end() && std::min(it->anchor, it->caret).line <= span.line; ++it)
		paintSelection(painter, font, layout, span, row, std::minmax(it->anchor, it->caret));

	// Runs of one colour, each shaped on its own. Their places are read first: drawing a run shapes it, which may drop
	// the line's shaping from the cache
	const Font::ShapedText& shaped = font.shape(text);
	if(shaped.advances.size() < text.size() || shaped.carets.size() <= span.end) return; // out of memory
	colorRuns(colors_, shaped, {span.start, span.end}, wrap_, runs_);
	std::size_t start = span.start;
	for(const auto& [end, x, width] : runs_) {
		const float left = layout.textLeft + x;
		if(left + width >= box.left + layout.gutter && left <= box.right)
			painter.drawText(font, std::u32string_view(text).substr(start, end - start), {left, top + layout.baseline},
			                 colors_[start]);
		start = end;
	}
}

void TextEditor::paintSelection(Painter& painter, const Font& /*font*/, const Layout& layout, RowSpan span,
                                std::size_t row, std::pair<Position, Position> selection) const {
	if(const auto columns = selectedColumns(span, selection))
		paintColumns(painter, layout, span, row, *columns, selectionColor_);
}

std::optional<std::pair<std::size_t, std::size_t>>
TextEditor::selectedColumns(RowSpan span, std::pair<Position, Position> selection) const {
	const auto [from, to] = selection;
	const bool lastRow = span.end == lines_[span.line].size();
	if(!rowSelected(span, selection, lastRow)) return std::nullopt;
	const std::size_t start = span.line == from.line ? std::max(from.column, span.start) : span.start;
	const std::size_t end = span.line == to.line ? std::min(to.column, span.end) : span.end;
	// A selected line break shows as a space's width
	const bool lineBreak = span.line != to.line && lastRow;
	if(start >= end && !lineBreak) return std::nullopt;
	return std::pair{start, lineBreak ? end + 1 : end};
}

bool TextEditor::rowSelected(RowSpan span, std::pair<Position, Position> selection, bool lastRow) noexcept {
	const auto [from, to] = selection;
	if(from == to || span.line < from.line || span.line > to.line) return false;
	if(span.line == from.line && (from.column > span.end || (!lastRow && from.column == span.end))) return false;
	return span.line != to.line || to.column >= span.start;
}

void TextEditor::colorLines(std::size_t first, std::size_t last,
                            const std::function<void(std::size_t)>& body) const {
	const bool highlighted = highlighter_ != nullptr;
	for(ColorCursor at = colorStart(states_, first, highlighted); at.line < last;) {
		const std::u32string& text = lines_[at.line];
		if(highlighted) {
			colors_.assign(text.size(), textColor_);
			highlighter_->colorLine(*model_, at.line, text, colors_);
		} else {
			colors_.resize(text.size());
			colorLine(text, at.state, colors_);
			if(at.line + 1 == states_.size()) states_.push_back(at.state);
		}
		body(at.line);
		colorStep(at, nextShown(at.line), highlighted, states_);
	}
}

void TextEditor::paintColumns(Painter& painter, const Layout& layout, RowSpan span, std::size_t row,
                              std::pair<std::size_t, std::size_t> columns, Color color) const {
	paintColumns(painter, layout, span, row, columns, color, 0.0f);
}

void TextEditor::paintColumns(Painter& painter, const Layout& layout, RowSpan span, std::size_t row,
                              std::pair<std::size_t, std::size_t> columns, Color color, float thickness) const {
	const Font* f = font();
	if(!f) return;
	const std::u32string& text = lines_[span.line];
	const bool lineBreak = columns.second > span.end;
	const std::size_t end = std::min(columns.second, span.end);
	// Measured before the carets are read: measuring may drop the line's shaping from the cache
	const float measured = lineBreak && !wrap_ ? f->measure(text) : 0.0f;
	const float space = lineBreak ? f->glyph(U' ').advance : 0.0f;
	const std::vector<float>& carets = f->shape(text).carets;
	if(carets.empty()) return;
	const auto x = [&carets](std::size_t column) { return carets[std::min(column, carets.size() - 1)]; };
	const float origin = wrap_ ? x(span.start) : 0.0f;
	const float bottom = lineTop(layout, row) + layout.lineHeight;
	const float top = thickness > 0.0f ? bottom - thickness : bottom - layout.lineHeight;
	const auto fill = [&](float a, float b) {
		const Rect rect{layout.textLeft + std::min(a, b), top, layout.textLeft + std::max(a, b), bottom};
		if(thickness < 0.0f) painter.strokeRoundedRect(rect, CornerRadii::all(2.0f), -thickness, color);
		else painter.fillRect(rect, color);
	};
	const float breakLeft = wrap_ ? x(span.end) - origin : measured;
	if(std::ranges::is_sorted(carets)) {
		const float right = lineBreak ? breakLeft + space : x(end) - origin;
		return fill(x(columns.first) - origin, right);
	}
	// Bidirectional text: each code point where it is drawn
	for(std::size_t column = columns.first; column < end; ++column) fill(x(column) - origin, x(column + 1) - origin);
	if(lineBreak) fill(breakLeft, breakLeft + space);
}

void TextEditor::paintGutter(Painter& painter, const Font& font, const Layout& layout) const {
	// The gutter covers text scrolled under it
	const Rect& box = frame();
	if(color().a > 0.0f) painter.fillRect({box.left, box.top, box.left + layout.gutter - 8.0f, box.bottom}, color());
	for(std::size_t row = layout.first; row < layout.last; ++row) {
		const RowSpan span = rowSpan(row);
		if(span.start != 0) continue; // a wrapped line's label is on its first row
		const float top = lineTop(layout, row);
		if(lineNumbers_) {
			lineLabel(span.line, label_);
			const float x = std::round(box.left + layout.gutter - 20.0f - font.measure(label_));
			painter.drawText(font, label_, {x, top + layout.baseline},
			                 span.line == caret_.line ? activeLineNumberColor_ : lineNumberColor_);
		}
		// A folded line's marker: a short bar where the gutter ends
		if(viewActive() && nextShown(span.line) > span.line + 1) {
			const float x = box.left + layout.gutter - 14.0f;
			painter.fillRoundedRect({x, top + 5.0f, x + 3.0f, top + layout.lineHeight - 5.0f}, CornerRadii::all(1.5f),
			                        activeLineNumberColor_);
		}
	}
	// Gutter icons, on their first line
	const auto from = std::ranges::lower_bound(decorations_, rowSpan(std::min(layout.first, rowCount() - 1)).line, {},
	                                           &EditorDecoration::line);
	for(auto it = from; it != decorations_.end() && rowOf({it->line, 0}) < layout.last; ++it) {
		if(it->kind != EditorDecoration::Kind::Gutter || hidden(it->line)) continue;
		const float top = lineTop(layout, rowOf({it->line, 0}));
		paintGutterMark(painter, font, *it, {box.left, top + layout.baseline, top + layout.lineHeight / 2.0f});
	}
}

void TextEditor::paintCaretAndScrollBar(Painter& painter, const Layout& layout) const {
	const Rect& box = frame();
	const float lh = layout.lineHeight;
	if(hasFocus()) {
		const float x = std::round(layout.textLeft + xOnRow(caret_));
		const float top = lineTop(layout, rowOf(caret_));
		if(x >= box.left + layout.gutter - 8.0f)
			painter.fillRect({x, top + 1.0f, x + 2.0f, top + lh - 1.0f}, caretColor_);
	}

	// A scroll bar thumb as long as the visible share of the text
	const float rows = static_cast<float>(rowCount());
	const float content = rows * lh + box.height() - lh;
	if(rows * lh + textPadding > box.height()) {
		const float length = std::max(box.height() * box.height() / content, 16.0f);
		const float top = box.top + scroll_.y / (content - box.height()) * (box.height() - length);
		painter.fillRect({box.right - scrollBarWidth - 2.0f, top, box.right - 2.0f, top + length},
		                 lineNumberColor_.withAlpha(0.9f));
	}
}

} // namespace affresco
