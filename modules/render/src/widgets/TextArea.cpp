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

void TextArea::setPlaceholderColor(Color color) { setLook(placeholderColor_, color); }

void TextArea::setSecret(bool secret) {
	if(secret_ == secret) return;
	secret_ = secret;
	invalidate();
}

bool TextArea::accepts(char32_t codepoint) const noexcept {
	const bool digit = codepoint >= U'0' && codepoint <= U'9';
	// Letters: ASCII ones, and any character past Latin-1's symbols that isn't a digit or punctuation we know
	const bool asciiLetter = (codepoint >= U'a' && codepoint <= U'z') || (codepoint >= U'A' && codepoint <= U'Z');
	const bool letter = asciiLetter || (codepoint >= 0xC0 && codepoint != 0xD7 && codepoint != 0xF7 &&
	                                    !(codepoint >= 0x2000 && codepoint <= 0x2BFF) && !(codepoint >= 0x3000 && codepoint <= 0x303F));
	switch(accept_) {
	case Accept::Any: return true;
	case Accept::Digits: return digit;
	case Accept::Letters: return letter;
	case Accept::Alphanumeric: return digit || letter;
	}
	return true;
}

bool TextArea::insert(std::u32string_view added) {
	bool changed = false;
	for(const char32_t codepoint : added) {
		if(maxLength_ != 0 && text_.size() >= maxLength_) break;
		if(!accepts(codepoint)) continue;
		text_.push_back(codepoint);
		changed = true;
	}
	if(changed) invalidate();
	return changed;
}

std::u32string TextArea::shownText() const {
	return secret_ ? std::u32string(text_.size(), U'\u2022') : text_;
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
		if(insert(pasted)) onTextChanged(text());
	}

	if(secret_ && event.key == Key::C) return; // a secret is never copied
	Label::keyEvent(event);
}

void TextArea::charEvent(const CharEvent& event) {
	if(insert(std::u32string_view(&event.codepoint, 1))) onTextChanged(text());

	Label::charEvent(event);
}

void TextArea::focusEvent(bool focused) {
	invalidatePaint(); // caret appears/disappears

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
		const std::u32string shown = shownText();
		const float width = f->measure(shown);
		const float left = std::round(content.left + std::min(0.0f, content.width() - width - 2.0f));
		painter.drawText(*f, shown, {left, baseline}, textColor());
	}

	if(hasFocus()) {
		const float caretX =
		    std::round(std::min(content.left + f->measure(shownText()) + 1.0f, content.right - 2.0f));
		painter.fillRect({caretX, baseline - f->ascent(), caretX + 2.0f, baseline - f->ascent() + f->lineHeight()},
		                 textColor());
	}
}

} // namespace affresco
