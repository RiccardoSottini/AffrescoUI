#pragma once

/// @file
/// @brief CodeEditor: an application-layer widget for the Forge template (code-editor.screen.json), built on the
/// library's TextEditor. It shows how an application adds its own type: subclass a widget, register it with
/// registerCodeEditor().

#include <affresco/affresco.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace examples {

/// @brief A TextEditor for code: syntax colours and the commands of common code editors.
///
/// On top of TextEditor: colours C++, JSON, CMake and Markdown; Enter adds a level after `{`, `[`, `(` or `:` and puts
/// a closing bracket on its own line; brackets and quotes close themselves (typing the closer steps over it, Backspace
/// deletes an empty pair, a selection gets wrapped); Cmd+/ toggles line comments; Alt+Up/Down moves lines; Cmd+Shift+K
/// deletes lines; Cmd+[ and Cmd+] indent; without a selection, Cmd+C and Cmd+X take the whole line.
class CodeEditor : public affresco::TextEditor {
public:
	/// @brief The kinds of text the syntax colouring tells apart (plain text uses textColor()).
	enum class Token {
		Plain,   ///< Anything else.
		Keyword, ///< Language keywords, preprocessor lines, JSON literals, CMake commands.
		Type,    ///< Type names, JSON keys, CMake variables and options.
		String,  ///< Quoted text, Markdown code.
		Number,  ///< Numeric literals.
		Comment, ///< Comments.
		Heading, ///< Markdown headings.
	};

	/// @brief The languages it colours.
	enum class Language { Plain, Cpp, Json, CMake, Markdown };

	/// @brief An editor with default syntax colours and no language.
	CodeEditor();

	/// @brief Sets which syntax to colour.
	/// @param name "c++" (or "cpp", "c"), "json", "cmake", "markdown" (or "md"); anything else is plain. Any case.
	void setLanguage(std::string_view name);

	/// @brief Sets the colour of one kind of text.
	/// @param token The kind (not Token::Plain, which is the text colour).
	/// @param color The colour.
	void setTokenColor(Token token, affresco::Color color);

	/// @brief Restores the default syntax colours.
	void resetTokenColors();

	/// @brief Moves the caret to the start of a line and scrolls to it, once per request (a "go to line" command).
	/// @param line The line, from 1; past the last line, the last.
	/// @param request Which request this is: the same one again does nothing, so reapplied properties don't move it.
	void goToLine(std::size_t line, long long request);

protected:
	/// @brief The editor's shortcuts (runShortcut()) and Backspace in an empty pair, then TextEditor's keys.
	/// @param event The key event.
	void keyEvent(const affresco::KeyEvent& event) override;
	/// @brief Typing with pairs: steps over a closer, wraps the selection, closes a pair (pairTypingFor()); a closing
	/// brace on a blank line goes back one level.
	/// @param event The typed code point.
	void charEvent(const affresco::CharEvent& event) override;
	/// @brief Colours a line by its tokens (tokenize()); `state` is 1 while a block comment is open.
	/// @param line The line.
	/// @param state In: the state at its start; out: at its end.
	/// @param colors One per code point, filled in.
	void colorLine(std::u32string_view line, int& state, std::vector<affresco::Color>& colors) const override;
	/// @brief Enter: after `{`, `[`, `(` or `:` the new line is indented one level more, and a closing bracket right
	/// after the caret goes on a line of its own.
	void newLine() override;

private:
	/// @brief What typing a code point does to a bracket or quote pair (see pairTypingFor()).
	enum class PairTyping {
		None,     ///< Nothing special: TextEditor types it.
		StepOver, ///< A closer that is already next: the caret steps over it.
		Wrap,     ///< An opener with a selection: the selection gets wrapped in the pair.
		Close,    ///< An opener: the pair goes in, with the caret inside.
	};

	/// @brief The editor's own shortcuts: Alt+Up/Down, Cmd+/, Cmd+Shift+K, Cmd+[ and Cmd+], Cmd+C and Cmd+X without a
	/// selection.
	/// @param event A key press or repeat.
	/// @return true if it ran one.
	bool runShortcut(const affresco::KeyEvent& event);
	/// @brief The shortcuts with Cmd (Ctrl elsewhere): /, Shift+K, [ and ], C and X without a selection.
	/// @param key The key, pressed with Cmd.
	/// @param shift Whether Shift was held too.
	/// @return true if it ran one.
	bool runCommandShortcut(affresco::Key key, bool shift);
	/// @brief Cmd+C or Cmd+X without a selection: the caret's whole line, with its line break, goes to the clipboard.
	/// @param cut true to delete the line too.
	/// @return false, doing nothing, if there is a selection or no window.
	bool copyLine(bool cut);
	/// @brief Backspace inside an empty pair, e.g. "(|)": deletes both.
	/// @return false, doing nothing, if there is a selection or the caret isn't inside an empty pair.
	bool deleteEmptyPair();
	/// @brief What typing a code point at the caret does to a pair.
	/// @param typed The code point.
	[[nodiscard]] PairTyping pairTypingFor(char32_t typed) const;
	/// @brief Wraps the selection in a pair and keeps the wrapped text selected.
	/// @param opener The opening bracket or quote.
	void wrapSelection(char32_t opener);
	/// @brief Types a pair in place of the selection, with the caret between the two.
	/// @param opener The opening bracket or quote.
	/// @param at The caret before it was typed.
	void closePair(char32_t opener, Position at);
	/// @brief After a closing brace typed on a blank line: takes the line back one level.
	/// @param typed The code point just typed.
	void outdentClosingBrace(char32_t typed);
	/// @brief Cmd+/: comments the selected lines out, or back in if they all are.
	void toggleComment();
	/// @brief Where toggleComment() puts the markers: the smallest indentation of the non-blank lines.
	/// @param first The first line.
	/// @param last The last line.
	/// @return The column, or std::nullopt if every line is blank.
	[[nodiscard]] std::optional<std::size_t> commentColumn(std::size_t first, std::size_t last) const;
	/// @brief Whether every non-blank line starts with a comment marker after its indentation.
	/// @param first The first line.
	/// @param last The last line.
	/// @param bare The marker without its space, e.g. "//".
	[[nodiscard]] bool linesCommented(std::size_t first, std::size_t last, std::u32string_view bare) const;
	/// @brief Comments one line out, or back in.
	/// @param line The line.
	/// @param column Where the marker goes, or where it is.
	/// @param uncomment true to take the marker out (with its space, if it has one).
	/// @param marker The marker with its space, e.g. "// ".
	/// @return How many columns the text after @p column moved (negative to the left).
	long toggleLineComment(std::size_t line, std::size_t column, bool uncomment, std::u32string_view marker);
	/// @brief Alt+Up or Alt+Down: moves the selected lines.
	/// @param down true to move them down.
	void moveLines(bool down);
	/// @brief Cmd+Shift+K: deletes the selected lines.
	void deleteLines();
	/// @brief The kind of each code point of a line.
	/// @param line The line.
	/// @param inComment In: whether a block comment is open at its start; out: at its end.
	[[nodiscard]] std::vector<Token> tokenize(std::u32string_view line, bool& inComment) const;

	Language language_ = Language::Plain;        ///< See setLanguage().
	long long goToRequest_ = 0;                  ///< The last request goToLine() carried out.
	std::array<affresco::Color, 7> tokenColors_; ///< Indexed by Token (Plain unused).
};

/// @brief Finds the text an application keeps for an id (e.g. Forge's open files), for "buffer".
using BufferLookup = std::function<std::shared_ptr<affresco::TextModel>(std::string_view id)>;

/// @brief Registers the "code" type on a screen: a CodeEditor with the properties of "editor", plus "language",
/// "syntax" (`{ "keyword", "type", "string", "number", "comment", "heading" }`: a colour each), "goTo" and, with a
/// lookup, "buffer": the id of a text the application keeps, which the editor then edits in place of "text" (so a
/// large file never passes through the screen's state).
/// @param screen The screen, before it is built; it must not move afterwards (the type reads its colours).
/// @param buffers Finds the text for "buffer" (empty: "buffer" is ignored); registering again replaces it.
void registerCodeEditor(affresco::Screen& screen, BufferLookup buffers = {});

} // namespace examples
