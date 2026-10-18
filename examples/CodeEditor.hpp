#pragma once

/// @file
/// @brief CodeEditor: an application-layer widget for the Forge template (code-editor.screen.json), built on the
/// library's TextEditor. It shows how an application adds its own type: subclass a widget, register it with
/// registerCodeEditor().

#include <affresco/affresco.hpp>

#include <array>
#include <cstddef>
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

protected:
	void keyEvent(const affresco::KeyEvent& event) override;
	void charEvent(const affresco::CharEvent& event) override;
	void colorLine(std::u32string_view line, int& state, std::vector<affresco::Color>& colors) const override;
	void newLine() override;

private:
	/// @brief Cmd+/: comments the selected lines out, or back in if they all are.
	void toggleComment();
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
	std::array<affresco::Color, 7> tokenColors_; ///< Indexed by Token (Plain unused).
};

/// @brief Registers the "code" type on a screen: a CodeEditor with the properties of "editor", plus "language" and
/// "syntax" (`{ "keyword", "type", "string", "number", "comment", "heading" }`: a colour each).
/// @param screen The screen, before it is built; it must not move afterwards (the type reads its colours).
void registerCodeEditor(affresco::Screen& screen);

} // namespace examples
