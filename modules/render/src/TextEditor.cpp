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

constexpr std::size_t maxUndo = 500;       ///< Undo steps kept.
/// Bytes the undo steps keep in all: each copies the whole text, so a large text keeps fewer than maxUndo.
constexpr std::size_t maxUndoBytes = std::size_t{256} << 20;
constexpr float textPadding = 6.0f;        ///< Space above the first line, in pixels.
constexpr float scrollSpeed = 24.0f;       ///< Pixels per wheel step.
constexpr float scrollBarWidth = 4.0f;     ///< Thickness of the scroll bar, in pixels.
constexpr double doubleClickSeconds = 0.4; ///< Longest pause between the presses of a double click.

/// @brief What a text keeps in memory, roughly: its characters and a string for each line.
/// @param lines The text.
/// @return Bytes.
std::size_t bytesOf(const std::vector<std::u32string>& lines) noexcept {
	std::size_t bytes = lines.size() * sizeof(std::u32string);
	for(const std::u32string& line : lines) bytes += line.size() * sizeof(char32_t);
	return bytes;
}

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

/// @brief Turns UTF-8 text into lines, dropping '\r' and turning tabs into spaces.
/// @param utf8 The text.
/// @return At least one line.
std::vector<std::u32string> splitLines(std::string_view utf8) {
	std::vector<std::u32string> lines(1);
	for(char32_t c : detail::fromUtf8(utf8)) {
		if(c == U'\n') lines.emplace_back();
		else if(c == U'\t') lines.back().append(TextEditor::tabSize, U' ');
		else if(c != U'\r') lines.back().push_back(c);
	}
	return lines;
}

/// @brief Joins lines into UTF-8 text with "\n" between them.
/// @param lines The lines.
std::string joinLines(const std::vector<std::u32string>& lines) {
	std::string out;
	for(std::size_t i = 0; i < lines.size(); ++i) {
		if(i > 0) out.push_back('\n');
		out += detail::toUtf8(lines[i]);
	}
	return out;
}

} // namespace

TextEditor::TextEditor() {
	setColor(Color::hex(0x1b1d23));
	setCursor(Cursor::IBeam);
	resetColors();
}

/* Properties */

void TextEditor::setText(std::string_view utf8) {
	if(utf8 == utf8_) return;
	remember();
	redo_.clear();
	lastEdit_ = Edit::Other;

	lines_ = splitLines(utf8);
	utf8_ = joinLines(lines_);
	auto clamp = [this](Position p) {
		p.line = std::min(p.line, lines_.size() - 1);
		p.column = std::min(p.column, lines_[p.line].size());
		return p;
	};
	caret_ = clamp(caret_);
	anchor_ = clamp(anchor_);
	invalidate();
}

void TextEditor::setFont(std::shared_ptr<const Font> font) {
	font_ = std::move(font);
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
	invalidate();
}

void TextEditor::setLineNumberColor(Color color) {
	lineNumberColor_ = color;
	invalidate();
}

void TextEditor::setActiveLineNumberColor(Color color) {
	activeLineNumberColor_ = color;
	invalidate();
}

void TextEditor::setLineColor(Color color) {
	lineColor_ = color;
	invalidate();
}

void TextEditor::setSelectionColor(Color color) {
	selectionColor_ = color;
	invalidate();
}

void TextEditor::setCaretColor(Color color) {
	caretColor_ = color;
	invalidate();
}

void TextEditor::resetColors() {
	textColor_ = Color::hex(0xd6d9e0);
	lineNumberColor_ = Color::hex(0x4b505c);
	activeLineNumberColor_ = Color::hex(0xc8ccd4);
	lineColor_ = Color::hex(0x23262e);
	selectionColor_ = Color::hex(0x264f78);
	caretColor_ = Color::hex(0xaeafad);
	invalidate();
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

float TextEditor::columnX(std::size_t line, std::size_t column) const noexcept {
	const Font* f = font();
	if(!f) return 0.0f;
	const std::u32string& text = lines_[line];
	// The shaped line's carets: inside a ligature they are spread over it, so the caret moves by code point
	const std::vector<float>& carets = f->shape(text).carets;
	return carets.empty() ? 0.0f : carets[std::min(column, carets.size() - 1)];
}

std::size_t TextEditor::columnAt(std::size_t line, float x) const noexcept {
	const Font* f = font();
	if(!f) return 0;
	// The nearest caret: in a right-to-left run they go from right to left
	const std::vector<float>& carets = f->shape(lines_[line]).carets;
	std::size_t nearest = 0;
	for(std::size_t i = 1; i < carets.size(); ++i)
		if(std::abs(carets[i] - x) < std::abs(carets[nearest] - x)) nearest = i;
	return nearest;
}

TextEditor::Position TextEditor::positionAt(Vec2 point) const noexcept {
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

void TextEditor::clampScroll() noexcept {
	const Font* f = font();
	const float contentHeight = static_cast<float>(lines_.size()) * lineHeight() + textPadding;
	// Like most code editors, the last line can scroll up to the top
	const float maxY = std::max(contentHeight - lineHeight() - textPadding, 0.0f);
	float widest = 0.0f;
	if(f)
		for(const std::u32string& line : lines_) widest = std::max(widest, f->measure(line));
	const float textWidth = frame().width() - gutterWidth() - 16.0f;
	const float maxX = std::max(widest - textWidth, 0.0f);
	scroll_.x = std::clamp(scroll_.x, 0.0f, maxX);
	scroll_.y = std::clamp(scroll_.y, 0.0f, maxY);
}

void TextEditor::revealCaret() noexcept {
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
	if(from.line == to.line) return lines_[from.line].substr(from.column, to.column - from.column);
	std::u32string out = lines_[from.line].substr(from.column);
	for(std::size_t line = from.line + 1; line < to.line; ++line) {
		out.push_back(U'\n');
		out += lines_[line];
	}
	out.push_back(U'\n');
	out += lines_[to.line].substr(0, to.column);
	return out;
}

void TextEditor::erase(Position from, Position to) {
	if(from.line == to.line) {
		lines_[from.line].erase(from.column, to.column - from.column);
		return;
	}
	lines_[from.line] = lines_[from.line].substr(0, from.column) + lines_[to.line].substr(to.column);
	lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(from.line) + 1,
	             lines_.begin() + static_cast<std::ptrdiff_t>(to.line) + 1);
}

TextEditor::Position TextEditor::insert(Position at, std::u32string_view text) {
	const std::u32string tail = lines_[at.line].substr(at.column);
	lines_[at.line].erase(at.column);
	Position end = at;
	for(char32_t c : text) {
		if(c == U'\n') {
			lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(end.line) + 1, std::u32string());
			++end.line;
			end.column = 0;
		} else if(c == U'\t') {
			lines_[end.line].append(tabSize, U' ');
			end.column += tabSize;
		} else if(c != U'\r') {
			lines_[end.line].push_back(c);
			++end.column;
		}
	}
	lines_[end.line] += tail;
	return end;
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
	invalidate();
}

TextEditor::Position TextEditor::step(Position from, bool forward, bool word) const noexcept {
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

std::size_t TextEditor::indentOf(std::size_t line) const noexcept {
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
	if(kind == Edit::Other || kind != lastEdit_) remember();
	redo_.clear();
	lastEdit_ = kind;
}

void TextEditor::remember() {
	undo_.push_back({lines_, caret_, anchor_, bytesOf(lines_)});

	// The oldest go first; the newest step stays, however large
	std::size_t kept = 0;
	for(const Snapshot& snapshot : undo_) kept += snapshot.bytes;
	std::size_t dropped = 0;
	while(undo_.size() - dropped > 1 && (undo_.size() - dropped > maxUndo || kept > maxUndoBytes))
		kept -= undo_[dropped++].bytes;
	undo_.erase(undo_.begin(), undo_.begin() + static_cast<std::ptrdiff_t>(dropped));
}

void TextEditor::endEdit() {
	utf8_ = joinLines(lines_);
	preferredX_ = -1.0f;
	revealCaret();
	onTextChanged(utf8_);
	onCaretMoved(caret_);
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
	std::vector<Snapshot>& from = redo ? redo_ : undo_;
	std::vector<Snapshot>& to = redo ? undo_ : redo_;
	if(from.empty()) return;
	to.push_back({lines_, caret_, anchor_, bytesOf(lines_)});
	Snapshot snapshot = std::move(from.back());
	from.pop_back();
	lines_ = std::move(snapshot.lines);
	caret_ = snapshot.caret;
	anchor_ = snapshot.anchor;
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
		const long change =
		    outdent ? -static_cast<long>(std::min(indentOf(line), tabSize)) : static_cast<long>(tabSize);
		if(outdent) lines_[line].erase(0, static_cast<std::size_t>(-change));
		else if(!lines_[line].empty()) lines_[line].insert(0, tabSize, U' ');
		else continue;
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
		invalidate();
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
		invalidate();
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
	if(scroll_.x != before.x || scroll_.y != before.y) invalidate();
}

void TextEditor::focusEvent(bool focused) {
	if(!focused) dragging_ = false;
	invalidate(); // the caret appears or disappears
	Panel::focusEvent(focused);
}

void TextEditor::arrange() { clampScroll(); }

/* Drawing */

void TextEditor::colorLine(std::u32string_view, int&, std::vector<Color>& colors) const {
	std::ranges::fill(colors, textColor_);
}

void TextEditor::paint(Painter& painter) const {
	Panel::paint(painter);
	const Font* f = font();
	if(!f) return;

	const Rect& box = frame();
	const float lh = lineHeight();
	const auto first = static_cast<std::size_t>(std::max(std::floor(scroll_.y / lh) - 1.0f, 0.0f));
	const Layout layout{.lineHeight = lh,
	                    .gutter = gutterWidth(),
	                    .textLeft = std::round(box.left + gutterWidth() - scroll_.x),
	                    .firstTop = box.top + textPadding - scroll_.y,
	                    .baseline = std::round((lh - f->lineHeight()) / 2.0f + f->ascent()),
	                    .first = first,
	                    .last = std::min(lines_.size(), first + visibleLines() + 3)};

	// The colouring state (e.g. an open block comment) may come from above the visible lines
	int state = 0;
	std::vector<Color> colors;
	std::vector<std::pair<std::size_t, float>> runs;
	for(std::size_t line = 0; line < layout.last; ++line) {
		colors.resize(lines_[line].size());
		colorLine(lines_[line], state, colors);
		if(line >= layout.first) paintLine(painter, *f, layout, line, colors, runs);
	}

	paintGutter(painter, *f, layout);
	paintCaretAndScrollBar(painter, layout);
}

float TextEditor::lineTop(const Layout& layout, std::size_t line) noexcept {
	return std::round(layout.firstTop + static_cast<float>(line) * layout.lineHeight);
}

void TextEditor::paintLine(Painter& painter, const Font& font, const Layout& layout, std::size_t line,
                           const std::vector<Color>& colors, std::vector<std::pair<std::size_t, float>>& runs) const {
	const Rect& box = frame();
	const float top = lineTop(layout, line), lh = layout.lineHeight;
	const std::u32string& text = lines_[line];

	if(line == caret_.line && !hasSelection()) painter.fillRect({box.left, top, box.right, top + lh}, lineColor_);
	paintSelection(painter, font, layout, line);

	// Runs of one colour, each shaped on its own, where the whole line's shaping puts them (so the caret matches).
	// Their widths are read first: drawing a run shapes it, which may drop the line's shaping from the cache
	const std::vector<float>& advances = font.shape(text).advances;
	if(advances.size() < text.size()) return; // out of memory: Font::shape() gave nothing
	runs.clear();
	for(std::size_t start = 0; start < text.size();) {
		std::size_t end = start + 1;
		while(end < text.size() && colors[end] == colors[start]) ++end;
		float width = 0.0f;
		for(std::size_t i = start; i < end; ++i) width += advances[i];
		runs.emplace_back(end, width);
		start = end;
	}
	float pen = layout.textLeft;
	std::size_t start = 0;
	for(const auto& [end, width] : runs) {
		if(pen + width >= box.left + layout.gutter && pen <= box.right)
			painter.drawText(font, std::u32string_view(text).substr(start, end - start), {pen, top + layout.baseline},
			                 colors[start]);
		pen += width;
		start = end;
	}
}

void TextEditor::paintSelection(Painter& painter, const Font& font, const Layout& layout, std::size_t line) const {
	const auto [from, to] = selection();
	if(!hasSelection() || line < from.line || line > to.line) return;

	const float top = lineTop(layout, line);
	const float left = line == from.line ? columnX(line, from.column) : 0.0f;
	// A selected line break shows as a space's width
	const float right =
	    line == to.line ? columnX(line, to.column) : font.measure(lines_[line]) + font.glyph(U' ').advance;
	painter.fillRect({layout.textLeft + std::min(left, right), top, layout.textLeft + std::max(left, right),
	                  top + layout.lineHeight},
	                 selectionColor_);
}

void TextEditor::paintGutter(Painter& painter, const Font& font, const Layout& layout) const {
	// The gutter covers text scrolled under it
	const Rect& box = frame();
	if(color().a > 0.0f) painter.fillRect({box.left, box.top, box.left + layout.gutter - 8.0f, box.bottom}, color());
	if(!lineNumbers_) return;
	for(std::size_t line = layout.first; line < layout.last; ++line) {
		const std::u32string number = detail::fromUtf8(std::to_string(line + 1));
		const float x = std::round(box.left + layout.gutter - 20.0f - font.measure(number));
		painter.drawText(font, number, {x, lineTop(layout, line) + layout.baseline},
		                 line == caret_.line ? activeLineNumberColor_ : lineNumberColor_);
	}
}

void TextEditor::paintCaretAndScrollBar(Painter& painter, const Layout& layout) const {
	const Rect& box = frame();
	const float lh = layout.lineHeight;
	if(hasFocus()) {
		const float x = std::round(layout.textLeft + columnX(caret_.line, caret_.column));
		const float top = lineTop(layout, caret_.line);
		if(x >= box.left + layout.gutter - 8.0f)
			painter.fillRect({x, top + 1.0f, x + 2.0f, top + lh - 1.0f}, caretColor_);
	}

	// A scroll bar thumb as long as the visible share of the text
	const float content = static_cast<float>(lines_.size()) * lh + box.height() - lh;
	if(static_cast<float>(lines_.size()) * lh + textPadding > box.height()) {
		const float length = std::max(box.height() * box.height() / content, 16.0f);
		const float top = box.top + scroll_.y / (content - box.height()) * (box.height() - length);
		painter.fillRect({box.right - scrollBarWidth - 2.0f, top, box.right - 2.0f, top + length},
		                 lineNumberColor_.withAlpha(0.9f));
	}
}

} // namespace affresco
