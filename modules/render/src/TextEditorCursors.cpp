/// @file
/// @brief TextEditor's extra cursors (typing, deleting and moving at each), column selection, Cmd+D, decorations and
/// the gutter's default labels.

#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Window.hpp"

#include "detail/Utf8.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace affresco {

namespace {

constexpr std::size_t maxInlineLength = 256;        ///< Code points of a decoration's inline text kept, at most.
constexpr std::size_t maxOccurrenceScan = 100'000; ///< Lines Cmd+D reads for the next occurrence, at most.

/// @brief A selection's start.
/// @param s The selection.
TextPosition startOf(const EditorSelection& s) noexcept { return std::min(s.anchor, s.caret); }

/// @brief A selection's end.
/// @param s The selection.
TextPosition endOf(const EditorSelection& s) noexcept { return std::max(s.anchor, s.caret); }

} // namespace

/* Decorations */

void TextEditor::setDecorations(std::vector<EditorDecoration> decorations) {
	const std::size_t lines = lines_.size();
	std::erase_if(decorations, [lines](const EditorDecoration& d) { return d.line >= lines; });
	std::ranges::stable_sort(decorations, {}, &EditorDecoration::line);
	std::vector<EditorDecoration> kept;
	kept.reserve(std::min(decorations.size(), maxDecorations));
	std::size_t onLine = 0;
	decorationSpan_ = 0;
	for(EditorDecoration& d : decorations) {
		if(kept.size() == maxDecorations) break;
		onLine = !kept.empty() && kept.back().line == d.line ? onLine + 1 : 1;
		if(onLine > maxDecorationsPerLine) continue;
		d.endLine = std::clamp(d.endLine, d.line, lines - 1);
		if(d.text.size() > maxInlineLength) d.text.resize(maxInlineLength);
		decorationSpan_ = std::max(decorationSpan_, d.endLine - d.line);
		kept.push_back(std::move(d));
	}
	if(kept == decorations_) return;
	decorations_ = std::move(kept);
	invalidatePaint();
}

void TextEditor::lineLabel(std::size_t line, std::u32string& out) const {
	out.clear();
	for(const char c : std::to_string(line + 1)) out.push_back(static_cast<char32_t>(c));
}

/* Cursors */

TextEditor::Position TextEditor::shifted(Position at, const TextChange& change) noexcept {
	if(at < change.from) return at;
	if(at < change.oldEnd) return change.from; // in the text removed
	if(at.line == change.oldEnd.line) return {change.newEnd.line, change.newEnd.column + at.column - change.oldEnd.column};
	return {at.line - change.oldEnd.line + change.newEnd.line, at.column};
}

bool TextEditor::addCursor(EditorSelection selection) {
	if(cursors_.size() >= maxCursors) return false;
	selection = {clamped(selection.anchor), clamped(selection.caret)};
	if(selection == EditorSelection{anchor_, caret_} || std::ranges::find(cursors_, selection) != cursors_.end())
		return false;
	cursors_.push_back(selection);
	mergeCursors();
	invalidatePaint();
	return true;
}

void TextEditor::clearCursors() {
	if(cursors_.empty()) return;
	cursors_.clear();
	invalidatePaint();
}

void TextEditor::mergeCursors() {
	std::ranges::sort(cursors_, {}, startOf);
	const EditorSelection main{anchor_, caret_};
	// Two selections overlap if they share text or a start, or a caret touches the other
	const auto overlaps = [](const EditorSelection& a, const EditorSelection& b) {
		const TextPosition from = std::max(startOf(a), startOf(b)), to = std::min(endOf(a), endOf(b));
		return from < to || startOf(a) == startOf(b) || (from == to && (a.caret == a.anchor || b.caret == b.anchor));
	};
	std::vector<EditorSelection> merged;
	merged.reserve(cursors_.size());
	for(const EditorSelection& s : cursors_) {
		if(overlaps(main, s)) continue; // the caret's own selection takes it
		if(!merged.empty() && overlaps(merged.back(), s)) {
			merged.back() = {startOf(merged.back()), std::max(endOf(merged.back()), endOf(s))};
			continue;
		}
		merged.push_back(s);
	}
	cursors_ = std::move(merged);
}

void TextEditor::forEachCursor(Edit kind, const std::function<void()>& body) {
	std::vector<EditorSelection> all;
	all.reserve(cursors_.size() + 1);
	all.push_back({anchor_, caret_});
	all.insert(all.end(), cursors_.begin(), cursors_.end());
	cursors_.clear();
	editedCursors_ = &all; // textChanged() moves the others as each is edited
	beginEdit(kind);
	for(std::size_t i = 0; i < all.size(); ++i) {
		activeCursor_ = i;
		anchor_ = all[i].anchor;
		caret_ = all[i].caret;
		body();
		all[i] = {anchor_, caret_};
	}
	activeCursor_ = SIZE_MAX;
	editedCursors_ = nullptr;
	anchor_ = all.front().anchor;
	caret_ = all.front().caret;
	cursors_.assign(all.begin() + 1, all.end());
	mergeCursors();
	endEdit();
}

void TextEditor::moveCursors(const std::function<EditorSelection(EditorSelection)>& move) {
	for(EditorSelection& s : cursors_) s = move(s);
	mergeCursors();
	invalidatePaint();
}

/* Keys */

bool TextEditor::viewKey(const KeyEvent& event) {
	const Mods m = event.mods;
	const bool command = m.super() || m.control();
	if(event.key == Key::Escape && !cursors_.empty()) {
		clearCursors();
		return true;
	}
	if(!command) return false;
	if(event.key == Key::A) clearCursors(); // then selects everything as usual
	if(event.key == Key::G && !m.alt()) {
		findNext(m.shift());
		return true;
	}
	if(event.key == Key::D && !m.shift() && !m.alt()) {
		selectNextOccurrence();
		return true;
	}
	if(m.alt() && (event.key == Key::Up || event.key == Key::Down)) {
		addCursorRow(event.key == Key::Down);
		return true;
	}
	if(m.alt() && (event.key == Key::LeftBracket || event.key == Key::RightBracket)) {
		if(event.key == Key::LeftBracket) fold(caret_.line);
		else unfold(caret_.line);
		return true;
	}
	return false;
}

bool TextEditor::cursorsKey(const KeyEvent& event) {
	if(cursors_.empty()) return false;
	const Mods m = event.mods;
	const bool command = m.super() || m.control();
	const bool word = m.alt() || (m.control() && !m.super());
	const bool shift = m.shift();
	const auto moveEach = [&](auto to) {
		moveCursors([&](EditorSelection s) {
			const Position target = to(s);
			return EditorSelection{shift ? s.anchor : target, target};
		});
		return false; // the caret moves as without extra cursors
	};
	const auto vertical = [this](Position p, long rows) {
		const long row = std::clamp(static_cast<long>(rowOf(p)) + rows, 0L, static_cast<long>(rowCount()) - 1);
		return positionOnRow(static_cast<std::size_t>(row), xOnRow(p));
	};
	if(command && event.key != Key::V) return false;
	switch(event.key) {
	case Key::Left:
		return moveEach([&](EditorSelection s) { return step(s.caret, false, word); });
	case Key::Right:
		return moveEach([&](EditorSelection s) { return step(s.caret, true, word); });
	case Key::Up:
		return moveEach([&](EditorSelection s) { return vertical(s.caret, -1); });
	case Key::Down:
		return moveEach([&](EditorSelection s) { return vertical(s.caret, 1); });
	case Key::Home:
		return moveEach([&](EditorSelection s) { return Position{s.caret.line, 0}; });
	case Key::End:
		return moveEach([&](EditorSelection s) { return Position{s.caret.line, lines_[s.caret.line].size()}; });
	default:
		break;
	}
	if(!editable()) return false;
	switch(event.key) {
	case Key::Enter:
	case Key::KeypadEnter:
		forEachCursor(Edit::Other, [this] {
			const Position from = selection().first;
			replaceSelection(U"\n" + std::u32string(std::min(indentOf(from.line), from.column), U' '));
		});
		return true;
	case Key::Tab:
		forEachCursor(Edit::Typing,
		              [this] { replaceSelection(std::u32string(tabSize - caret_.column % tabSize, U' ')); });
		return true;
	case Key::Backspace:
	case Key::Delete: {
		const bool forward = event.key == Key::Delete;
		forEachCursor(Edit::Deleting, [this, forward, word] {
			if(!hasSelection()) caret_ = step(caret_, forward, word);
			replaceSelection({});
		});
		return true;
	}
	case Key::V: {
		if(!window()) return true;
		const std::u32string text = detail::fromUtf8(window()->clipboardText());
		if(!text.empty()) forEachCursor(Edit::Other, [&] { replaceSelection(text); });
		return true;
	}
	default:
		return false;
	}
}

void TextEditor::selectNextOccurrence() {
	if(!hasSelection()) return selectWord(caret_);
	const auto [from, to] = selection();
	if(from.line != to.line) return;
	const std::u32string needle = textIn(from, to);
	// From the end of the last selection, wrapping around once
	Position start = to;
	for(const EditorSelection& s : cursors_) start = std::max(start, endOf(s));
	const std::size_t lines = lines_.size();
	for(std::size_t scanned = 0; scanned <= std::min(lines, maxOccurrenceScan); ++scanned) {
		const std::size_t line = (start.line + scanned) % lines;
		const std::size_t column = scanned == 0 ? start.column : 0;
		const std::size_t at = lines_[line].find(needle, column);
		if(at == std::u32string::npos) continue;
		const Position found{line, at}, end{line, at + needle.size()};
		if(found == from) return; // back at the caret's: every occurrence has one
		if(addCursor({found, end})) return;
	}
}

void TextEditor::addCursorRow(bool down) {
	Position outer = caret_;
	for(const EditorSelection& s : cursors_) outer = down ? std::max(outer, s.caret) : std::min(outer, s.caret);
	const std::size_t row = rowOf(outer);
	if(down ? row + 1 >= rowCount() : row == 0) return;
	const Position at = positionOnRow(down ? row + 1 : row - 1, xOnRow(outer));
	addCursor({at, at});
}

void TextEditor::selectColumn(Vec2 from, Vec2 to) {
	const Position a = positionAt(from), b = positionAt(to);
	const float left = frame().left + gutterWidth() - scroll_.x;
	const float fromX = from.x - left, toX = to.x - left;
	cursors_.clear();
	const std::size_t first = std::min(a.line, b.line), last = std::max(a.line, b.line);
	for(std::size_t line = first; line <= last && cursors_.size() < maxCursors; line = nextShown(line)) {
		if(line == b.line) continue;
		cursors_.push_back({{line, columnAt(line, fromX)}, {line, columnAt(line, toX)}});
	}
	anchor_ = {b.line, columnAt(b.line, fromX)};
	caret_ = {b.line, columnAt(b.line, toX)};
	mergeCursors();
	revealCaret();
	onCaretMoved(caret_);
}

} // namespace affresco
