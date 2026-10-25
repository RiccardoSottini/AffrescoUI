/// @file
/// @brief TextEditor's view: the lines its visible rows show, for "Top", "Bot" and "42%" indicators, scroll bars
/// and minimaps outside it.

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

namespace {

/// @brief A count of rows, from a float that may be past any count (a huge frame, a tiny line height) or NaN.
/// @param rows The rows, as a float.
/// @param count The most it can be.
/// @return It, between 0 and count: converting a float past size_t's range is undefined.
std::size_t rowsUpTo(float rows, std::size_t count) {
	if(!(rows > 0.0f)) return 0; // NaN too
	return rows >= static_cast<float>(count) ? count : static_cast<std::size_t>(rows);
}

} // namespace

TextEditor::View TextEditor::view() const {
	const std::size_t count = std::max<std::size_t>(rowCount(), 1);
	const float lh = lineHeight();
	const std::size_t rows = visibleLines();
	if(lh <= 0.0f) return {1, rowSpan(std::min(rows, count) - 1).line + 1, rows, lines_.size()};
	// The rows shown whole: a row the top edge cuts isn't the first, nor one the bottom edge cuts the last
	const float scroll = std::max(scroll_.y, 0.0f);
	const std::size_t first = std::min(rowsUpTo(std::ceil(scroll / lh - 0.01f), count), count - 1);
	const float bottom = (scroll + frame().height() - textPadding) / lh;
	const std::size_t shown = std::max<std::size_t>(rowsUpTo(std::floor(bottom + 0.01f), count), 1);
	const std::size_t last = std::clamp(std::min(shown, count) - 1, first, count - 1);
	return {rowSpan(first).line + 1, rowSpan(last).line + 1, rows, lines_.size()};
}

void TextEditor::noteView() {
	const View now = view();
	if(now == lastView_) return;
	lastView_ = now;
	onViewChanged(now);
}

} // namespace affresco
