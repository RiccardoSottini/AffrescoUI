/// @file
/// @brief TextEditor's view: the lines its visible rows show, for "Top", "Bot" and "42%" indicators, scroll bars
/// and minimaps outside it.

#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

TextEditor::View TextEditor::view() const {
	const std::size_t count = std::max<std::size_t>(rowCount(), 1);
	const float lh = lineHeight();
	const std::size_t rows = visibleLines();
	if(lh <= 0.0f) return {1, rowSpan(std::min(rows, count) - 1).line + 1, rows, lines_.size()};
	// The rows shown whole: a row the top edge cuts isn't the first, nor one the bottom edge cuts the last
	const float scroll = std::max(scroll_.y, 0.0f);
	const auto first = std::min(static_cast<std::size_t>(std::ceil(scroll / lh - 0.01f)), count - 1);
	const float bottom = (scroll + frame().height() - textPadding) / lh;
	const auto shown = static_cast<std::size_t>(std::max(std::floor(bottom + 0.01f), 1.0f));
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
