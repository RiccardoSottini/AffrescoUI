/// @file
/// @brief TextEditor's scroll bar marks: where the decorations, find's matches and the caret are in the whole text,
/// drawn over the scroll bar so they can be found again.

#include "affresco/render/Painter.hpp"
#include "affresco/render/TextEditor.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

namespace {

constexpr float scrollMarkHeight = 3.0f; ///< Height of one mark, in pixels.

} // namespace

void TextEditor::setScrollMarks(bool shown) {
	if(shown == scrollMarks_) return;
	scrollMarks_ = shown;
	invalidatePaint();
}

const std::vector<std::size_t>& TextEditor::matchLines() const {
	const std::array<std::uint64_t, 2> key{model_->version(), findGeneration_};
	if(matchLinesFound_ && key == matchLinesKey_) return matchLines_;
	matchLinesFound_ = true;
	matchLinesKey_ = key;
	matchLines_.clear();
	if(!find_.valid()) return matchLines_;
	std::size_t count = 0;
	for(std::size_t line = 0; line < lines_.size() && count < maxMatches; ++line) {
		lineMatches(line, 0, maxMatches - count, found_);
		if(found_.empty()) continue;
		count += found_.size();
		matchLines_.push_back(line);
	}
	return matchLines_;
}

std::vector<TextEditor::ScrollMark> TextEditor::scrollMarkList() const {
	std::vector<ScrollMark> marks;
	if(!scrollMarks_) return marks;
	const auto add = [&marks](std::size_t row, Color color) {
		color.a = 1.0f;
		if(!marks.empty() && marks.back().row == row && marks.back().color == color) return;
		marks.push_back({row, color});
	};
	for(const EditorDecoration& decoration : decorations_)
		if(decoration.kind != EditorDecoration::Kind::Inline && decoration.line < lines_.size())
			add(rowOf({decoration.line, 0}), decoration.color);
	for(const std::size_t line : matchLines()) add(rowOf({line, 0}), matchColor_);
	add(rowOf(caret_), caretColor_);
	std::ranges::stable_sort(marks, {}, &ScrollMark::row);
	return marks;
}

void TextEditor::paintScrollMarks(Painter& painter) const {
	if(!scrollMarks_) return;
	const Rect& box = frame();
	const float rows = static_cast<float>(std::max<std::size_t>(rowCount(), 1));
	const float lane = box.height() - scrollMarkHeight;
	for(const ScrollMark& mark : scrollMarkList()) {
		const float top = std::round(box.top + (static_cast<float>(mark.row) + 0.5f) / rows * lane);
		painter.fillRect({box.right - scrollBarWidth - 2.0f, top, box.right - 2.0f, top + scrollMarkHeight},
		                 mark.color);
	}
}

} // namespace affresco
