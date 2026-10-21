/// @file
/// @brief TextEditor: properties, geometry, editing on the text model, undo and input events (drawing is in
/// TextEditorPaint.cpp).

#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>

namespace affresco {

namespace {

constexpr float scrollSpeed = 24.0f;       ///< Pixels per wheel step.
constexpr double doubleClickSeconds = 0.4; ///< Longest pause between the presses of a double click.

/// @brief The time now, in seconds, for double clicks.
double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

/// @brief Whether a code point is part of a word (letters, digits, underscore).
/// @param c The code point.
bool isWord(char32_t c) noexcept {
	return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_' ||
	       (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7);
}

/// @brief Whether a code point is a space or a tab.
/// @param c The code point.
bool isBlank(char32_t c) noexcept { return c == U' ' || c == U'\t'; }

} // namespace

TextEditor::TextEditor() {
	setColor(Color::hex(0x1b1d23));
	setCursor(Cursor::IBeam);
	resetColors();
	model_.onChanged.connect([this](const TextChange& change) { textChanged(change); }); // the model is a member
}

/* Properties */

void TextEditor::setText(std::string_view utf8) {
	// Tabs become spaces and "\r" goes; most texts have neither, and are not copied
	std::string normalised;
	if(utf8.find_first_of("\t\r") != std::string_view::npos) {
		normalised.reserve(utf8.size());
		for(const char c : utf8) {
			if(c == '\t') normalised.append(tabSize, ' ');
			else if(c != '\r') normalised.push_back(c);
		}
		utf8 = normalised;
	}
	if(model_.equals(utf8)) return;
	model_.beginStep(false, caret_, anchor_);
	model_.assign(utf8);
	model_.endStep();
	lastEdit_ = Edit::Other;

	auto clamp = [this](Position p) {
		p.line = std::min(p.line, model_.lineCount() - 1);
		p.column = std::min(p.column, model_.lineLength(p.line));
		return p;
	};
	caret_ = clamp(caret_);
	anchor_ = clamp(anchor_);
	invalidate();
}

void TextEditor::setFont(std::shared_ptr<const Font> font) {
	font_ = std::move(font);
	widest_ = -1.0f;
	invalidate();
}

const Font* TextEditor::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

void TextEditor::setLineNumbers(bool shown) {
	lineNumbers_ = shown;
	invalidate();
}

void TextEditor::setTextColor(Color color) {
	textColor_ = color;
	invalidatePaint();
}

void TextEditor::setLineNumberColor(Color color) {
	lineNumberColor_ = color;
	invalidatePaint();
}

void TextEditor::setActiveLineNumberColor(Color color) {
	activeLineNumberColor_ = color;
	invalidatePaint();
}

void TextEditor::setLineColor(Color color) {
	lineColor_ = color;
	invalidatePaint();
}

void TextEditor::setSelectionColor(Color color) {
	selectionColor_ = color;
	invalidatePaint();
}

void TextEditor::setCaretColor(Color color) {
	caretColor_ = color;
	invalidatePaint();
}

void TextEditor::resetColors() {
	textColor_ = Color::hex(0xd6d9e0);
	lineNumberColor_ = Color::hex(0x4b505c);
	activeLineNumberColor_ = Color::hex(0xc8ccd4);
	lineColor_ = Color::hex(0x23262e);
	selectionColor_ = Color::hex(0x264f78);
	caretColor_ = Color::hex(0xaeafad);
	invalidatePaint();
}

std::size_t TextEditor::selectedLength() const {
	const auto [from, to] = selection();
	return textIn(from, to).size();
}

/* Geometry */

float TextEditor::lineHeight() const noexcept {
	const Font* f = font();
	return f ? std::round(f->lineHeight() * 1.45f) : 20.0f;
}

float TextEditor::gutterWidth() const noexcept {
	const Font* f = font();
	if(!f) return 0.0f;
	if(!lineNumbers_) return 12.0f;
	const std::size_t digits = std::max<std::size_t>(std::to_string(lines_.size()).size(), 3);
	return std::round(16.0f + static_cast<float>(digits) * f->glyph(U'0').advance + 20.0f);
}

float TextEditor::columnX(std::size_t line, std::size_t column) const {
	const Font* f = font();
	if(!f) return 0.0f;
	const std::u32string& text = lines_[line];
	// The shaped line's carets: inside a ligature they are spread over it, so the caret moves by code point
	const std::vector<float>& carets = f->shape(text).carets;
	return carets.empty() ? 0.0f : carets[std::min(column, carets.size() - 1)];
}

std::size_t TextEditor::columnAt(std::size_t line, float x) const {
	const Font* f = font();
	if(!f) return 0;
	// The nearest caret: in a right-to-left run they go from right to left
	const std::vector<float>& carets = f->shape(lines_[line]).carets;
	std::size_t nearest = 0;
	for(std::size_t i = 1; i < carets.size(); ++i)
		if(std::abs(carets[i] - x) < std::abs(carets[nearest] - x)) nearest = i;
	return nearest;
}

TextEditor::Position TextEditor::positionAt(Vec2 point) const {
	const float y = point.y - frame().top - textPadding + scroll_.y;
	const float row = std::floor(y / lineHeight());
	Position p;
	if(row < 0.0f) return p; // above the text: the start
	// Below (or not a number, from a huge frame): the end, before the row is converted to an integer
	if(!(row < static_cast<float>(lines_.size()))) return {lines_.size() - 1, lines_.back().size()};
	p.line = static_cast<std::size_t>(row);
	p.column = columnAt(p.line, point.x - frame().left - gutterWidth() + scroll_.x);
	return p;
}

std::size_t TextEditor::visibleLines() const noexcept {
	// No more than the text has: a huge height (or not a number) must not reach the conversion to an integer
	const float rows =
	    std::min(std::floor((frame().height() - textPadding) / lineHeight()), static_cast<float>(lines_.size()));
	return rows >= 1.0f ? static_cast<std::size_t>(rows) : 1;
}

void TextEditor::clampScroll() {
	const Font* f = font();
	const float contentHeight = static_cast<float>(lines_.size()) * lineHeight() + textPadding;
	// Like most code editors, the last line can scroll up to the top
	const float maxY = std::max(contentHeight - lineHeight() - textPadding, 0.0f);
	const float widest = f ? widestLine(*f) : 0.0f;
	const float textWidth = frame().width() - gutterWidth() - 16.0f;
	const float maxX = std::max(widest - textWidth, 0.0f);
	scroll_.x = std::clamp(scroll_.x, 0.0f, maxX);
	scroll_.y = std::clamp(scroll_.y, 0.0f, maxY);
}

float TextEditor::widestLine(const Font& font) const {
	if(widest_ >= 0.0f && widestFont_ == &font) return widest_;
	widestFont_ = &font;
	widest_ = 0.0f;
	if(lines_.size() <= exactWidth) {
		for(std::size_t line = 0; line < lines_.size(); ++line) widest_ = std::max(widest_, font.measure(lines_[line]));
		return widest_;
	}
	// Measuring every line of a large text would cost more than opening it: a byte is at most one code point
	const float advance = std::max(font.glyph(U'M').advance, font.glyph(U'0').advance);
	widest_ = static_cast<float>(model_.longestLine()) * advance;
	return widest_;
}

void TextEditor::textChanged(const TextChange& change) {
	if(states_.size() > change.from.line + 1) states_.resize(change.from.line + 1);
	const std::size_t edited = change.newEnd.line - change.from.line + 1;
	if(lines_.size() <= exactWidth || edited > exactWidth) {
		widest_ = -1.0f;
		return;
	}
	// A large text keeps its estimate, grown by the lines edited (a shorter line leaves it wider than needed)
	if(widest_ < 0.0f || !widestFont_) return;
	for(std::size_t line = change.from.line; line <= change.newEnd.line; ++line)
		widest_ = std::max(widest_, widestFont_->measure(lines_[line]));
}

void TextEditor::revealCaret() {
	const float lh = lineHeight();
	const float top = static_cast<float>(caret_.line) * lh;
	const float height = frame().height() - textPadding;
	if(top < scroll_.y) scroll_.y = top;
	else if(top + lh > scroll_.y + height) scroll_.y = top + lh - height;

	const float x = columnX(caret_.line, caret_.column);
	const float width = frame().width() - gutterWidth() - 16.0f;
	if(x < scroll_.x) scroll_.x = std::max(x - 40.0f, 0.0f);
	else if(x > scroll_.x + width) scroll_.x = x - width + 40.0f;
	invalidate();
}

/* Text */

std::u32string TextEditor::textIn(Position from, Position to) const {
	return detail::fromUtf8(model_.textIn(from, to));
}

void TextEditor::erase(Position from, Position to) { model_.erase(from, to); }

TextEditor::Position TextEditor::insert(Position at, std::u32string_view text) {
	std::string utf8;
	utf8.reserve(text.size());
	for(const char32_t c : text) {
		if(c == U'\t') utf8.append(tabSize, ' ');
		else if(c != U'\r') detail::appendUtf8(utf8, c);
	}
	return model_.insert(at, utf8);
}

std::pair<TextEditor::Position, TextEditor::Position> TextEditor::selection() const noexcept {
	return caret_ < anchor_ ? std::pair{caret_, anchor_} : std::pair{anchor_, caret_};
}

void TextEditor::replaceSelection(std::u32string_view text) {
	const auto [from, to] = selection();
	erase(from, to);
	caret_ = anchor_ = insert(from, text);
}

void TextEditor::setSelection(Position anchor, Position caret) noexcept {
	anchor_ = anchor;
	caret_ = caret;
	invalidatePaint();
}

TextEditor::Position TextEditor::step(Position from, bool forward, bool word) const {
	Position p = from;
	if(!forward) {
		if(p.column == 0) return p.line == 0 ? p : Position{p.line - 1, lines_[p.line - 1].size()};
		const std::u32string& text = lines_[p.line];
		if(!word) return {p.line, p.column - 1};
		while(p.column > 0 && isBlank(text[p.column - 1])) --p.column;
		if(p.column > 0 && isWord(text[p.column - 1]))
			while(p.column > 0 && isWord(text[p.column - 1])) --p.column;
		else if(p.column > 0) --p.column;
		return p;
	}
	const std::u32string& text = lines_[p.line];
	if(p.column == text.size()) return p.line + 1 == lines_.size() ? p : Position{p.line + 1, 0};
	if(!word) return {p.line, p.column + 1};
	while(p.column < text.size() && isBlank(text[p.column])) ++p.column;
	if(p.column < text.size() && isWord(text[p.column]))
		while(p.column < text.size() && isWord(text[p.column])) ++p.column;
	else if(p.column < text.size()) ++p.column;
	return p;
}

std::size_t TextEditor::indentOf(std::size_t line) const {
	const std::u32string& text = lines_[line];
	std::size_t n = 0;
	while(n < text.size() && text[n] == U' ') ++n;
	return n;
}

std::pair<std::size_t, std::size_t> TextEditor::selectedLines() const noexcept {
	auto [from, to] = selection();
	// A selection ending at the start of a line doesn't include that line
	if(to.line > from.line && to.column == 0) --to.line;
	return {from.line, to.line};
}

/* Editing */

void TextEditor::beginEdit(Edit kind) {
	model_.beginStep(kind != Edit::Other && kind == lastEdit_, caret_, anchor_);
	lastEdit_ = kind;
}

void TextEditor::endEdit() {
	model_.endStep();
	preferredX_ = -1.0f;
	revealCaret();
	if(!onTextChanged.empty()) onTextChanged(model_.text());
	onCaretMoved(caret_);
}

void TextEditor::recolor() {
	states_.clear();
	invalidate();
}

void TextEditor::moveCaret(Position to, bool extend, bool keepX) {
	if(!keepX) preferredX_ = -1.0f;
	const Position oldCaret = caret_, oldAnchor = anchor_;
	caret_ = to;
	if(!extend) anchor_ = to;
	lastEdit_ = Edit::Other; // typing after a move starts a new undo step
	revealCaret();
	if(caret_ != oldCaret || anchor_ != oldAnchor) onCaretMoved(caret_);
}

void TextEditor::undo(bool redo) {
	if(!model_.undo(redo, caret_, anchor_)) return;
	lastEdit_ = Edit::Other;
	endEdit();
}

void TextEditor::newLine() {
	beginEdit(Edit::Other);
	const Position from = selection().first;
	const std::size_t indent = std::min(indentOf(from.line), from.column);
	replaceSelection(U"\n" + std::u32string(indent, U' '));
	endEdit();
}

void TextEditor::indentLines(bool outdent) {
	const auto [first, last] = selectedLines();
	beginEdit(Edit::Other);
	for(std::size_t line = first; line <= last; ++line) {
		const std::size_t removed = outdent ? std::min(indentOf(line), tabSize) : 0;
		if(outdent && removed == 0) continue;
		if(!outdent && lines_[line].empty()) continue;
		if(outdent) model_.erase({line, 0}, {line, removed});
		else model_.insert({line, 0}, std::string(tabSize, ' '));
		const long change = outdent ? -static_cast<long>(removed) : static_cast<long>(tabSize);
		for(Position* p : {&caret_, &anchor_})
			if(p->line == line)
				p->column = static_cast<std::size_t>(std::max(0L, static_cast<long>(p->column) + change));
	}
	endEdit();
}

void TextEditor::copy(bool cut) {
	if(!window() || !hasSelection()) return;
	const auto [from, to] = selection();
	window()->setClipboardText(detail::toUtf8(textIn(from, to)));
	if(cut) {
		beginEdit(Edit::Other);
		replaceSelection({});
		endEdit();
	}
}

void TextEditor::paste() {
	if(!window()) return;
	const std::u32string text = detail::fromUtf8(window()->clipboardText());
	if(text.empty()) return;
	beginEdit(Edit::Other);
	replaceSelection(text);
	endEdit();
}

void TextEditor::selectWord(Position at) {
	const std::u32string& text = lines_[at.line];
	const char32_t ref = at.column < text.size() ? text[at.column] : (at.column > 0 ? text[at.column - 1] : U' ');
	auto same = [ref](char32_t c) {
		if(isWord(ref)) return isWord(c);
		if(isBlank(ref)) return isBlank(c);
		return !isWord(c) && !isBlank(c);
	};
	std::size_t start = at.column, end = at.column;
	while(start > 0 && same(text[start - 1])) --start;
	while(end < text.size() && same(text[end])) ++end;
	anchor_ = {at.line, start};
	caret_ = {at.line, end};
	revealCaret();
	onCaretMoved(caret_);
}

/* Events */

void TextEditor::keyEvent(const KeyEvent& event) {
	Panel::keyEvent(event);
	if(event.action == Action::Release) return;

	const Mods m = event.mods;
	const bool command = m.super() || m.control(); // Cmd on macOS, Ctrl elsewhere
	const bool word = m.alt() || (m.control() && !m.super());
	const bool shift = m.shift();

	auto vertical = [&](long lines) {
		if(preferredX_ < 0.0f) preferredX_ = columnX(caret_.line, caret_.column);
		const long target = static_cast<long>(caret_.line) + lines;
		if(target < 0) return moveCaret({0, 0}, shift, true);
		if(target >= static_cast<long>(lines_.size()))
			return moveCaret({lines_.size() - 1, lines_.back().size()}, shift, true);
		const auto line = static_cast<std::size_t>(target);
		moveCaret({line, columnAt(line, preferredX_)}, shift, true);
	};

	switch(event.key) {
	case Key::Left:
		if(m.super()) return moveCaret({caret_.line, 0}, shift);
		if(hasSelection() && !shift) return moveCaret(selection().first, false);
		return moveCaret(step(caret_, false, word), shift);
	case Key::Right:
		if(m.super()) return moveCaret({caret_.line, lines_[caret_.line].size()}, shift);
		if(hasSelection() && !shift) return moveCaret(selection().second, false);
		return moveCaret(step(caret_, true, word), shift);
	case Key::Up:
		if(m.super()) return moveCaret({0, 0}, shift);
		return vertical(-1);
	case Key::Down:
		if(m.super()) return moveCaret({lines_.size() - 1, lines_.back().size()}, shift);
		return vertical(1);
	case Key::Home: {
		// First non-blank character, or the start of the line if already there
		const std::size_t indent = indentOf(caret_.line);
		if(command) return moveCaret({0, 0}, shift);
		return moveCaret({caret_.line, caret_.column == indent ? 0 : indent}, shift);
	}
	case Key::End:
		if(command) return moveCaret({lines_.size() - 1, lines_.back().size()}, shift);
		return moveCaret({caret_.line, lines_[caret_.line].size()}, shift);
	case Key::PageUp:
		return vertical(-static_cast<long>(visibleLines()));
	case Key::PageDown:
		return vertical(static_cast<long>(visibleLines()));
	case Key::Enter:
	case Key::KeypadEnter:
		if(!command) newLine();
		return;
	case Key::Tab:
		if(command) return;
		if(shift || hasSelection()) return indentLines(shift);
		beginEdit(Edit::Typing);
		replaceSelection(std::u32string(tabSize - caret_.column % tabSize, U' '));
		return endEdit();
	case Key::Backspace: {
		if(hasSelection()) {
			beginEdit(Edit::Other);
			replaceSelection({});
			return endEdit();
		}
		if(caret_ == Position{}) return;
		beginEdit(Edit::Deleting);
		Position from = m.super() ? Position{caret_.line, 0} : step(caret_, false, word);
		if(from == caret_) from = step(caret_, false, false); // Cmd+Backspace at a line start joins lines
		erase(from, caret_);
		caret_ = anchor_ = from;
		return endEdit();
	}
	case Key::Delete: {
		if(hasSelection()) {
			beginEdit(Edit::Other);
			replaceSelection({});
			return endEdit();
		}
		const Position to = step(caret_, true, word);
		if(to == caret_) return;
		beginEdit(Edit::Deleting);
		erase(caret_, to);
		anchor_ = caret_;
		return endEdit();
	}
	default:
		break;
	}

	if(!command) return;
	switch(event.key) {
	case Key::A:
		anchor_ = {0, 0};
		caret_ = {lines_.size() - 1, lines_.back().size()};
		invalidatePaint();
		onCaretMoved(caret_);
		return;
	case Key::C:
		return copy(false);
	case Key::X:
		return copy(true);
	case Key::V:
		return paste();
	case Key::Z:
		return undo(shift);
	case Key::Y:
		if(m.control()) undo(true);
		return;
	default:
		return;
	}
}

void TextEditor::charEvent(const CharEvent& event) {
	Panel::charEvent(event);
	if(event.codepoint < 0x20) return;
	beginEdit(Edit::Typing);
	replaceSelection(std::u32string(1, event.codepoint));
	endEdit();
}

void TextEditor::mouseButtonEvent(const MouseButtonEvent& event) {
	Panel::mouseButtonEvent(event);
	if(event.button != MouseButton::Left) return;
	if(event.action == Action::Release) {
		dragging_ = false;
		return;
	}

	const Position at = positionAt(event.position);
	const double time = now();
	clicks_ = time - lastClickTime_ < doubleClickSeconds && at.line == lastClickAt_.line ? clicks_ % 3 + 1 : 1;
	lastClickTime_ = time;
	lastClickAt_ = at;
	dragging_ = true;

	if(clicks_ == 2) return selectWord(at);
	if(clicks_ == 3) {
		anchor_ = {at.line, 0};
		caret_ = at.line + 1 < lines_.size() ? Position{at.line + 1, 0} : Position{at.line, lines_[at.line].size()};
		invalidatePaint();
		onCaretMoved(caret_);
		return;
	}
	moveCaret(at, event.mods.shift());
}

void TextEditor::mouseMoveEvent(const MouseMoveEvent& event) {
	Panel::mouseMoveEvent(event);
	if(!dragging_ || clicks_ != 1) return;
	const Position at = positionAt(event.position);
	if(at != caret_) moveCaret(at, true);
}

void TextEditor::scrollEvent(const ScrollEvent& event) {
	Panel::scrollEvent(event);
	const Vec2 before = scroll_;
	scroll_.x -= event.offset.x * scrollSpeed;
	scroll_.y -= event.offset.y * scrollSpeed;
	clampScroll();
	if(scroll_.x != before.x || scroll_.y != before.y) invalidatePaint();
}

void TextEditor::focusEvent(bool focused) {
	if(!focused) dragging_ = false;
	invalidatePaint(); // the caret appears or disappears
	Panel::focusEvent(focused);
}

void TextEditor::arrange() { clampScroll(); }

} // namespace affresco
