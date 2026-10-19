#pragma once

/// @file
/// @brief TextEditor: a multi-line text editor, with optional line numbers, that applications can extend.

#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"

#include <compare>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

/// @brief A multi-line text editor, with the usual keys (Cmd on macOS, Ctrl elsewhere).
///
/// Click to place the caret, drag to select (double click: a word, triple: a line). Arrows, Home/End, Page Up/Down;
/// Alt+arrows jump words, Cmd+arrows go to line or document ends; Shift extends the selection. Typing replaces the
/// selection; Enter keeps the line's indentation; Tab and Shift+Tab indent; Cmd+A/C/X/V/Z, Cmd+Shift+Z. Draws optional
/// line numbers in a gutter, highlights the caret's line and scrolls with the wheel. Tabs become spaces; the caret
/// doesn't blink, so an idle editor costs nothing.
///
/// Applications extend it by subclassing: colorLine() colours the text (e.g. syntax), and the event hooks can add
/// commands with the protected editing API (beginEdit(), the text operations, endEdit()), which keeps undo working.
class TextEditor : public Panel {
public:
	/// @brief A place in the text: a line and a code point within it, both from 0.
	struct Position {
		std::size_t line = 0;   ///< The line.
		std::size_t column = 0; ///< Code points before it on the line.

		/// @brief Ordered as in the text: by line, then by column.
		constexpr auto operator<=>(const Position&) const noexcept = default;
	};

	static constexpr std::size_t tabSize = 4; ///< Spaces per indentation level.

	/// @brief An empty editor with a dark background and an I-beam cursor.
	TextEditor();

	/// @brief Replaces the text, without firing onTextChanged, unless it is the same. The caret stays where it was if
	/// the line still exists; the undo history is kept, so an outside change can be undone like a typed one.
	/// @param utf8 UTF-8 text, lines separated by "\n" ("\r\n" too); tabs become spaces.
	void setText(std::string_view utf8);
	/// @brief The text, as UTF-8 with "\n" between lines.
	[[nodiscard]] const std::string& text() const noexcept { return utf8_; }

	/// @brief Sets the font.
	/// @param font The font, or nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief The font in use: its own, else the window's default, else nullptr (nothing is drawn).
	[[nodiscard]] const Font* font() const noexcept;

	/// @brief Shows or hides the line numbers.
	/// @param shown true to show them in a gutter on the left.
	void setLineNumbers(bool shown);
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

	/// @brief Where the caret is.
	[[nodiscard]] Position caret() const noexcept { return caret_; }
	/// @brief The selection's other end (the caret when nothing is selected).
	[[nodiscard]] Position anchor() const noexcept { return anchor_; }
	/// @brief How many code points are selected (line breaks count as one).
	[[nodiscard]] std::size_t selectedLength() const;
	/// @brief The text, one element per line (at least one).
	[[nodiscard]] const std::vector<std::u32string>& lines() const noexcept { return lines_; }

	Signal<const std::string&> onTextChanged; ///< The user edited the text (not setText()); UTF-8.
	Signal<Position> onCaretMoved;            ///< The caret or the selection changed, by the user or by an edit.

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

	/// @brief Colours one line; called for each drawn line, top to bottom, starting from the first line of the text.
	/// The default gives every code point textColor().
	/// @param line The text of the line.
	/// @param state Carried from one line to the next (e.g. "inside a block comment"); 0 at the first line.
	/// @param colors One colour per code point, to fill (already sized to the line).
	virtual void colorLine(std::u32string_view line, int& state, std::vector<Color>& colors) const;

	/// @brief Enter: replaces the selection with a new line that keeps the indentation. Subclasses may override it.
	virtual void newLine();

	/* Editing API for subclasses: call beginEdit(), change the text, set the selection, then endEdit() */

	/// @brief Remembers the text before an edit, so it can be undone.
	/// @param kind The edit; consecutive typing (or deleting) is one undo step.
	void beginEdit(Edit kind);
	/// @brief Finishes an edit: updates the UTF-8 copy, reveals the caret, fires the signals.
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
	[[nodiscard]] std::size_t indentOf(std::size_t line) const noexcept;
	/// @brief Indents (or outdents) the selected lines by one level, as one edit.
	/// @param outdent true to remove a level.
	void indentLines(bool outdent);

private:
	/// @brief The text, caret and selection, for undo and redo.
	struct Snapshot {
		std::vector<std::u32string> lines; ///< The text.
		Position caret;                    ///< The caret.
		Position anchor;                   ///< The selection's other end.
	};

	/// @brief The height of one line, in pixels.
	[[nodiscard]] float lineHeight() const noexcept;
	/// @brief The gutter's width: the line numbers and the space after them, or a small margin without them.
	[[nodiscard]] float gutterWidth() const noexcept;
	/// @brief Where a column starts on a line, relative to the start of the text.
	/// @param line The line.
	/// @param column The column.
	[[nodiscard]] float columnX(std::size_t line, std::size_t column) const noexcept;
	/// @brief The column nearest to an x position on a line.
	/// @param line The line.
	/// @param x Relative to the start of the text.
	[[nodiscard]] std::size_t columnAt(std::size_t line, float x) const noexcept;
	/// @brief The text position under a point.
	/// @param point In window units.
	[[nodiscard]] Position positionAt(Vec2 point) const noexcept;
	/// @brief How many whole lines fit in the panel.
	[[nodiscard]] std::size_t visibleLines() const noexcept;
	/// @brief Keeps the scroll offset inside the content.
	void clampScroll() noexcept;
	/// @brief Scrolls so the caret is visible.
	void revealCaret() noexcept;
	/// @brief The position one code point, or one word, to the left or right.
	/// @param from Where to start.
	/// @param forward true for right.
	/// @param word true to jump a word.
	[[nodiscard]] Position step(Position from, bool forward, bool word) const noexcept;
	/// @brief Swaps the text with the last undo (or redo) snapshot.
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

	std::vector<std::u32string> lines_{std::u32string()}; ///< See lines().
	std::string utf8_;                                    ///< See text().
	Position caret_;                                      ///< Where typing happens.
	Position anchor_;                                     ///< The selection's other end (== caret_: none).
	float preferredX_ = -1.0f;                            ///< The x vertical moves aim for (< 0: the caret's).
	Vec2 scroll_;                                         ///< Pixels scrolled right and down.
	bool dragging_ = false;                               ///< A button press is selecting text.
	double lastClickTime_ = -1.0;                         ///< When the last press happened, for double clicks.
	Position lastClickAt_;                                ///< Where it happened.
	int clicks_ = 0;                                      ///< Presses in a row at one place (1, 2 or 3).

	std::vector<Snapshot> undo_;  ///< Older texts, newest last.
	std::vector<Snapshot> redo_;  ///< Undone texts, newest last.
	Edit lastEdit_ = Edit::Other; ///< The edit that made the newest undo snapshot (Other: start a new step).

	std::shared_ptr<const Font> font_; ///< nullptr: the window's default.
	bool lineNumbers_ = false;         ///< See setLineNumbers().
	Color textColor_;                  ///< See setTextColor().
	Color lineNumberColor_;            ///< See setLineNumberColor().
	Color activeLineNumberColor_;      ///< See setActiveLineNumberColor().
	Color lineColor_;                  ///< See setLineColor().
	Color selectionColor_;             ///< See setSelectionColor().
	Color caretColor_;                 ///< See setCaretColor().
};

} // namespace affresco
