#include "affresco/render/CodeEditor.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

namespace affresco {

namespace {

constexpr std::size_t tabSize = 4;          ///< Spaces per indentation level.
constexpr std::size_t maxUndo = 500;        ///< Undo steps kept.
constexpr float textPadding = 6.0f;         ///< Space above the first line, in pixels.
constexpr float scrollSpeed = 24.0f;        ///< Pixels per wheel step.
constexpr float scrollBarWidth = 4.0f;      ///< Thickness of the scroll bar, in pixels.
constexpr double doubleClickSeconds = 0.4;  ///< Longest pause between the presses of a double click.

/// @brief The time now, in seconds, for double clicks.
double now() {
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/// @brief Whether a code point is part of a word (letters, digits, underscore).
/// @param c The code point.
bool isWord(char32_t c) noexcept {
	return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') || c == U'_' ||
	       (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7);
}

/// @brief Whether a code point is a space or a tab.
/// @param c The code point.
bool isBlank(char32_t c) noexcept { return c == U' ' || c == U'\t'; }

/// @brief The closing partner of an opening bracket or quote.
/// @param c The code point.
/// @return The partner, or 0 if @p c doesn't open a pair.
char32_t closerOf(char32_t c) noexcept {
	switch(c) {
	case U'(': return U')';
	case U'[': return U']';
	case U'{': return U'}';
	case U'"': return U'"';
	default: return 0;
	}
}

/// @brief Turns UTF-8 text into lines, dropping '\r' and turning tabs into spaces.
/// @param utf8 The text.
/// @return At least one line.
std::vector<std::u32string> splitLines(std::string_view utf8) {
	std::vector<std::u32string> lines(1);
	for(char32_t c : detail::fromUtf8(utf8)) {
		if(c == U'\n') lines.emplace_back();
		else if(c == U'\t') lines.back().append(tabSize, U' ');
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

/// @brief Whether a word is in a sorted list.
/// @param list The list, sorted.
/// @param word The word.
template <std::size_t N> bool contains(const std::array<std::u32string_view, N>& list, std::u32string_view word) {
	return std::ranges::binary_search(list, word);
}

constexpr std::array<std::u32string_view, 55> cppKeywords = {U"alignas", U"alignof", U"auto", U"break", U"case", U"catch", U"class", U"co_await", U"co_return", U"co_yield", U"concept", U"const", U"consteval", U"constexpr", U"constinit", U"continue", U"decltype", U"default", U"delete", U"do", U"else", U"enum", U"explicit", U"export", U"extern", U"false", U"final", U"for", U"friend", U"if", U"inline", U"mutable", U"namespace", U"new", U"noexcept", U"nullptr", U"operator", U"override", U"private", U"protected", U"public", U"requires", U"return", U"static", U"struct", U"switch", U"template", U"this", U"throw", U"true", U"try", U"typename", U"using", U"virtual", U"while"}; ///< C++ keywords, sorted.
static_assert(std::ranges::is_sorted(cppKeywords));

constexpr std::array<std::u32string_view, 11> cppTypes = {U"bool", U"char", U"double", U"float", U"int", U"long", U"short", U"signed", U"size_t", U"unsigned", U"void"}; ///< Built-in types, sorted.
static_assert(std::ranges::is_sorted(cppTypes));

} // namespace

CodeEditor::CodeEditor() {
	setColor(Color::hex(0x1b1d23));
	setCursor(Cursor::IBeam);
	resetColors();
}

/* Properties */

void CodeEditor::setText(std::string_view utf8) {
	if(utf8 == utf8_) return;
	if(undo_.size() >= maxUndo) undo_.erase(undo_.begin());
	undo_.push_back({lines_, caret_, anchor_});
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

void CodeEditor::setLanguage(std::string_view name) {
	std::string lower(name);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	const Language language = lower == "c++" || lower == "cpp" || lower == "c" ? Language::Cpp
	                          : lower == "json"                                ? Language::Json
	                          : lower == "cmake"                               ? Language::CMake
	                          : lower == "markdown" || lower == "md"           ? Language::Markdown
	                                                                           : Language::Plain;
	if(language == language_) return;
	language_ = language;
	invalidate();
}

void CodeEditor::setFont(std::shared_ptr<const Font> font) {
	font_ = std::move(font);
	invalidate();
}

const Font* CodeEditor::font() const noexcept {
	if(font_) return font_.get();
	return window() ? window()->defaultFont() : nullptr;
}

void CodeEditor::setTokenColor(Token token, Color color) {
	tokenColors_[static_cast<std::size_t>(token)] = color;
	invalidate();
}

void CodeEditor::setLineNumberColor(Color color) {
	lineNumberColor_ = color;
	invalidate();
}

void CodeEditor::setActiveLineNumberColor(Color color) {
	activeLineNumberColor_ = color;
	invalidate();
}

void CodeEditor::setLineColor(Color color) {
	lineColor_ = color;
	invalidate();
}

void CodeEditor::setSelectionColor(Color color) {
	selectionColor_ = color;
	invalidate();
}

void CodeEditor::setCaretColor(Color color) {
	caretColor_ = color;
	invalidate();
}

void CodeEditor::resetColors() {
	tokenColors_ = {Color::hex(0xd6d9e0), Color::hex(0xc49bff), Color::hex(0x5fd0dd), Color::hex(0xe6b071),
	                Color::hex(0xb5cea8), Color::hex(0x6f8f6b), Color::hex(0x7aa7ff)};
	lineNumberColor_ = Color::hex(0x4b505c);
	activeLineNumberColor_ = Color::hex(0xc8ccd4);
	lineColor_ = Color::hex(0x23262e);
	selectionColor_ = Color::hex(0x264f78);
	caretColor_ = Color::hex(0xaeafad);
	invalidate();
}

std::size_t CodeEditor::selectedLength() const {
	const auto [from, to] = selection();
	return textIn(from, to).size();
}

/* Geometry */

float CodeEditor::lineHeight() const noexcept {
	const Font* f = font();
	return f ? std::round(f->lineHeight() * 1.45f) : 20.0f;
}

float CodeEditor::gutterWidth() const noexcept {
	const Font* f = font();
	if(!f) return 0.0f;
	const std::size_t digits = std::max<std::size_t>(std::to_string(lines_.size()).size(), 3);
	return std::round(16.0f + static_cast<float>(digits) * f->glyph(U'0').advance + 20.0f);
}

float CodeEditor::columnX(std::size_t line, std::size_t column) const noexcept {
	const Font* f = font();
	if(!f) return 0.0f;
	const std::u32string& text = lines_[line];
	return f->measure(std::u32string_view(text).substr(0, std::min(column, text.size())));
}

std::size_t CodeEditor::columnAt(std::size_t line, float x) const noexcept {
	const Font* f = font();
	if(!f) return 0;
	const std::u32string& text = lines_[line];
	float pen = 0.0f;
	for(std::size_t i = 0; i < text.size(); ++i) {
		const float advance = f->glyph(text[i]).advance;
		if(x < pen + advance / 2.0f) return i;
		pen += advance;
	}
	return text.size();
}

CodeEditor::Position CodeEditor::positionAt(Vec2 point) const noexcept {
	const float y = point.y - frame().top - textPadding + scroll_.y;
	const float row = std::floor(y / lineHeight());
	Position p;
	if(row < 0.0f) return p; // above the text: the start
	p.line = static_cast<std::size_t>(row);
	if(p.line >= lines_.size()) return {lines_.size() - 1, lines_.back().size()}; // below: the end
	p.column = columnAt(p.line, point.x - frame().left - gutterWidth() + scroll_.x);
	return p;
}

std::size_t CodeEditor::visibleLines() const noexcept {
	return static_cast<std::size_t>(std::max(std::floor((frame().height() - textPadding) / lineHeight()), 1.0f));
}

void CodeEditor::clampScroll() noexcept {
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

void CodeEditor::revealCaret() noexcept {
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

std::u32string CodeEditor::textIn(Position from, Position to) const {
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

void CodeEditor::erase(Position from, Position to) {
	if(from.line == to.line) {
		lines_[from.line].erase(from.column, to.column - from.column);
		return;
	}
	lines_[from.line] = lines_[from.line].substr(0, from.column) + lines_[to.line].substr(to.column);
	lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(from.line) + 1,
	             lines_.begin() + static_cast<std::ptrdiff_t>(to.line) + 1);
}

CodeEditor::Position CodeEditor::insert(Position at, std::u32string_view text) {
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

std::pair<CodeEditor::Position, CodeEditor::Position> CodeEditor::selection() const noexcept {
	return caret_ < anchor_ ? std::pair{caret_, anchor_} : std::pair{anchor_, caret_};
}

void CodeEditor::replaceSelection(std::u32string_view text) {
	const auto [from, to] = selection();
	erase(from, to);
	caret_ = anchor_ = insert(from, text);
}

CodeEditor::Position CodeEditor::step(Position from, bool forward, bool word) const noexcept {
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

std::size_t CodeEditor::indentOf(std::size_t line) const noexcept {
	const std::u32string& text = lines_[line];
	std::size_t n = 0;
	while(n < text.size() && text[n] == U' ') ++n;
	return n;
}

std::pair<std::size_t, std::size_t> CodeEditor::selectedLines() const noexcept {
	auto [from, to] = selection();
	// A selection ending at the start of a line doesn't include that line
	if(to.line > from.line && to.column == 0) --to.line;
	return {from.line, to.line};
}

/* Editing commands */

void CodeEditor::beginEdit(Edit kind) {
	if(kind == Edit::Other || kind != lastEdit_) {
		if(undo_.size() >= maxUndo) undo_.erase(undo_.begin());
		undo_.push_back({lines_, caret_, anchor_});
	}
	redo_.clear();
	lastEdit_ = kind;
}

void CodeEditor::endEdit() {
	utf8_ = joinLines(lines_);
	preferredX_ = -1.0f;
	revealCaret();
	onTextChanged(utf8_);
	onCaretMoved(caret_);
}

void CodeEditor::moveCaret(Position to, bool extend, bool keepX) {
	if(!keepX) preferredX_ = -1.0f;
	const Position oldCaret = caret_, oldAnchor = anchor_;
	caret_ = to;
	if(!extend) anchor_ = to;
	lastEdit_ = Edit::Other; // typing after a move starts a new undo step
	revealCaret();
	if(caret_ != oldCaret || anchor_ != oldAnchor) onCaretMoved(caret_);
}

void CodeEditor::undo(bool redo) {
	std::vector<Snapshot>& from = redo ? redo_ : undo_;
	std::vector<Snapshot>& to = redo ? undo_ : redo_;
	if(from.empty()) return;
	to.push_back({lines_, caret_, anchor_});
	Snapshot snapshot = std::move(from.back());
	from.pop_back();
	lines_ = std::move(snapshot.lines);
	caret_ = snapshot.caret;
	anchor_ = snapshot.anchor;
	lastEdit_ = Edit::Other;
	endEdit();
}

void CodeEditor::newLine() {
	beginEdit(Edit::Other);
	const auto [from, to] = selection();
	erase(from, to);
	caret_ = anchor_ = from;

	const std::u32string& text = lines_[from.line];
	std::size_t indent = std::min(indentOf(from.line), from.column);
	const char32_t before = from.column > 0 ? text[from.column - 1] : 0;
	const char32_t after = from.column < text.size() ? text[from.column] : 0;
	const bool opens = before == U'{' || before == U'[' || before == U'(' || before == U':';
	const bool closes = opens && after != 0 && after == closerOf(before);

	std::u32string inserted = U"\n" + std::u32string(indent + (opens ? tabSize : 0), U' ');
	if(closes) {
		// Between a pair of brackets: the closing one goes on its own line, the caret on an indented one between
		inserted += U"\n" + std::u32string(indent, U' ');
		const Position end = insert(from, inserted);
		caret_ = anchor_ = {end.line - 1, indent + tabSize};
	} else {
		caret_ = anchor_ = insert(from, inserted);
	}
	endEdit();
}

void CodeEditor::indent(bool outdent) {
	const auto [first, last] = selectedLines();
	if(!outdent && !hasSelection()) {
		beginEdit(Edit::Typing);
		replaceSelection(std::u32string(tabSize - caret_.column % tabSize, U' '));
		endEdit();
		return;
	}
	beginEdit(Edit::Other);
	for(std::size_t line = first; line <= last; ++line) {
		const long change = outdent ? -static_cast<long>(std::min(indentOf(line), tabSize)) : static_cast<long>(tabSize);
		if(outdent) lines_[line].erase(0, static_cast<std::size_t>(-change));
		else if(!lines_[line].empty()) lines_[line].insert(0, tabSize, U' ');
		else continue;
		for(Position* p : {&caret_, &anchor_})
			if(p->line == line) p->column = static_cast<std::size_t>(std::max(0L, static_cast<long>(p->column) + change));
	}
	endEdit();
}

void CodeEditor::toggleComment() {
	const std::u32string_view marker = language_ == Language::CMake ? U"# " : U"// ";
	if(language_ == Language::Json || language_ == Language::Markdown) return; // no line comments there
	const auto [first, last] = selectedLines();

	// Comment out unless every non-empty line already is
	bool allCommented = true;
	std::size_t column = std::u32string::npos; // the markers line up at the smallest indentation
	for(std::size_t line = first; line <= last; ++line) {
		const std::u32string& text = lines_[line];
		const std::size_t indent = indentOf(line);
		if(indent == text.size()) continue;
		column = std::min(column, indent);
		if(text.compare(indent, marker.size() - 1, marker.substr(0, marker.size() - 1)) != 0) allCommented = false;
	}
	if(column == std::u32string::npos) return;

	beginEdit(Edit::Other);
	for(std::size_t line = first; line <= last; ++line) {
		std::u32string& text = lines_[line];
		const std::size_t indent = indentOf(line);
		if(indent == text.size()) continue;
		long change = 0;
		std::size_t at = column;
		if(allCommented) {
			at = indent;
			const std::size_t length = text.compare(indent, marker.size(), marker) == 0 ? marker.size() : marker.size() - 1;
			text.erase(indent, length);
			change = -static_cast<long>(length);
		} else {
			text.insert(column, marker);
			change = static_cast<long>(marker.size());
		}
		for(Position* p : {&caret_, &anchor_})
			if(p->line == line && p->column >= at)
				p->column = static_cast<std::size_t>(std::max(static_cast<long>(at), static_cast<long>(p->column) + change));
	}
	endEdit();
}

void CodeEditor::moveLines(bool down) {
	const auto [first, last] = selectedLines();
	if(down ? last + 1 >= lines_.size() : first == 0) return;
	beginEdit(Edit::Other);
	if(down) std::rotate(lines_.begin() + static_cast<std::ptrdiff_t>(first),
	                     lines_.begin() + static_cast<std::ptrdiff_t>(last) + 1,
	                     lines_.begin() + static_cast<std::ptrdiff_t>(last) + 2);
	else std::rotate(lines_.begin() + static_cast<std::ptrdiff_t>(first) - 1,
	                 lines_.begin() + static_cast<std::ptrdiff_t>(first),
	                 lines_.begin() + static_cast<std::ptrdiff_t>(last) + 1);
	for(Position* p : {&caret_, &anchor_}) p->line = down ? p->line + 1 : p->line - 1;
	endEdit();
}

void CodeEditor::deleteLines() {
	const auto [first, last] = selectedLines();
	beginEdit(Edit::Other);
	if(last + 1 < lines_.size()) erase({first, 0}, {last + 1, 0});
	else if(first > 0) erase({first - 1, lines_[first - 1].size()}, {last, lines_[last].size()});
	else erase({first, 0}, {last, lines_[last].size()});
	const std::size_t line = std::min(first, lines_.size() - 1);
	caret_ = anchor_ = {line, std::min(caret_.column, lines_[line].size())};
	endEdit();
}

void CodeEditor::copy(bool cut) {
	if(!window()) return;
	if(hasSelection()) {
		const auto [from, to] = selection();
		window()->setClipboardText(detail::toUtf8(textIn(from, to)));
		if(cut) {
			beginEdit(Edit::Other);
			replaceSelection({});
			endEdit();
		}
		return;
	}
	// No selection: the whole line, with its line break
	window()->setClipboardText(detail::toUtf8(lines_[caret_.line]) + "\n");
	if(cut) deleteLines();
}

void CodeEditor::paste() {
	if(!window()) return;
	const std::u32string text = detail::fromUtf8(window()->clipboardText());
	if(text.empty()) return;
	beginEdit(Edit::Other);
	replaceSelection(text);
	endEdit();
}

void CodeEditor::selectWord(Position at) {
	const std::u32string& text = lines_[at.line];
	std::size_t start = at.column, end = at.column;
	auto same = [&](char32_t c) {
		const char32_t ref = at.column < text.size() ? text[at.column] : (at.column > 0 ? text[at.column - 1] : U' ');
		if(isWord(ref)) return isWord(c);
		if(isBlank(ref)) return isBlank(c);
		return !isWord(c) && !isBlank(c);
	};
	while(start > 0 && same(text[start - 1])) --start;
	while(end < text.size() && same(text[end])) ++end;
	anchor_ = {at.line, start};
	caret_ = {at.line, end};
	revealCaret();
	onCaretMoved(caret_);
}

/* Events */

void CodeEditor::keyEvent(const KeyEvent& event) {
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
		if(target >= static_cast<long>(lines_.size())) return moveCaret({lines_.size() - 1, lines_.back().size()}, shift, true);
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
		if(m.alt()) return moveLines(false);
		if(m.super()) return moveCaret({0, 0}, shift);
		return vertical(-1);
	case Key::Down:
		if(m.alt()) return moveLines(true);
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
	case Key::PageUp: return vertical(-static_cast<long>(visibleLines()));
	case Key::PageDown: return vertical(static_cast<long>(visibleLines()));
	case Key::Enter:
	case Key::KeypadEnter:
		if(!command) newLine();
		return;
	case Key::Tab:
		if(!command) indent(shift);
		return;
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
		// Inside an empty pair, e.g. "(|)": both go
		const std::u32string& text = lines_[caret_.line];
		Position to = caret_;
		if(!word && !m.super() && caret_.column > 0 && caret_.column < text.size() &&
		   closerOf(text[caret_.column - 1]) == text[caret_.column])
			++to.column;
		erase(from, to);
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
	default: break;
	}

	if(!command) return;
	switch(event.key) {
	case Key::A:
		anchor_ = {0, 0};
		caret_ = {lines_.size() - 1, lines_.back().size()};
		invalidate();
		onCaretMoved(caret_);
		return;
	case Key::C: return copy(false);
	case Key::X: return copy(true);
	case Key::V: return paste();
	case Key::Z: return undo(shift);
	case Key::Y:
		if(m.control()) undo(true);
		return;
	case Key::Slash: return toggleComment();
	case Key::K:
		if(shift) deleteLines();
		return;
	case Key::LeftBracket: // Cmd+[ and Cmd+]: outdent and indent, as in VS Code
	case Key::RightBracket: {
		const Position caret = caret_, anchor = anchor_;
		if(!hasSelection()) {
			anchor_ = {caret_.line, 0};
			caret_ = {caret_.line, lines_[caret_.line].size()};
		}
		indent(event.key == Key::LeftBracket);
		if(caret == anchor) {
			const long change = event.key == Key::LeftBracket ? -static_cast<long>(tabSize) : static_cast<long>(tabSize);
			caret_ = anchor_ = {caret.line, std::min(lines_[caret.line].size(),
			                                         static_cast<std::size_t>(std::max(0L, static_cast<long>(caret.column) + change)))};
			onCaretMoved(caret_);
		}
		return;
	}
	default: return;
	}
}

void CodeEditor::charEvent(const CharEvent& event) {
	Panel::charEvent(event);
	const char32_t c = event.codepoint;
	if(c < 0x20) return;

	const std::u32string& text = lines_[caret_.line];
	const char32_t next = caret_.column < text.size() ? text[caret_.column] : 0;

	// Typing a closer that is already there steps over it
	if(!hasSelection() && (c == U')' || c == U']' || c == U'}' || c == U'"') && next == c) {
		moveCaret({caret_.line, caret_.column + 1}, false);
		return;
	}

	beginEdit(Edit::Typing);
	const char32_t closer = closerOf(c);
	const char32_t previous = caret_.column > 0 ? text[caret_.column - 1] : 0;
	const bool closes = closer && (next == 0 || isBlank(next) || next == U')' || next == U']' || next == U'}' ||
	                               next == U',' || next == U';') &&
	                    !(c == U'"' && isWord(previous));
	if(hasSelection() && closer) {
		// Wrap the selection in the pair
		const auto [from, to] = selection();
		const std::u32string inner = textIn(from, to);
		erase(from, to);
		const Position start = insert(from, std::u32string(1, c));
		const Position end = insert(start, inner);
		insert(end, std::u32string(1, closer));
		anchor_ = start;
		caret_ = end;
	} else if(closes) {
		replaceSelection(std::u32string{c, closer});
		anchor_ = caret_ = {caret_.line, caret_.column - 1};
	} else {
		replaceSelection(std::u32string(1, c));
		// A closing brace typed on a blank line goes back one level
		if(c == U'}' && indentOf(caret_.line) + 1 == caret_.column && caret_.column > tabSize) {
			lines_[caret_.line].erase(0, tabSize);
			caret_.column -= tabSize;
			anchor_ = caret_;
		}
	}
	endEdit();
}

void CodeEditor::mouseButtonEvent(const MouseButtonEvent& event) {
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

void CodeEditor::mouseMoveEvent(const MouseMoveEvent& event) {
	Panel::mouseMoveEvent(event);
	if(!dragging_ || clicks_ != 1) return;
	const Position at = positionAt(event.position);
	if(at != caret_) moveCaret(at, true);
}

void CodeEditor::scrollEvent(const ScrollEvent& event) {
	Panel::scrollEvent(event);
	const Vec2 before = scroll_;
	scroll_.x -= event.offset.x * scrollSpeed;
	scroll_.y -= event.offset.y * scrollSpeed;
	clampScroll();
	if(scroll_.x != before.x || scroll_.y != before.y) invalidate();
}

void CodeEditor::focusEvent(bool focused) {
	if(!focused) dragging_ = false;
	invalidate(); // the caret appears or disappears
	Panel::focusEvent(focused);
}

void CodeEditor::arrange() { clampScroll(); }

/* Drawing */

void CodeEditor::paint(Painter& painter) const {
	Panel::paint(painter);
	const Font* f = font();
	if(!f) return;

	const Rect& box = frame();
	const float lh = lineHeight();
	const float gutter = gutterWidth();
	const float textLeft = std::round(box.left + gutter - scroll_.x);
	const float firstTop = box.top + textPadding - scroll_.y;
	const float baselineOffset = std::round((lh - f->lineHeight()) / 2.0f + f->ascent());
	const auto [selFrom, selTo] = selection();

	const auto first = static_cast<std::size_t>(std::max(std::floor(scroll_.y / lh) - 1.0f, 0.0f));
	const std::size_t last = std::min(lines_.size(), first + visibleLines() + 3);

	// Block comments may start above the visible lines
	bool inComment = false;
	for(std::size_t line = 0; line < first; ++line) (void)highlight(lines_[line], inComment);

	for(std::size_t line = first; line < last; ++line) {
		const float top = std::round(firstTop + static_cast<float>(line) * lh);
		const std::u32string& text = lines_[line];

		if(line == caret_.line && !hasSelection())
			painter.fillRect({box.left, top, box.right, top + lh}, lineColor_);

		if(hasSelection() && line >= selFrom.line && line <= selTo.line) {
			const float from = line == selFrom.line ? columnX(line, selFrom.column) : 0.0f;
			// A selected line break shows as a space's width
			const float to = line == selTo.line ? columnX(line, selTo.column) : f->measure(text) + f->glyph(U' ').advance;
			painter.fillRect({textLeft + from, top, textLeft + to, top + lh}, selectionColor_);
		}

		// Runs of one colour
		const std::vector<Token> tokens = highlight(text, inComment);
		float pen = static_cast<float>(textLeft);
		std::size_t start = 0;
		while(start < text.size()) {
			std::size_t end = start + 1;
			while(end < text.size() && tokens[end] == tokens[start]) ++end;
			const std::u32string_view run = std::u32string_view(text).substr(start, end - start);
			const float width = f->measure(run);
			if(pen + width >= box.left + gutter && pen <= box.right)
				painter.drawText(*f, run, {pen, top + baselineOffset}, tokenColors_[static_cast<std::size_t>(tokens[start])]);
			pen += width;
			start = end;
		}
	}

	// The gutter covers text scrolled under it
	if(color().a > 0.0f) painter.fillRect({box.left, box.top, box.left + gutter - 8.0f, box.bottom}, color());
	for(std::size_t line = first; line < last; ++line) {
		const float top = std::round(firstTop + static_cast<float>(line) * lh);
		const std::u32string number = detail::fromUtf8(std::to_string(line + 1));
		const float x = std::round(box.left + gutter - 20.0f - f->measure(number));
		painter.drawText(*f, number, {x, top + baselineOffset},
		                 line == caret_.line ? activeLineNumberColor_ : lineNumberColor_);
	}

	if(hasFocus()) {
		const float x = std::round(textLeft + columnX(caret_.line, caret_.column));
		const float top = std::round(firstTop + static_cast<float>(caret_.line) * lh);
		if(x >= box.left + gutter - 8.0f) painter.fillRect({x, top + 1.0f, x + 2.0f, top + lh - 1.0f}, caretColor_);
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

/* Syntax */

std::vector<CodeEditor::Token> CodeEditor::highlight(std::u32string_view line, bool& inComment) const {
	std::vector<Token> tokens(line.size(), Token::Plain);
	const std::size_t n = line.size();
	auto fill = [&](std::size_t from, std::size_t to, Token token) {
		std::fill(tokens.begin() + static_cast<std::ptrdiff_t>(from),
		          tokens.begin() + static_cast<std::ptrdiff_t>(std::min(to, n)), token);
	};
	// A quoted string from an opening quote: returns the index after the closing one
	auto quoted = [&](std::size_t i) {
		const char32_t quote = line[i];
		std::size_t j = i + 1;
		while(j < n && line[j] != quote) j += line[j] == U'\\' ? 2 : 1;
		return std::min(j + 1, n);
	};
	auto number = [&](std::size_t i) {
		std::size_t j = i;
		while(j < n && (isWord(line[j]) || line[j] == U'.' || line[j] == U'\'')) ++j;
		return j;
	};

	switch(language_) {
	case Language::Plain: break;

	case Language::Cpp: {
		std::size_t i = 0;
		if(inComment) {
			const std::size_t end = line.find(U"*/");
			i = end == std::u32string_view::npos ? n : end + 2;
			fill(0, i, Token::Comment);
			if(end != std::u32string_view::npos) inComment = false;
		}
		const std::size_t firstChar = line.find_first_not_of(U' ');
		if(i == 0 && firstChar != std::u32string_view::npos && line[firstChar] == U'#') {
			// A preprocessor line: the directive, then <header> or "header" as a string
			std::size_t j = firstChar + 1;
			while(j < n && isWord(line[j])) ++j;
			fill(firstChar, j, Token::Keyword);
			i = j;
			const std::size_t open = line.find_first_of(U"<\"", i);
			if(open != std::u32string_view::npos) {
				const std::size_t close = line.find(line[open] == U'<' ? U'>' : U'"', open + 1);
				fill(open, close == std::u32string_view::npos ? n : close + 1, Token::String);
				i = close == std::u32string_view::npos ? n : close + 1;
			}
		}
		while(i < n) {
			const char32_t c = line[i];
			if(c == U'/' && i + 1 < n && line[i + 1] == U'/') {
				fill(i, n, Token::Comment);
				break;
			}
			if(c == U'/' && i + 1 < n && line[i + 1] == U'*') {
				const std::size_t end = line.find(U"*/", i + 2);
				const std::size_t stop = end == std::u32string_view::npos ? n : end + 2;
				fill(i, stop, Token::Comment);
				if(end == std::u32string_view::npos) inComment = true;
				i = stop;
			} else if(c == U'R' && i + 2 < n && line[i + 1] == U'"' && line[i + 2] == U'(') {
				const std::size_t end = line.find(U")\"", i + 3);
				const std::size_t stop = end == std::u32string_view::npos ? n : end + 2;
				fill(i, stop, Token::String);
				i = stop;
			} else if(c == U'"' || c == U'\'') {
				const std::size_t stop = quoted(i);
				fill(i, stop, Token::String);
				i = stop;
			} else if(c >= U'0' && c <= U'9') {
				const std::size_t stop = number(i);
				fill(i, stop, Token::Number);
				i = stop;
			} else if(isWord(c)) {
				std::size_t j = i;
				while(j < n && isWord(line[j])) ++j;
				const std::u32string_view w = line.substr(i, j - i);
				if(contains(cppKeywords, w)) fill(i, j, Token::Keyword);
				else if(contains(cppTypes, w) || (w.front() >= U'A' && w.front() <= U'Z') || w == U"std")
					fill(i, j, Token::Type);
				i = j;
			} else {
				++i;
			}
		}
		break;
	}

	case Language::Json: {
		std::size_t i = 0;
		while(i < n) {
			const char32_t c = line[i];
			if(c == U'"') {
				const std::size_t stop = quoted(i);
				std::size_t k = stop;
				while(k < n && isBlank(line[k])) ++k;
				fill(i, stop, k < n && line[k] == U':' ? Token::Type : Token::String); // keys and values differ
				i = stop;
			} else if((c >= U'0' && c <= U'9') || c == U'-') {
				const std::size_t stop = number(i + 1);
				fill(i, stop, Token::Number);
				i = stop;
			} else if(isWord(c)) {
				std::size_t j = i;
				while(j < n && isWord(line[j])) ++j;
				const std::u32string_view w = line.substr(i, j - i);
				if(w == U"true" || w == U"false" || w == U"null") fill(i, j, Token::Keyword);
				i = j;
			} else {
				++i;
			}
		}
		break;
	}

	case Language::CMake: {
		std::size_t i = 0;
		while(i < n) {
			const char32_t c = line[i];
			if(c == U'#') {
				fill(i, n, Token::Comment);
				break;
			}
			if(c == U'"') {
				const std::size_t stop = quoted(i);
				fill(i, stop, Token::String);
				i = stop;
			} else if(c == U'$' && i + 1 < n && line[i + 1] == U'{') {
				const std::size_t end = line.find(U'}', i);
				const std::size_t stop = end == std::u32string_view::npos ? n : end + 1;
				fill(i, stop, Token::Type);
				i = stop;
			} else if(c >= U'0' && c <= U'9') {
				const std::size_t stop = number(i);
				fill(i, stop, Token::Number);
				i = stop;
			} else if(isWord(c)) {
				std::size_t j = i;
				while(j < n && (isWord(line[j]) || line[j] == U':')) ++j;
				const std::u32string_view w = line.substr(i, j - i);
				std::size_t k = j;
				while(k < n && isBlank(line[k])) ++k;
				if(k < n && line[k] == U'(') fill(i, j, Token::Keyword); // a command
				else if(std::ranges::all_of(w, [](char32_t ch) { return !(ch >= U'a' && ch <= U'z'); }))
					fill(i, j, Token::Type); // an option such as PRIVATE
				i = j;
			} else {
				++i;
			}
		}
		break;
	}

	case Language::Markdown: {
		const std::size_t firstChar = line.find_first_not_of(U' ');
		if(firstChar == std::u32string_view::npos) break;
		if(firstChar >= 4) {
			fill(0, n, Token::String); // an indented code block
			break;
		}
		if(line[firstChar] == U'#') {
			fill(0, n, Token::Heading);
			break;
		}
		if((line[firstChar] == U'-' || line[firstChar] == U'*') && firstChar + 1 < n && line[firstChar + 1] == U' ')
			fill(firstChar, firstChar + 1, Token::Keyword);
		for(std::size_t i = firstChar; i < n; ++i) {
			if(line[i] != U'`') continue;
			const std::size_t end = line.find(U'`', i + 1);
			const std::size_t stop = end == std::u32string_view::npos ? n : end + 1;
			fill(i, stop, Token::String);
			i = stop - 1;
		}
		break;
	}
	}
	return tokens;
}

} // namespace affresco
