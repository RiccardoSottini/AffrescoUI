#include "affresco/render/TextArea.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {

namespace {

/// @brief Whether a character is an ASCII digit.
/// @param codepoint The character.
/// @return true for 0 to 9.
constexpr bool isDigit(char32_t codepoint) noexcept { return codepoint >= U'0' && codepoint <= U'9'; }

/// @brief Whether a character is a letter: an ASCII one, or any past Latin-1's symbols that isn't punctuation we
/// know (the general and CJK punctuation, the arrows, the symbols and the box drawing).
/// @param codepoint The character.
/// @return true if it counts as a letter.
constexpr bool isLetter(char32_t codepoint) noexcept {
	const bool asciiLetter = (codepoint >= U'a' && codepoint <= U'z') || (codepoint >= U'A' && codepoint <= U'Z');
	if(asciiLetter) return true;
	if(codepoint < 0xC0 || codepoint == 0xD7 || codepoint == 0xF7) return false;
	return (codepoint < 0x2000 || codepoint > 0x2BFF) && (codepoint < 0x3000 || codepoint > 0x303F);
}

/// @brief Whether a key event pastes: V with Command or Control, pressed or repeated.
/// @param event The event.
/// @return true if it pastes.
bool isPaste(const KeyEvent& event) noexcept {
	return event.key == Key::V && event.action != Action::Release && (event.mods.super() || event.mods.control());
}

/// @brief Clipboard text as one line: its line breaks and tabs become spaces.
/// @param utf8 The text.
/// @return The line.
std::u32string pastedLine(std::string_view utf8) {
	std::u32string pasted = detail::fromUtf8(utf8);
	std::ranges::replace(pasted, U'\n', U' ');
	std::ranges::replace(pasted, U'\r', U' ');
	std::ranges::replace(pasted, U'\t', U' ');
	return pasted;
}

} // namespace

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
	switch(accept_) {
	case Accept::Any:
		return true;
	case Accept::Digits:
		return isDigit(codepoint);
	case Accept::Letters:
		return isLetter(codepoint);
	case Accept::Alphanumeric:
		return isDigit(codepoint) || isLetter(codepoint);
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

bool TextArea::insertText(std::string_view utf8) {
	if(!insert(detail::fromUtf8(utf8))) return false;
	onTextChanged(text());
	return true;
}

std::string TextArea::removeLast(std::size_t count) {
	count = std::min(count, text_.size());
	if(count == 0) return "";
	std::string removed = detail::toUtf8(std::u32string_view(text_).substr(text_.size() - count));
	text_.resize(text_.size() - count);
	invalidate();
	onTextChanged(text());
	return removed;
}

std::u32string TextArea::shownText() const { return secret_ ? std::u32string(text_.size(), U'\u2022') : text_; }

void TextArea::keyEvent(const KeyEvent& event) {
	if(secret_ && event.key == Key::C) return; // a secret is never copied
	Label::keyEvent(event);                    // first: a binding that runs for the key keeps it from the text
	if(event.action == Action::Release || (window() && window()->keyTaken())) return;

	if(event.key == Key::Backspace && !text_.empty()) {
		text_.pop_back();
		invalidate();
		onTextChanged(text());
	}

	if(isPaste(event) && window() && insert(pastedLine(window()->clipboardText()))) onTextChanged(text());
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
	Panel::paint(painter); // NOLINT(bugprone-parent-virtual-call): it draws its own lines, not Label's text

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
		const float caretX = std::round(std::min(content.left + f->measure(shownText()) + 1.0f, content.right - 2.0f));
		painter.fillRect({caretX, baseline - f->ascent(), caretX + 2.0f, baseline - f->ascent() + f->lineHeight()},
		                 textColor());
	}
}

} // namespace affresco
