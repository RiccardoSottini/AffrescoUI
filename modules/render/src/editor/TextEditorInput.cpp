/// @file
/// @brief TextEditor: the keys, the characters typed and the pointer (the cursors' keys are in TextEditorCursors.cpp,
/// the view's in TextEditorView.cpp).

#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Window.hpp"

#include "detail/Utf8.hpp"
#include "editor/TextEditorShared.hpp"

#include <string>

namespace affresco {

using editor::commandHeld;
using editor::wordAround;
using editor::wordHeld;

namespace {

constexpr float scrollSpeed = 24.0f;       ///< Pixels per wheel step.
constexpr double doubleClickSeconds = 0.4; ///< Longest pause between the presses of a double click.

/// @brief Where Home takes the caret on its line: the first non-blank character, or the line's start if it is there.
/// @param column The caret's column.
/// @param indent The line's indent.
/// @return The column.
[[nodiscard]] constexpr std::size_t homeColumn(std::size_t column, std::size_t indent) noexcept {
	return column == indent ? 0 : indent;
}

/// @brief Counts a click: a quick one on the same line makes a double, then a triple click, then a single again.
/// @param clicks The clicks counted so far.
/// @param since The seconds since the last one.
/// @param sameLine Whether it is on the last one's line.
/// @return The clicks, from 1 to 3.
[[nodiscard]] int nextClicks(int clicks, double since, bool sameLine) noexcept {
	return since < doubleClickSeconds && sameLine ? clicks % 3 + 1 : 1;
}

/// @brief Where a whole line ends when it is selected: the next line's start, or the last line's end.
/// @param lines The lines.
/// @param line The line.
/// @return The position.
[[nodiscard]] TextEditor::Position lineEnd(const TextLines& lines, std::size_t line) {
	return line + 1 < lines.size() ? TextEditor::Position{line + 1, 0} : TextEditor::Position{line, lines[line].size()};
}

} // namespace

void TextEditor::keyEvent(const KeyEvent& event) {
	Panel::keyEvent(event);
	if(event.action == Action::Release || (window() && window()->keyTaken())) return; // a binding may have taken it
	builtInKey(event);
}

void TextEditor::builtInKey(const KeyEvent& event) {
	if(viewKey(event) || cursorsKey(event) || caretKey(event) || editingKey(event)) return;
	if(commandHeld(event.mods)) commandKey(event);
}

bool TextEditor::caretKey(const KeyEvent& event) {
	const Mods m = event.mods;
	const bool command = commandHeld(m);
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
		moveCaret(command ? Position{0, 0} : Position{caret_.line, homeColumn(caret_.column, indentOf(caret_.line))},
		          shift);
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
	const bool word = wordHeld(mods);
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
	const bool command = commandHeld(m);
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
		replaceSelection(std::u32string(tabSize_ - caret_.column % tabSize_, U' '));
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
	const bool word = wordHeld(mods);
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
	// The window's clock, as its other double clicks read: a test controls it, so a slow build makes no double click
	const double time = window() ? window()->clock().seconds() : SteadyClock().seconds();
	clicks_ = nextClicks(clicks_, time - lastClickTime_, at.line == lastClickAt_.line);
	lastClickTime_ = time;
	lastClickAt_ = at;
	dragging_ = true;

	if(clicks_ == 2) return selectWord(at);
	if(clicks_ == 3) {
		anchor_ = {at.line, 0};
		caret_ = lineEnd(lines_, at.line);
		invalidatePaint();
		onCaretMoved(caret_);
		return;
	}
	moveCaret(at, event.mods.shift());
}

void TextEditor::mouseMoveEvent(const MouseMoveEvent& event) {
	Panel::mouseMoveEvent(event);
	if(columnDragging_) return selectColumn(columnFrom_, event.position);
	if(const bool over = minimap_ && minimapFrame().contains(event.position); over != minimapHovered_) {
		minimapHovered_ = over;
		if(minimapViewportOnHover_) invalidatePaint();
	}
	if(minimapDrag(event.position)) return;
	if(!dragging_ && !onWordHovered.empty()) {
		const Position at = positionAt(event.position);
		const std::u32string& text = lines_[at.line];
		const auto [start, end] = wordAround(text, at.column);
		const bool onWord = start < end && event.position.x >= frame().left + gutterWidth();
		const Position word = onWord ? Position{at.line, start} : Position{SIZE_MAX, 0};
		if(word != hoveredWord_)
			onWordHovered(hoveredWord_ = word, onWord ? detail::toUtf8(text.substr(start, end - start)) : "");
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
	noteView();
}

void TextEditor::hoverEvent(bool hovered) {
	if(!hovered) {
		hoverAt(std::nullopt);
		if(minimapHovered_ && minimapViewportOnHover_) invalidatePaint();
		minimapHovered_ = false;
	}
	Panel::hoverEvent(hovered);
}

void TextEditor::focusEvent(bool focused) {
	if(!focused) dragging_ = columnDragging_ = minimapDragging_ = false;
	invalidatePaint(); // the caret appears or disappears
	Panel::focusEvent(focused);
}

} // namespace affresco
