/// @file
/// @brief CodeInput: cells, their colours, and painting one character in each.

#include "affresco/render/CodeInput.hpp"
#include "affresco/render/Painter.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace affresco {

CodeInput::CodeInput() {
	setColor(colors::transparent);
	setAccept(Accept::Digits);
	setMaxLength(length_);
}

void CodeInput::setLength(std::size_t length) {
	length = std::clamp<std::size_t>(length, 1, mostCells);
	if(length == length_) return;
	length_ = length;
	setMaxLength(length);
	if(text_.size() > length) text_.resize(length);
	invalidate();
}

void CodeInput::setCellColor(Color color) { setLook(cellColor_, color); }

void CodeInput::setCellBorderColor(Color color) { setLook(cellBorderColor_, color); }

void CodeInput::setActiveColor(Color color) { setLook(activeColor_, color); }

void CodeInput::setGap(float gap) { setLook(gap_, std::max(0.0f, gap)); }

void CodeInput::setCellRadius(float radius) { setLook(cellRadius_, std::max(0.0f, radius)); }

void CodeInput::paint(Painter& painter) const {
	Panel::paint(painter); // NOLINT(bugprone-parent-virtual-call): it draws its own cells, not TextArea's text

	const Rect content = contentRect();
	const auto cells = static_cast<float>(length_);
	const float width = std::max(1.0f, (content.width() - gap_ * (cells - 1.0f)) / cells);
	const Font* f = font();
	const std::size_t next = std::min(text_.size(), length_ - 1);
	for(std::size_t index = 0; index < length_; ++index) {
		const float left = std::round(content.left + static_cast<float>(index) * (width + gap_));
		const Rect cell{left, content.top, std::round(left + width), content.bottom};
		const bool active = hasFocus() && index == next;
		painter.fillRoundedRect(cell, CornerRadii::all(cellRadius_), cellColor_);
		painter.strokeRoundedRect(cell, CornerRadii::all(cellRadius_), active ? 2.0f : 1.0f,
		                          active ? activeColor_ : cellBorderColor_);
		if(!f || index >= text_.size()) continue;
		const std::u32string character(1, secret() ? U'•' : text_[index]);
		const float x = std::round(cell.left + (cell.width() - f->measure(character)) / 2.0f);
		const float baseline = std::round(cell.top + (cell.height() - f->lineHeight()) / 2.0f + f->ascent());
		painter.drawText(*f, character, {x, baseline}, textColor());
	}
}

} // namespace affresco
