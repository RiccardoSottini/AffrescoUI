#include "affresco/render/TextArea.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

TextArea::TextArea() {
	setColor(colors::white);
	setCursor(Cursor::IBeam);
}

void TextArea::clear() noexcept {
	text_.clear();
	invalidate();
}

void TextArea::setPlaceholder(std::string_view utf8) {
	placeholder_ = detail::fromUtf8(utf8);
	invalidate();
}

void TextArea::setPlaceholderColor(Color color) {
	placeholderColor_ = color;
	invalidate();
}

void TextArea::keyEvent(const KeyEvent& event) {
	if(event.key == Key::Backspace && event.action != Action::Release && !text_.empty()) {
		text_.pop_back();
		invalidate();
		onTextChanged(text());
	}

	const bool paste =
	    event.key == Key::V && event.action != Action::Release && (event.mods.super() || event.mods.control());
	if(paste && window()) {
		std::u32string pasted = detail::fromUtf8(window()->clipboardText());
		std::ranges::replace_if(pasted, [](char32_t c) { return c == U'\n' || c == U'\r' || c == U'\t'; }, U' ');
		if(!pasted.empty()) {
			text_ += pasted;
			invalidate();
			onTextChanged(text());
		}
	}

	Label::keyEvent(event);
}

void TextArea::charEvent(const CharEvent& event) {
	text_.push_back(event.codepoint);
	invalidate();
	onTextChanged(text());

	Label::charEvent(event);
}

void TextArea::focusEvent(bool focused) {
	invalidate(); // caret appears/disappears

	Label::focusEvent(focused);
}

void TextArea::paint(Painter& painter) const {
	Panel::paint(painter);

	const Font* f = font();
	if(!f) return;

	const Rect content = contentRect();
	const float baseline = std::round(content.top + (content.height() - f->lineHeight()) / 2.0f + f->ascent());

	if(text_.empty()) {
		if(!placeholder_.empty())
			painter.drawText(*f, placeholder_, {std::round(content.left), baseline}, placeholderColor_);
	} else {
		// Keep the end of the text (where typing happens) visible
		const float width = f->measure(text_);
		const float left = std::round(content.left + std::min(0.0f, content.width() - width - 2.0f));
		painter.drawText(*f, text_, {left, baseline}, textColor());
	}

	if(hasFocus()) {
		const float caretX = std::round(std::min(content.left + f->measure(text_) + 1.0f, content.right - 2.0f));
		painter.fillRect({caretX, baseline - f->ascent(), caretX + 2.0f, baseline - f->ascent() + f->lineHeight()},
		                 textColor());
	}
}

} // namespace affresco
