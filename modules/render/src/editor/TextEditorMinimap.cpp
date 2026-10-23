/// @file
/// @brief TextEditor's minimap: a narrow overview of the rows at the right, with the visible ones marked, that scrolls
/// the editor when pressed or dragged.

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

namespace {

constexpr float minimapRowHeight = 2.0f;   ///< Height of one row in the minimap, in pixels.
constexpr float minimapColumnWidth = 1.0f; ///< Width of one column in the minimap, in pixels.

/// @brief A row's text drawn as one bar: from its first non-blank column to its last.
/// @param text The row's line.
/// @param start The row's first column.
/// @param end One past its last.
/// @return The first and one past the last non-blank column (equal: blank).
std::pair<std::size_t, std::size_t> inkOf(const std::u32string& text, std::size_t start, std::size_t end) noexcept {
	end = std::min(end, text.size());
	std::size_t first = start;
	while(first < end && (text[first] == U' ' || text[first] == U'\t')) ++first;
	std::size_t last = end;
	while(last > first && (text[last - 1] == U' ' || text[last - 1] == U'\t')) --last;
	return {first - start, last - start};
}

} // namespace

void TextEditor::setMinimap(bool shown) {
	if(shown == minimap_) return;
	minimap_ = shown;
	minimapDragging_ = false;
	if(wrap_) rebuildRows();
	invalidate();
}

void TextEditor::setMinimapText(bool text) {
	if(text == minimapText_) return;
	minimapText_ = text;
	invalidatePaint();
}

void TextEditor::setMinimapViewportOnHover(bool onHover) {
	if(onHover == minimapViewportOnHover_) return;
	minimapViewportOnHover_ = onHover;
	invalidatePaint();
}

float TextEditor::minimapWidth() const noexcept { return minimap_ ? minimapSize + 4.0f : 0.0f; }

Rect TextEditor::minimapFrame() const noexcept {
	const Rect& box = frame();
	const float right = box.right - scrollBarWidth - 4.0f;
	return {right - minimapSize, box.top, right, box.bottom};
}

float TextEditor::minimapOffset() const noexcept {
	// The minimap scrolls with the editor when its rows don't fit
	const float rows = static_cast<float>(rowCount()) * minimapRowHeight;
	const float height = frame().height();
	if(rows <= height) return 0.0f;
	const float lh = lineHeight();
	const float maxScroll = std::max(static_cast<float>(rowCount()) * lh - lh, 1.0f);
	return std::clamp(scroll_.y / maxScroll, 0.0f, 1.0f) * (rows - height);
}

void TextEditor::paintMinimap(Painter& painter, const Layout& layout) const {
	if(!minimap_) return;
	const Rect map = minimapFrame();
	painter.fillRect(map, color().a > 0.0f ? color() : lineColor_);
	const float offset = minimapOffset();
	const auto first = static_cast<std::size_t>(offset / minimapRowHeight);
	const std::size_t last =
	    std::min(rowCount(), first + static_cast<std::size_t>(map.height() / minimapRowHeight) + 1);
	const Color ink = textColor_.withAlpha(0.35f);
	const auto paintRow = [&](std::size_t row, const RowSpan& span) {
		const std::u32string& text = lines_[span.line];
		const auto [from, to] = inkOf(text, span.start, span.end);
		if(from == to) return;
		const float top = map.top + static_cast<float>(row) * minimapRowHeight - offset;
		const auto x = [&](std::size_t column) {
			return std::min(map.left + 2.0f + static_cast<float>(column) * minimapColumnWidth, map.right - 2.0f);
		};
		if(!minimapText_) {
			if(x(from) < x(to)) painter.fillRect({x(from), top, x(to), top + minimapRowHeight - 0.5f}, ink);
			return;
		}
		// Runs of one colour, blanks skipped, each code point a column
		for(std::size_t column = from; column < to;) {
			const std::size_t at = span.start + column;
			if(text[at] == U' ' || text[at] == U'\t' || at >= colors_.size()) {
				++column;
				continue;
			}
			std::size_t end = column + 1;
			while(end < to && span.start + end < colors_.size() && colors_[span.start + end] == colors_[at] &&
			      text[span.start + end] != U' ' && text[span.start + end] != U'\t')
				++end;
			if(x(column) < x(end))
				painter.fillRect({x(column), top, x(end), top + minimapRowHeight - 0.5f}, colors_[at].withAlpha(0.6f));
			column = end;
		}
	};
	if(!minimapText_)
		for(std::size_t row = first; row < last; ++row) paintRow(row, rowSpan(row));
	else if(first < last)
		colorLines(rowSpan(first).line, rowSpan(last - 1).line + 1, [&](std::size_t line) {
			if(hidden(line)) return;
			for(std::size_t row = std::max(rowOf({line, 0}), first); row < last; ++row) {
				const RowSpan span = rowSpan(row);
				if(span.line != line) break;
				paintRow(row, span);
			}
		});
	// The visible rows, as a slider (only while the pointer is over the minimap, when asked)
	if(minimapViewportOnHover_ && !minimapHovered_ && !minimapDragging_) return;
	const float sliderTop = map.top + scroll_.y / layout.lineHeight * minimapRowHeight - offset;
	const float sliderHeight = frame().height() / layout.lineHeight * minimapRowHeight;
	painter.fillRect({map.left, std::max(sliderTop, map.top), map.right, std::min(sliderTop + sliderHeight, map.bottom)},
	                 lineNumberColor_.withAlpha(0.18f));
}

bool TextEditor::minimapPress(Vec2 position, bool pressed) {
	if(!pressed) {
		const bool was = minimapDragging_;
		minimapDragging_ = false;
		return was;
	}
	if(!minimap_ || !minimapFrame().contains(position)) return false;
	minimapDragging_ = true;
	minimapDrag(position);
	return true;
}

bool TextEditor::minimapDrag(Vec2 position) {
	if(!minimapDragging_) return false;
	// The row under the pointer comes to the middle of the editor
	const float row = (position.y - frame().top + minimapOffset()) / minimapRowHeight;
	const float lh = lineHeight();
	setScroll({scroll_.x, std::round(row * lh - frame().height() / 2.0f)});
	return true;
}

} // namespace affresco
