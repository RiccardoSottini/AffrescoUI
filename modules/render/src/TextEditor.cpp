/// @file
/// @brief TextEditor: properties, geometry, editing on the text model, undo and input events (drawing is in
/// TextEditorPaint.cpp; rows, cursors and find in TextEditorView.cpp, TextEditorCursors.cpp and TextEditorFind.cpp).

#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "TextEditorRows.hpp"
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
	modelConnection_ = model_->onChanged.connect([this](const TextChange& change) { textChanged(change); });
}

TextEditor::~TextEditor() { model_->onChanged.disconnect(modelConnection_); }

void TextEditor::setModel(std::shared_ptr<TextModel> model) {
	if(!model) model = std::make_shared<TextModel>();
	if(model == model_) return;
	model_->onChanged.disconnect(modelConnection_);
	model_ = std::move(model);
	lines_ = TextLines(*model_);
	modelConnection_ = model_->onChanged.connect([this](const TextChange& change) { textChanged(change); });
	caret_ = anchor_ = {};
	cursors_.clear();
	folds_.clear();
	lastEdit_ = Edit::Other;
	states_.assign(1, 0);
	if(highlighter_) highlighter_->reset(*model_);
	widest_ = -1.0f;
	scroll_ = {};
	rebuildRows();
	invalidate();
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
	if(model_->equals(utf8)) return;
	editing_ = true;
	model_->beginStep(false, caret_, anchor_);
	model_->assign(utf8);
	model_->endStep();
	editing_ = false;
	lastEdit_ = Edit::Other;
	caret_ = clamped(caret_);
	anchor_ = clamped(anchor_);
	cursors_.clear();
	invalidate();
}

TextEditor::Position TextEditor::clamped(Position at) const noexcept {
	at.line = std::min(at.line, model_->lineCount() - 1);
	at.column = std::min(at.column, model_->lineLength(at.line));
	return at;
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

void TextEditor::setLineLabels(std::vector<std::size_t> numbers) {
	if(numbers == lineLabels_) return;
	largestLabel_ = numbers.empty() ? 0 : std::ranges::max(numbers);
	lineLabels_ = std::move(numbers);
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
	guideColor_ = Color::hex(0x3b3f4a);
	matchColor_ = Color::hex(0x613214);
	bracketColor_ = Color::hex(0x3e4452);
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
	const std::size_t digits = std::max<std::size_t>(std::to_string(std::max(lines_.size(), largestLabel_)).size(), 3);
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

void TextEditor::textChanged(const TextChange& change) {
	if(states_.size() > change.from.line + 1) states_.resize(change.from.line + 1);
	if(highlighter_) highlighter_->changed(*model_, change);
	// Another editor's edit of a shared model (or an outside one) moves the text under this caret
	if(!editing_) {
		caret_ = shifted(caret_, change);
		anchor_ = shifted(anchor_, change);
	}
	for(std::size_t i = 0; i < cursors_.size(); ++i) {
		cursors_[i].caret = shifted(cursors_[i].caret, change);
		cursors_[i].anchor = shifted(cursors_[i].anchor, change);
	}
	if(editedCursors_)
		for(std::size_t i = 0; i < editedCursors_->size(); ++i) {
			if(i == activeCursor_) continue;
			EditorSelection& other = (*editedCursors_)[i];
			other = {shifted(other.anchor, change), shifted(other.caret, change)};
		}
	shiftView(change);
	rowsChanged(change);
	invalidate();
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

/* Text */

std::u32string TextEditor::textIn(Position from, Position to) const {
	return detail::fromUtf8(model_->textIn(from, to));
}

void TextEditor::erase(Position from, Position to) { model_->erase(from, to); }

TextEditor::Position TextEditor::insert(Position at, std::u32string_view text) {
	std::string utf8;
	utf8.reserve(text.size());
	for(const char32_t c : text) {
		if(c == U'\t') utf8.append(tabSize, ' ');
		else if(c != U'\r') detail::appendUtf8(utf8, c);
	}
	return model_->insert(at, utf8);
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
	editing_ = true;
	model_->beginStep(kind != Edit::Other && kind == lastEdit_, caret_, anchor_);
	lastEdit_ = kind;
}

void TextEditor::endEdit() {
	model_->endStep();
	editing_ = false;
	preferredX_ = -1.0f;
	revealCaret();
	if(!onTextChanged.empty()) onTextChanged(model_->text());
	onCaretMoved(caret_);
}

void TextEditor::recolor() {
	states_.clear();
	invalidate();
}

void TextEditor::setHighlighter(std::unique_ptr<EditorHighlighter> highlighter) {
	highlighter_ = std::move(highlighter);
	if(highlighter_) highlighter_->reset(*model_);
	recolor();
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
	if(!editable()) return;
	editing_ = true;
	const bool undone = model_->undo(redo, caret_, anchor_);
	editing_ = false;
	if(!undone) return;
	cursors_.clear(); // undo restores one caret
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
		if(outdent) model_->erase({line, 0}, {line, removed});
		else model_->insert({line, 0}, std::string(tabSize, ' '));
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
	if(cut && editable()) {
		beginEdit(Edit::Other);
		replaceSelection({});
		endEdit();
	}
}

void TextEditor::paste() {
	if(!window() || !editable()) return;
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
	if(viewKey(event) || cursorsKey(event) || caretKey(event) || editingKey(event)) return;
	if(event.mods.super() || event.mods.control()) commandKey(event); // Cmd on macOS, Ctrl elsewhere
}

bool TextEditor::caretKey(const KeyEvent& event) {
	const Mods m = event.mods;
	const bool command = m.super() || m.control();
	const bool shift = m.shift();
	const Position end{lines_.size() - 1, lines_.back().size()};
	switch(event.key) {
	case Key::Left:
	case Key::Right:
		horizontalKey(event.key == Key::Right, m);
		return true;
	case Key::Up:
	case Key::Down:
		verticalKey(event.key == Key::Down, m);
		return true;
	case Key::Home: {
		// First non-blank character, or the start of the line if already there
		const std::size_t indent = indentOf(caret_.line);
		moveCaret(command ? Position{0, 0} : Position{caret_.line, caret_.column == indent ? 0 : indent}, shift);
		return true;
	}
	case Key::End:
		moveCaret(command ? end : Position{caret_.line, lines_[caret_.line].size()}, shift);
		return true;
	case Key::PageUp:
	case Key::PageDown: {
		const auto rows = static_cast<long>(visibleLines());
		moveRows(event.key == Key::PageUp ? -rows : rows, shift);
		return true;
	}
	default:
		return false;
	}
}

void TextEditor::horizontalKey(bool right, Mods mods) {
	const bool word = mods.alt() || (mods.control() && !mods.super());
	const bool shift = mods.shift();
	if(mods.super()) return moveCaret({caret_.line, right ? lines_[caret_.line].size() : 0}, shift);
	if(hasSelection() && !shift) return moveCaret(right ? selection().second : selection().first, false);
	moveCaret(step(caret_, right, word), shift);
}

void TextEditor::verticalKey(bool down, Mods mods) {
	if(!mods.super()) return moveRows(down ? 1 : -1, mods.shift());
	moveCaret(down ? Position{lines_.size() - 1, lines_.back().size()} : Position{0, 0}, mods.shift());
}

bool TextEditor::editingKey(const KeyEvent& event) {
	const Mods m = event.mods;
	const bool command = m.super() || m.control();
	switch(event.key) {
	case Key::Enter:
	case Key::KeypadEnter:
		if(editable() && !command) newLine();
		return true;
	case Key::Tab:
		if(!editable() || command) return true;
		if(m.shift() || hasSelection()) {
			indentLines(m.shift());
			return true;
		}
		beginEdit(Edit::Typing);
		replaceSelection(std::u32string(tabSize - caret_.column % tabSize, U' '));
		endEdit();
		return true;
	case Key::Backspace:
	case Key::Delete:
		if(editable()) deleteKey(event.key == Key::Delete, m);
		return true;
	default:
		return false;
	}
}

void TextEditor::deleteKey(bool forward, Mods mods) {
	const bool word = mods.alt() || (mods.control() && !mods.super());
	if(hasSelection()) {
		beginEdit(Edit::Other);
		replaceSelection({});
		return endEdit();
	}
	if(forward) {
		const Position to = step(caret_, true, word);
		if(to == caret_) return;
		beginEdit(Edit::Deleting);
		erase(caret_, to);
		anchor_ = caret_;
		return endEdit();
	}
	if(caret_ == Position{}) return;
	beginEdit(Edit::Deleting);
	Position from = mods.super() ? Position{caret_.line, 0} : step(caret_, false, word);
	if(from == caret_) from = step(caret_, false, false); // Cmd+Backspace at a line start joins lines
	erase(from, caret_);
	caret_ = anchor_ = from;
	endEdit();
}

void TextEditor::commandKey(const KeyEvent& event) {
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
		return undo(event.mods.shift());
	case Key::Y:
		if(event.mods.control()) undo(true);
		return;
	default:
		return;
	}
}

void TextEditor::charEvent(const CharEvent& event) {
	Panel::charEvent(event);
	if(event.codepoint < 0x20 || !editable()) return;
	const std::u32string typed(1, event.codepoint);
	if(!cursors_.empty()) return forEachCursor(Edit::Typing, [&] { replaceSelection(typed); });
	beginEdit(Edit::Typing);
	replaceSelection(typed);
	endEdit();
}

void TextEditor::mouseButtonEvent(const MouseButtonEvent& event) {
	Panel::mouseButtonEvent(event);
	hoverAt(std::nullopt);
	if(event.button != MouseButton::Left) return;
	if(minimapPress(event.position, event.action != Action::Release)) return;
	if(event.action == Action::Release) {
		dragging_ = columnDragging_ = false;
		return;
	}

	const Position at = positionAt(event.position);
	// Alt+click adds a cursor, Alt+Shift+drag selects a column; a click on a folded line's gutter unfolds it
	if(event.mods.alt() && event.mods.shift()) {
		columnDragging_ = true;
		columnFrom_ = event.position;
		return selectColumn(columnFrom_, event.position);
	}
	if(event.mods.alt()) {
		addCursor({at, at});
		return;
	}
	if(event.position.x < frame().left + gutterWidth() - 8.0f && unfold(at.line)) return;
	if(event.position.x < frame().left + gutterWidth() && !onGutterClicked.empty())
		return onGutterClicked(at.line, event.mods);
	clearCursors();
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
	if(columnDragging_) return selectColumn(columnFrom_, event.position);
	if(minimapDrag(event.position)) return;
	if(!dragging_ && !onWordHovered.empty()) {
		const Position at = positionAt(event.position);
		const std::u32string& text = lines_[at.line];
		std::size_t start = at.column, end = at.column;
		while(start > 0 && isWord(text[start - 1])) --start;
		while(end < text.size() && isWord(text[end])) ++end;
		const bool onWord = start < end && event.position.x >= frame().left + gutterWidth();
		const Position word = onWord ? Position{at.line, start} : Position{SIZE_MAX, 0};
		if(word != hoveredWord_) onWordHovered(hoveredWord_ = word, onWord ? detail::toUtf8(text.substr(start, end - start)) : "");
	}
	if(!dragging_) return hoverAt(event.position);
	if(clicks_ != 1) return;
	const Position at = positionAt(event.position);
	if(at != caret_) moveCaret(at, true);
}

void TextEditor::scrollEvent(const ScrollEvent& event) {
	Panel::scrollEvent(event);
	const Vec2 before = scroll_;
	scroll_.x -= event.offset.x * scrollSpeed;
	scroll_.y -= event.offset.y * scrollSpeed;
	clampScroll();
	if(scroll_.x == before.x && scroll_.y == before.y) return;
	invalidatePaint();
	hoverAt(std::nullopt);
	onScrolled(scroll_);
}

void TextEditor::hoverEvent(bool hovered) {
	if(!hovered) hoverAt(std::nullopt);
	Panel::hoverEvent(hovered);
}

void TextEditor::focusEvent(bool focused) {
	if(!focused) dragging_ = columnDragging_ = minimapDragging_ = false;
	invalidatePaint(); // the caret appears or disappears
	Panel::focusEvent(focused);
}

void TextEditor::arrange() {
	// The wrapped rows follow the width (and the font)
	if(wrap_ && (wrapWidth() != rowsWidth_ || font() != rowsFont_)) rebuildRows();
	clampScroll();
}

} // namespace affresco
