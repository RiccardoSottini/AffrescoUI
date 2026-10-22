#pragma once

/// @file
/// @brief TextEditor: a multi-line text editor, with optional line numbers, that applications can extend.

#include "affresco/core/TextPattern.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/render/TextEditorView.hpp"
#include "affresco/render/TextModel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace affresco {

namespace detail {
class ViewRows;
} // namespace detail

/// @brief A multi-line text editor, with the usual keys (Cmd on macOS, Ctrl elsewhere).
///
/// Click to place the caret, drag to select (double click: a word, triple: a line). Arrows, Home/End, Page Up/Down;
/// Alt+arrows jump words, Cmd+arrows go to line or document ends; Shift extends the selection. Typing replaces the
/// selection; Enter keeps the line's indentation; Tab and Shift+Tab indent; Cmd+A/C/X/V/Z, Cmd+Shift+Z. Draws optional
/// line numbers in a gutter, highlights the caret's line and scrolls with the wheel. Tabs become spaces; the caret
/// doesn't blink, so an idle editor costs nothing. The text is a TextModel, so a file of any size opens and edits fast:
/// only the lines drawn are decoded and coloured.
///
/// The view (TextEditorView.cpp, TextEditorCursors.cpp, TextEditorFind.cpp): word wrap, folding, decorations, extra
/// cursors (Alt+click, Cmd+Alt+Up/Down, Cmd+D, Alt+Shift+drag for a column; Escape clears them), find and replace
/// (plain, whole word or PCRE2 with match limits), bracket matching, indentation guides and IME composition. Several
/// editors may show one model (setModel()): each keeps its caret where the others' edits move the text.
///
/// Applications extend it by subclassing: colorLine() colours the text (e.g. syntax), and the event hooks can add
/// commands with the protected editing API (beginEdit(), the text operations, endEdit()), which keeps undo working.
class TextEditor : public Panel {
public:
	using Position = TextPosition; ///< A place in the text: a line and a code point within it, both from 0.

	static constexpr std::size_t tabSize = 4; ///< Spaces per indentation level.

	static constexpr std::size_t maxDecorations = 10'000;      ///< Decorations kept, at most; the rest are dropped.
	static constexpr std::size_t maxDecorationsPerLine = 8;    ///< Decorations starting on one line, at most.
	static constexpr std::size_t maxCursors = 10'000;          ///< Extra cursors, at most.
	static constexpr std::size_t maxFolds = 10'000;            ///< Folded ranges, at most.
	static constexpr std::size_t maxMatches = 10'000;          ///< Matches counted or replaced at once, at most.
	static constexpr std::size_t maxBracketScan = 100'000;     ///< Code points scanned for a matching bracket.
	static constexpr std::size_t maxCompositionLength = 256;   ///< Code points of IME composition shown, at most.

	/// @brief An empty editor with a dark background and an I-beam cursor.
	TextEditor();
	/// @brief Stops listening to the model.
	~TextEditor() override;
	TextEditor(const TextEditor&) = delete;
	TextEditor& operator=(const TextEditor&) = delete;

	/// @brief Replaces the text, without firing onTextChanged, unless it is the same. The caret stays where it was if
	/// the line still exists; the undo history is kept, so an outside change can be undone like a typed one.
	/// @param utf8 UTF-8 text, lines separated by "\n" ("\r\n" too); tabs become spaces.
	void setText(std::string_view utf8);
	/// @brief The text, as UTF-8 with "\n" between lines (a copy: model() reads it without one).
	[[nodiscard]] std::string text() const { return model_->text(); }
	/// @brief The text model: its lines, ranges and size, read without copying the text.
	[[nodiscard]] const TextModel& model() const noexcept { return *model_; }
	/// @brief Shows another model, e.g. a file buffer that several editors share; the caret goes to its start, folds
	/// and extra cursors go, and the model's undo history is the editor's.
	/// @param model The model; nullptr for a new, empty one.
	void setModel(std::shared_ptr<TextModel> model);
	/// @brief The model, to share with another editor.
	[[nodiscard]] const std::shared_ptr<TextModel>& sharedModel() const noexcept { return model_; }

	/// @brief Makes the text read-only (it can still be selected, copied, searched and set by setText()).
	/// @param readOnly true to refuse typing, pasting, cutting, replacing and undo.
	void setReadOnly(bool readOnly) noexcept { readOnly_ = readOnly; }
	/// @brief Whether the text is read-only.
	[[nodiscard]] bool readOnly() const noexcept { return readOnly_; }

	/* View: TextEditorView.cpp */

	/// @brief Wraps long lines at the panel's width (at a space when one fits), or scrolls them sideways.
	/// @param wrap true to wrap.
	void setWrap(bool wrap);
	/// @brief Whether long lines wrap.
	[[nodiscard]] bool wrap() const noexcept { return wrap_; }
	/// @brief The lines indented more than a line, after it: the range fold() would hide.
	/// @param line The line.
	/// @return The range, or nothing when no line after it is indented more (or it is past the end).
	[[nodiscard]] std::optional<FoldRange> foldable(std::size_t line) const;
	/// @brief Hides the lines indented more than a line, after it.
	/// @param line The line.
	/// @return Whether anything was folded.
	bool fold(std::size_t line);
	/// @brief Shows the lines folded under a line again.
	/// @param line The line.
	/// @return Whether a fold was there.
	bool unfold(std::size_t line);
	/// @brief Folds a line, or unfolds it if it is folded.
	/// @param line The line.
	void toggleFold(std::size_t line);
	/// @brief Replaces the folds (out-of-range and empty ones are dropped, at most maxFolds kept).
	/// @param folds The ranges, in any order; they may nest.
	void setFolds(std::vector<FoldRange> folds);
	/// @brief Sets the folds a document gives, only when they differ from those it gave last (so folding by the user
	/// between two updates of the document stays).
	/// @param folds As for setFolds().
	void offerFolds(std::vector<FoldRange> folds);
	/// @brief Shows every line again.
	void unfoldAll();
	/// @brief The folded ranges, by first line.
	[[nodiscard]] const std::vector<FoldRange>& folds() const noexcept { return folds_; }
	/// @brief The rows shown: the lines, plus the extra rows of wrapped lines, minus the folded lines.
	[[nodiscard]] std::size_t rowCount() const noexcept;
	/// @brief Sets the scroll offset (kept inside the content), e.g. to follow another editor.
	/// @param offset Pixels scrolled right and down.
	void setScroll(Vec2 offset);
	/// @brief Pixels scrolled right and down.
	[[nodiscard]] Vec2 scroll() const noexcept { return scroll_; }
	/// @brief Shows text being composed by an input method at the caret, underlined, without editing (empty: none).
	/// @param text The composed text (at most maxCompositionLength code points are shown).
	void setComposition(std::u32string text);
	/// @brief Shows the matching bracket of one next to the caret.
	/// @param shown true to show it.
	void setBracketMatching(bool shown);
	/// @brief Draws a thin line at each indentation level.
	/// @param shown true to draw them.
	void setIndentGuides(bool shown);

	/* Decorations and cursors: TextEditorCursors.cpp */

	/// @brief Replaces the decorations: at most maxDecorations, and maxDecorationsPerLine starting on one line (the
	/// rest are dropped). They move with the lines as the text is edited.
	/// @param decorations The decorations, in any order.
	void setDecorations(std::vector<EditorDecoration> decorations);
	/// @brief The decorations, by first line.
	[[nodiscard]] const std::vector<EditorDecoration>& decorations() const noexcept { return decorations_; }
	/// @brief Adds a cursor (with its selection) to the caret: typing, deleting and moving act on all of them.
	/// @param selection Its anchor and caret (clamped to the text).
	/// @return Whether it was added (not past maxCursors, and not where a cursor is).
	bool addCursor(EditorSelection selection);
	/// @brief The cursors besides the caret, in text order.
	[[nodiscard]] const std::vector<EditorSelection>& cursors() const noexcept { return cursors_; }
	/// @brief Leaves only the caret.
	void clearCursors();

	/* Find and replace: TextEditorFind.cpp */

	/// @brief Highlights the matches of a pattern on the lines drawn (empty: none).
	/// @param text The text, or a regular expression.
	/// @param options Plain, whole word or regular expression; the case.
	/// @return An error for a pattern that doesn't compile (then nothing is highlighted).
	std::optional<std::string> setFind(std::string_view text, PatternOptions options);
	/// @brief Selects the next match after the caret (or before it), wrapping around the text.
	/// @param backwards true for the previous one.
	/// @return Whether one was found.
	bool findNext(bool backwards = false);
	/// @brief Replaces the selected match (then selects the next one), or finds the next one.
	/// @param replacement The text; with a regular expression, $1 and ${name} insert its groups.
	/// @return Whether something was replaced.
	bool replaceNext(std::string_view replacement);
	/// @brief Replaces every match, at most maxMatches, as one undo step.
	/// @param replacement As for replaceNext().
	/// @return How many were replaced.
	std::size_t replaceAll(std::string_view replacement);
	/// @brief How many matches the text has, counted up to maxMatches.
	[[nodiscard]] std::size_t matchCount() const;

	/// @brief Sets the font.
	/// @param font The font, or nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief The font in use: its own, else the window's default, else nullptr (nothing is drawn).
	[[nodiscard]] const Font* font() const noexcept;

	/// @brief Shows or hides the line numbers.
	/// @param shown true to show them in a gutter on the left.
	void setLineNumbers(bool shown);
	/// @brief Sets the numbers the gutter shows instead of each line's own (a diff shows each text's numbers).
	/// @param numbers One per line, from line 0 (0: none shown); lines past the end show their own. Empty: all do.
	void setLineLabels(std::vector<std::size_t> numbers);
	/// @brief Sets the text colour (what colorLine() gives by default).
	/// @param color The colour.
	void setTextColor(Color color);
	/// @brief The text colour.
	[[nodiscard]] Color textColor() const noexcept { return textColor_; }
	/// @brief Sets the colour of the line numbers.
	/// @param color The colour.
	void setLineNumberColor(Color color);
	/// @brief Sets the colour of the caret's line number.
	/// @param color The colour.
	void setActiveLineNumberColor(Color color);
	/// @brief Sets the background of the caret's line (drawn while nothing is selected).
	/// @param color The colour; transparent for none.
	void setLineColor(Color color);
	/// @brief Sets the background of the selected text.
	/// @param color The colour.
	void setSelectionColor(Color color);
	/// @brief Sets the caret's colour.
	/// @param color The colour.
	void setCaretColor(Color color);
	/// @brief Restores the text, line number, caret line, selection and caret colours to their defaults.
	void resetColors();
	/// @brief Sets the colour of the indentation guides.
	/// @param color The colour.
	void setGuideColor(Color color);
	/// @brief Sets the background of find matches.
	/// @param color The colour.
	void setMatchColor(Color color);
	/// @brief Sets the background of matching brackets.
	/// @param color The colour.
	void setBracketColor(Color color);

	/// @brief Where the caret is.
	[[nodiscard]] Position caret() const noexcept { return caret_; }
	/// @brief The selection's other end (the caret when nothing is selected).
	[[nodiscard]] Position anchor() const noexcept { return anchor_; }
	/// @brief How many code points are selected (line breaks count as one).
	[[nodiscard]] std::size_t selectedLength() const;
	/// @brief The text, one element per line (at least one), decoded when read.
	[[nodiscard]] const TextLines& lines() const noexcept { return lines_; }

	Signal<const std::string&> onTextChanged; ///< The user edited the text (not setText()); UTF-8.
	Signal<Position> onCaretMoved;            ///< The caret or the selection changed, by the user or by an edit.
	Signal<Vec2> onScrolled;                  ///< The scroll offset changed (by the wheel, the caret or setScroll()).

protected:
	/// @brief What an edit was, so consecutive typing can be undone in one step.
	enum class Edit {
		Other,    ///< Its own undo step.
		Typing,   ///< Merged with the typing just before it.
		Deleting, ///< Merged with the deleting just before it.
	};

	void keyEvent(const KeyEvent& event) override;
	void charEvent(const CharEvent& event) override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void scrollEvent(const ScrollEvent& event) override;
	void focusEvent(bool focused) override;
	void arrange() override;
	void paint(Painter& painter) const override;

	/// @brief Colours one line; called for each drawn line, top to bottom. The state each line starts with is kept, so
	/// lines above the drawn ones are coloured once, and again only after an edit above them or recolor(). The default
	/// gives every code point textColor().
	/// @param line The text of the line.
	/// @param state Carried from one line to the next (e.g. "inside a block comment"); 0 at the first line.
	/// @param colors One colour per code point, to fill (already sized to the line).
	virtual void colorLine(std::u32string_view line, int& state, std::vector<Color>& colors) const;

	/// @brief Colours every line again: call it when colorLine() would give other results (e.g. a new language).
	void recolor();

	/// @brief Gives the gutter's label of a line (by default its number, from 1).
	/// @param line The line.
	/// @param out Gets the label (empty: none).
	virtual void lineLabel(std::size_t line, std::u32string& out) const;

	/// @brief Enter: replaces the selection with a new line that keeps the indentation. Subclasses may override it.
	virtual void newLine();

	/* Editing API for subclasses: call beginEdit(), change the text, set the selection, then endEdit() */

	/// @brief Starts an undo step, keeping the caret and selection to restore.
	/// @param kind The edit; consecutive typing (or deleting) is one undo step.
	void beginEdit(Edit kind);
	/// @brief Finishes an edit: ends the undo step, reveals the caret, fires the signals.
	void endEdit();
	/// @brief The text between two positions.
	/// @param from The start.
	/// @param to The end (not before @p from).
	[[nodiscard]] std::u32string textIn(Position from, Position to) const;
	/// @brief Deletes the text between two positions.
	/// @param from The start.
	/// @param to The end (not before @p from).
	void erase(Position from, Position to);
	/// @brief Inserts text, which may contain "\n".
	/// @param at Where.
	/// @param text The text.
	/// @return The position after it.
	Position insert(Position at, std::u32string_view text);
	/// @brief Replaces the selection (or inserts at the caret) and puts the caret after the new text.
	/// @param text The text.
	void replaceSelection(std::u32string_view text);
	/// @brief Sets the selection without an edit (no undo step, no signal; endEdit() or moveCaret() fire it).
	/// @param anchor The fixed end.
	/// @param caret The moving end.
	void setSelection(Position anchor, Position caret) noexcept;
	/// @brief Moves the caret outside an edit, revealing it and firing onCaretMoved.
	/// @param to The new position.
	/// @param extend true to keep the selection's anchor (Shift held).
	/// @param keepX true to keep the column the caret aims for (vertical moves).
	void moveCaret(Position to, bool extend, bool keepX = false);
	/// @brief The selection, start first.
	[[nodiscard]] std::pair<Position, Position> selection() const noexcept;
	/// @brief Whether anything is selected.
	[[nodiscard]] bool hasSelection() const noexcept { return caret_ != anchor_; }
	/// @brief The first and last line the selection (or the caret) touches.
	[[nodiscard]] std::pair<std::size_t, std::size_t> selectedLines() const noexcept;
	/// @brief Leading spaces of a line.
	/// @param line The line.
	[[nodiscard]] std::size_t indentOf(std::size_t line) const;
	/// @brief Indents (or outdents) the selected lines by one level, as one edit.
	/// @param outdent true to remove a level.
	void indentLines(bool outdent);

private:
	static constexpr float textPadding = 6.0f;    ///< Space above the first line, in pixels.
	static constexpr float scrollBarWidth = 4.0f; ///< Thickness of the scroll bar, in pixels.
	static constexpr std::size_t exactWidth = 4096; ///< Up to this many lines, the widest is measured, not estimated.

	/// @brief One row on screen: a line, or the part of a wrapped line between two columns.
	struct RowSpan {
		std::size_t line = 0;  ///< The line.
		std::size_t start = 0; ///< Its first column.
		std::size_t end = 0;   ///< One past its last column (the line's length on its last row).
	};

	/// @brief Keeps the colouring states, the widest line, the rows and what moves with the text in step with an edit.
	/// @param change The edit.
	void textChanged(const TextChange& change);
	/// @brief The widest line, in pixels: measured for up to exactWidth lines, else estimated from the longest line's
	/// bytes, then grown by the lines edited since.
	/// @param font The font.
	[[nodiscard]] float widestLine(const Font& font) const;
	/// @brief Whether the text can be edited now: not read-only.
	[[nodiscard]] bool editable() const noexcept { return !readOnly_; }

	/// @brief The height of one line, in pixels.
	[[nodiscard]] float lineHeight() const noexcept;
	/// @brief The gutter's width: the line numbers and the space after them, or a small margin without them.
	[[nodiscard]] float gutterWidth() const noexcept;
	/// @brief Where a column starts on a line, relative to the start of the text.
	/// @param line The line.
	/// @param column The column.
	[[nodiscard]] float columnX(std::size_t line, std::size_t column) const;
	/// @brief The column nearest to an x position on a line.
	/// @param line The line.
	/// @param x Relative to the start of the text.
	[[nodiscard]] std::size_t columnAt(std::size_t line, float x) const;
	/// @brief The text position under a point.
	/// @param point In window units.
	[[nodiscard]] Position positionAt(Vec2 point) const;
	/// @brief How many whole rows fit in the panel.
	[[nodiscard]] std::size_t visibleLines() const noexcept;
	/// @brief Keeps the scroll offset inside the content.
	void clampScroll();
	/// @brief Scrolls so the caret is visible.
	void revealCaret();
	/// @brief The position one code point, or one word, to the left or right.
	/// @param from Where to start.
	/// @param forward true for right.
	/// @param word true to jump a word.
	[[nodiscard]] Position step(Position from, bool forward, bool word) const;
	/// @brief Undoes (or redoes) the newest step.
	/// @param redo true to redo.
	void undo(bool redo);
	/// @brief Cmd+C or Cmd+X: copies the selection to the clipboard.
	/// @param cut true to delete it too.
	void copy(bool cut);
	/// @brief Cmd+V: inserts the clipboard's text.
	void paste();
	/// @brief Selects the word (or run of spaces or symbols) around a position.
	/// @param at The position.
	void selectWord(Position at);
	/// @brief Moves the caret by rows, keeping the x it aims for (Up, Down, Page Up, Page Down).
	/// @param rows Rows down (negative: up).
	/// @param extend true to keep the selection's anchor.
	void moveRows(long rows, bool extend);
	/// @brief Clamps a position to the text.
	/// @param at The position.
	[[nodiscard]] Position clamped(Position at) const noexcept;

	/* Rows (TextEditorView.cpp): with neither wrap nor folds, a row is a line and rows_ is null */

	/// @brief Whether rows differ from lines: wrap or folds are on.
	[[nodiscard]] bool viewActive() const noexcept { return rows_ != nullptr; }
	/// @brief The line and columns of a row.
	/// @param row The row, below rowCount().
	[[nodiscard]] RowSpan rowSpan(std::size_t row) const;
	/// @brief The row a position is on (a folded line's: the fold's shown line).
	/// @param at The position.
	[[nodiscard]] std::size_t rowOf(Position at) const;
	/// @brief Where each row of a line starts, when wrapped: {0} for a line on one row.
	/// @param line The line.
	/// @param starts Gets the columns.
	void lineBreaks(std::size_t line, std::vector<std::size_t>& starts) const;
	/// @brief The width a wrapped row may take, in pixels.
	[[nodiscard]] float wrapWidth() const noexcept;
	/// @brief The rows a line takes when wrapped.
	/// @param line The line.
	[[nodiscard]] std::uint32_t wrappedRows(std::size_t line) const;
	/// @brief Works out every line's rows again (wrap, folds or the width changed), or drops the rows.
	void rebuildRows();
	/// @brief Keeps the rows in step with an edit.
	/// @param change The edit.
	void rowsChanged(const TextChange& change);
	/// @brief Whether a line is hidden in a fold.
	/// @param line The line.
	[[nodiscard]] bool hidden(std::size_t line) const noexcept;
	/// @brief The first line after a line's fold, if it starts one that is folded (else line + 1).
	/// @param line The line.
	[[nodiscard]] std::size_t nextShown(std::size_t line) const noexcept;
	/// @brief Where a position is on its row, relative to the start of the text.
	/// @param at The position.
	[[nodiscard]] float xOnRow(Position at) const;
	/// @brief The position nearest an x on a row.
	/// @param row The row.
	/// @param x Relative to the start of the text.
	[[nodiscard]] Position positionOnRow(std::size_t row, float x) const;
	/// @brief Unfolds the folds that hide a line (the caret went there).
	/// @param line The line.
	void unfoldAround(std::size_t line);
	/// @brief Moves folds and decorations with an edit, and drops what it removed.
	/// @param change The edit.
	void shiftView(const TextChange& change);

	/* Cursors (TextEditorCursors.cpp) */

	/// @brief Where a position goes after an edit.
	/// @param at The position.
	/// @param change The edit.
	[[nodiscard]] static Position shifted(Position at, const TextChange& change) noexcept;
	/// @brief Runs an edit at the caret and at each extra cursor, as one undo step, then merges cursors that meet.
	/// @param kind The edit.
	/// @param body Edits at caret_ and anchor_ (each cursor in turn is put there).
	void forEachCursor(Edit kind, const std::function<void()>& body);
	/// @brief Moves every extra cursor as a caret move would (arrows), then merges those that meet.
	/// @param move Gives the new selection of one.
	void moveCursors(const std::function<EditorSelection(EditorSelection)>& move);
	/// @brief Sorts the extra cursors, drops those at the caret's place and merges those that overlap.
	void mergeCursors();
	/// @brief Handles the keys of the view: extra cursors, Escape, folding.
	/// @param event The key.
	/// @return Whether it was handled.
	bool viewKey(const KeyEvent& event);
	/// @brief Typing keys with extra cursors: characters, Enter, Tab, Backspace and Delete at each.
	/// @param event The key.
	/// @return Whether it was handled.
	bool cursorsKey(const KeyEvent& event);
	/// @brief Cmd+D: selects the word at the caret, or adds a cursor at the next occurrence of the selection.
	void selectNextOccurrence();
	/// @brief Cmd+Alt+Up/Down: adds a cursor on the row above or below the outermost one.
	/// @param down true for below.
	void addCursorRow(bool down);
	/// @brief Alt+Shift+drag: one selection per line between two points, between their x positions.
	/// @param from Where the drag started.
	/// @param to Where the pointer is.
	void selectColumn(Vec2 from, Vec2 to);

	/* Find (TextEditorFind.cpp) */

	/// @brief Finds the matches on one line, from a column.
	/// @param line The line.
	/// @param from The first column.
	/// @param cap At most this many.
	/// @param out Gets each match's column and length, in code points.
	void lineMatches(std::size_t line, std::size_t from, std::size_t cap,
	                 std::vector<std::pair<std::size_t, std::size_t>>& out) const;
	/// @brief Keeps visibleMatches_ the matches of the rows drawn.
	/// @param firstLine The first line drawn.
	/// @param lastLine One past the last.
	void findVisibleMatches(std::size_t firstLine, std::size_t lastLine) const;
	/// @brief The bracket next to the caret and its match, when bracket matching is on.
	[[nodiscard]] std::optional<std::pair<Position, Position>> matchingBrackets() const;

	/// @brief Where paint() draws, worked out once per frame.
	struct Layout {
		float lineHeight;  ///< See lineHeight().
		float gutter;      ///< See gutterWidth().
		float textLeft;    ///< Where the text's columns start, scrolled.
		float firstTop;    ///< Where the first row's top is, scrolled.
		float baseline;    ///< From a row's top to its baseline.
		std::size_t first; ///< The first row drawn.
		std::size_t last;  ///< One past the last.
	};
	/// @brief The top of a row, in whole pixels.
	/// @param layout The frame's layout.
	/// @param row The row.
	[[nodiscard]] static float lineTop(const Layout& layout, std::size_t row) noexcept;
	/// @brief Draws one row: its selections, then its text in runs of one colour.
	/// @param painter The painter.
	/// @param font The font.
	/// @param layout The frame's layout.
	/// @param span The row.
	/// @param row Its index.
	void paintLine(Painter& painter, const Font& font, const Layout& layout, RowSpan span, std::size_t row) const;
	/// @brief Draws the selected part of a row, if any of it is.
	/// @param painter The painter.
	/// @param font The font.
	/// @param layout The frame's layout.
	/// @param span The row.
	/// @param row Its index.
	/// @param selection The selection, start first.
	void paintSelection(Painter& painter, const Font& font, const Layout& layout, RowSpan span, std::size_t row,
	                    std::pair<Position, Position> selection) const;
	/// @brief Draws a range of columns of a row as a rectangle, or several where the text's direction changes.
	/// @param painter The painter.
	/// @param layout The frame's layout.
	/// @param span The row.
	/// @param row Its index.
	/// @param columns The columns, first first; a second past the row's end adds a space's width.
	/// @param color The colour.
	void paintColumns(Painter& painter, const Layout& layout, RowSpan span, std::size_t row,
	                  std::pair<std::size_t, std::size_t> columns, Color color) const;
	/// @brief Draws the gutter over the text scrolled under it, the line labels, fold markers and gutter icons.
	/// @param painter The painter.
	/// @param font The font.
	/// @param layout The frame's layout.
	void paintGutter(Painter& painter, const Font& font, const Layout& layout) const;
	/// @brief Draws the caret, when focused, and the scroll bar's thumb.
	/// @param painter The painter.
	/// @param layout The frame's layout.
	void paintCaretAndScrollBar(Painter& painter, const Layout& layout) const;
	/// @brief Draws what goes behind the text: background decorations, find matches, brackets, extra selections.
	/// @param painter The painter.
	/// @param font The font.
	/// @param layout The frame's layout.
	void paintUnder(Painter& painter, const Font& font, const Layout& layout) const;
	/// @brief Draws what goes over the text: squiggles, inline text, guides, extra carets and the composition.
	/// @param painter The painter.
	/// @param font The font.
	/// @param layout The frame's layout.
	void paintAbove(Painter& painter, const Font& font, const Layout& layout) const;
	/// @brief Calls a function for each row drawn that a range of lines shows.
	/// @param layout The frame's layout.
	/// @param from The first line.
	/// @param to The last line.
	/// @param body Gets each row and its span.
	void forRowsOf(const Layout& layout, std::size_t from, std::size_t to,
	               const std::function<void(std::size_t, RowSpan)>& body) const;

	/// @brief The text; shared, as a file buffer may be shown by several editors at once (setModel()).
	std::shared_ptr<TextModel> model_ = std::make_shared<TextModel>();
	TextLines lines_{*model_};                            ///< See lines().
	Connection modelConnection_ = 0;                      ///< To model_->onChanged.
	Position caret_;                                      ///< Where typing happens.
	Position anchor_;                                     ///< The selection's other end (== caret_: none).
	float preferredX_ = -1.0f;                            ///< The x vertical moves aim for (< 0: the caret's).
	Vec2 scroll_;                                         ///< Pixels scrolled right and down.
	bool dragging_ = false;                               ///< A button press is selecting text.
	double lastClickTime_ = -1.0;                         ///< When the last press happened, for double clicks.
	Position lastClickAt_;                                ///< Where it happened.
	int clicks_ = 0;                                      ///< Presses in a row at one place (1, 2 or 3).
	bool editing_ = false;  ///< This editor is changing the model: its caret is set by the edit, not moved.
	bool readOnly_ = false; ///< See setReadOnly().

	Edit lastEdit_ = Edit::Other; ///< The edit that made the newest undo step (Other: start a new step).
	mutable std::vector<int> states_{0}; ///< The colouring state each line starts with, for the lines coloured so far.
	mutable float widest_ = -1.0f;       ///< See widestLine(); < 0: to work out.
	mutable const Font* widestFont_ = nullptr; ///< The font widest_ was measured with.
	mutable std::vector<Color> colors_;        ///< One line's colours, reused from frame to frame.
	mutable std::vector<std::tuple<std::size_t, float, float>> runs_; ///< One row's colour runs: end, x and width.
	mutable std::vector<std::size_t> breaks_;  ///< One line's row starts, reused.
	mutable std::u32string label_;             ///< One line's gutter label, reused.

	bool wrap_ = false;                        ///< See setWrap().
	std::vector<FoldRange> folds_;             ///< See folds().
	std::vector<FoldRange> givenFolds_;        ///< See offerFolds().
	std::unique_ptr<detail::ViewRows> rows_;   ///< The rows of each line, while wrap or folds are on.
	std::vector<std::uint32_t> wrapRows_;      ///< Each line's wrapped rows (folds aside), while wrapping.
	float rowsWidth_ = -1.0f;                  ///< The width wrapRows_ was worked out for.
	const Font* rowsFont_ = nullptr;           ///< The font it was worked out with.
	std::vector<EditorDecoration> decorations_; ///< See decorations().
	std::size_t decorationSpan_ = 0;           ///< The most lines one decoration spans, to find those drawn.
	std::vector<EditorSelection> cursors_;     ///< See cursors().
	std::vector<EditorSelection>* editedCursors_ = nullptr; ///< In forEachCursor(): every cursor, moved by edits.
	std::size_t activeCursor_ = SIZE_MAX;      ///< In forEachCursor(): the one being edited (not moved).
	bool columnDragging_ = false;              ///< Alt+Shift+drag is selecting a column.
	Vec2 columnFrom_;                          ///< Where it started.

	TextPattern find_;                ///< The find pattern; invalid: none.
	std::string findText_;            ///< Its text.
	PatternOptions findOptions_;      ///< Its options.
	std::uint64_t findGeneration_ = 0; ///< Counts setFind() calls, for the cached matches.
	/// @brief The matches of the rows drawn last: line, column and length.
	struct VisibleMatch {
		std::size_t line;   ///< The line.
		std::size_t column; ///< The first code point.
		std::size_t length; ///< Code points.
	};
	mutable std::vector<VisibleMatch> visibleMatches_;                ///< See VisibleMatch.
	mutable std::vector<std::pair<std::size_t, std::size_t>> found_; ///< One line's matches, reused.
	mutable std::string subject_;                                     ///< One line in UTF-8, reused.
	/// @brief What visibleMatches_ was found for: the model's version, the lines and findGeneration_.
	mutable std::array<std::uint64_t, 4> matchesKey_{};
	bool brackets_ = false;           ///< See setBracketMatching().
	bool guides_ = false;             ///< See setIndentGuides().
	std::u32string composition_;      ///< See setComposition().

	std::shared_ptr<const Font> font_; ///< nullptr: the window's default.
	bool lineNumbers_ = false;         ///< See setLineNumbers().
	std::vector<std::size_t> lineLabels_; ///< See setLineLabels().
	std::size_t largestLabel_ = 0;        ///< The largest of lineLabels_, for the gutter's width.
	Color textColor_;                  ///< See setTextColor().
	Color lineNumberColor_;            ///< See setLineNumberColor().
	Color activeLineNumberColor_;      ///< See setActiveLineNumberColor().
	Color lineColor_;                  ///< See setLineColor().
	Color selectionColor_;             ///< See setSelectionColor().
	Color caretColor_;                 ///< See setCaretColor().
	Color guideColor_;                 ///< See setGuideColor().
	Color matchColor_;                 ///< See setMatchColor().
	Color bracketColor_;               ///< See setBracketColor().
};

} // namespace affresco
