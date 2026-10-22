/// @file
/// @brief TextEditor: properties, geometry, editing on the text model and undo (the keys and the pointer are in
/// TextEditorInput.cpp, drawing in TextEditorPaint.cpp; rows, cursors and find in TextEditorView.cpp,
/// TextEditorCursors.cpp and TextEditorFind.cpp).

#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "detail/Utf8.hpp"
#include "editor/TextEditorRows.hpp"
#include "editor/TextEditorShared.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace affresco {

using editor::isBlank;
using editor::isWord;

namespace {

/// @brief Moves a column back by a character, or by a word: the blanks before it, then a run of word characters or
/// one other character.
/// @param text The line.
/// @param column The column, above 0.
/// @param word Whether it moves by a word.
/// @return The column.
[[nodiscard]] std::size_t stepBack(const std::u32string& text, std::size_t column, bool word) noexcept {
	if(!word) return column - 1;
	while(column > 0 && isBlank(text[column - 1])) --column;
	if(column > 0 && isWord(text[column - 1]))
		while(column > 0 && isWord(text[column - 1])) --column;
	else if(column > 0) --column;
	return column;
}

/// @brief Moves a column forward by a character, or by a word: the blanks after it, then a run of word characters
/// or one other character.
/// @param text The line.
/// @param column The column, below the line's length.
/// @param word Whether it moves by a word.
/// @return The column.
[[nodiscard]] std::size_t stepForward(const std::u32string& text, std::size_t column, bool word) noexcept {
	if(!word) return column + 1;
	while(column < text.size() && isBlank(text[column])) ++column;
	if(column < text.size() && isWord(text[column]))
		while(column < text.size() && isWord(text[column])) ++column;
	else if(column < text.size()) ++column;
	return column;
}

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
	cursorsMerge_ = false;
	cursorsPending_ = true;
	cursorsBefore_ = cursors_;
	model_->beginStep(false, caret_, anchor_);
	model_->assign(utf8);
	model_->endStep();
	cursors_.clear();
	recordCursorStep();
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
	const auto shift = [&](EditorSelection& selection) {
		selection = {shifted(selection.anchor, change), shifted(selection.caret, change)};
	};
	std::ranges::for_each(cursors_, shift);
	// forEachCursor()'s cursors, but the one being edited, which the edit itself places
	for(std::size_t i = 0; editedCursors_ && i < editedCursors_->size(); ++i)
		if(i != activeCursor_) shift((*editedCursors_)[i]);
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
	// At either end of a line, it crosses to the next or the previous one (not past the text's ends)
	const std::u32string& text = lines_[from.line];
	if(!forward) {
		if(from.column == 0) return from.line == 0 ? from : Position{from.line - 1, lines_[from.line - 1].size()};
		return {from.line, stepBack(text, from.column, word)};
	}
	if(from.column == text.size()) return from.line + 1 == lines_.size() ? from : Position{from.line + 1, 0};
	return {from.line, stepForward(text, from.column, word)};
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
	const bool merge = kind != Edit::Other && kind == lastEdit_;
	cursorsMerge_ = merge && model_->undoSteps() > 0;
	cursorsPending_ = true;
	if(editedCursors_) cursorsBefore_.assign(editedCursors_->begin() + 1, editedCursors_->end());
	else cursorsBefore_ = cursors_;
	model_->beginStep(merge, caret_, anchor_);
	lastEdit_ = kind;
}

void TextEditor::endEdit() {
	model_->endStep();
	recordCursorStep();
	editing_ = false;
	preferredX_ = -1.0f;
	revealCaret();
	if(!onTextChanged.empty()) onTextChanged(model_->text());
	onCaretMoved(caret_);
}

void TextEditor::recordCursorStep() {
	if(!cursorsPending_) return; // undo and redo end no step of their own
	cursorsPending_ = false;
	redoCursors_.clear();
	if(cursorsMerge_ && !undoCursors_.empty()) undoCursors_.back().after = cursors_;
	else undoCursors_.push_back({std::move(cursorsBefore_), cursors_});
	cursorsBefore_.clear();
	// Aligned with the model's steps from the newest: the oldest go when the model drops them
	const std::size_t kept = model_->undoSteps();
	if(undoCursors_.size() > kept)
		undoCursors_.erase(undoCursors_.begin(), undoCursors_.end() - static_cast<std::ptrdiff_t>(kept));
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
	const std::size_t steps = model_->undoSteps();
	const bool undone = model_->undo(redo, caret_, anchor_);
	editing_ = false;
	if(!undone) return;
	// The extra cursors come back with the caret, while they are kept for every step
	std::vector<CursorStep>& from = redo ? redoCursors_ : undoCursors_;
	std::vector<CursorStep>& to = redo ? undoCursors_ : redoCursors_;
	if(redo ? !from.empty() : from.size() == steps) {
		cursors_ = redo ? from.back().after : from.back().before;
		to.push_back(std::move(from.back()));
		from.pop_back();
		mergeCursors();
	} else {
		cursors_.clear();
		undoCursors_.clear();
		redoCursors_.clear();
	}
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
	const auto same = [ref](char32_t c) {
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

void TextEditor::arrange() {
	// The wrapped rows follow the width (and the font)
	if(wrap_ && (wrapWidth() != rowsWidth_ || font() != rowsFont_)) rebuildRows();
	clampScroll();
}

} // namespace affresco
