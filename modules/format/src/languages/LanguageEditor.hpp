#pragma once

/// @file
/// @brief The "editor" type's TextEditor: with no language and no commands it is a plain TextEditor; a declared
/// language (Language.hpp) adds its editing rules (auto-closing pairs, indentation after and before some lines), and
/// "commands" binds keys to code commands (comment lines, move lines, ...). It also reports typing, the hovered place
/// and the keys of a popup the document shows (completions), and applies one-off requests (go to, insert).

#include "Language.hpp"

#include "affresco/core/Signal.hpp"
#include "affresco/render/TextEditor.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief A TextEditor with a declared language's editing rules and declared code commands.
///
/// Invariants: without a language, commands or a popup, every key and character goes to TextEditor unchanged.
class LanguageEditor : public TextEditor {
public:
	static constexpr std::size_t maxCommands = 256; ///< Keys bound in "commands", at most.

	/// @brief The code commands a key can run: each one edit (one undo step).
	/// @return Their names.
	[[nodiscard]] static const std::vector<std::string_view>& codeCommandNames();

	LanguageEditor();

	/// @brief Sets the language whose rules typing follows.
	/// @param language It, or nullptr for none.
	void setLanguage(std::shared_ptr<const Language> language) { language_ = std::move(language); }
	/// @brief The language, or nullptr.
	[[nodiscard]] const std::shared_ptr<const Language>& language() const noexcept { return language_; }

	/// @brief Binds keys to code commands.
	/// @param commands Normalised shortcuts ("cmd+shift+k", as shortcut() writes them) → command names.
	void setCommands(std::map<std::string, std::string, std::less<>> commands) { commands_ = std::move(commands); }

	/// @brief Runs a code command (codeCommandNames()).
	/// @param name Its name.
	/// @return Whether it is one.
	bool runCodeCommand(std::string_view name);

	/// @brief While on, the keys a popup uses (up, down, pageUp, pageDown, enter, tab, escape; without modifiers) go
	/// to onCode as "onPopupKey" instead of the text.
	/// @param on Whether a popup is shown.
	void setPopup(bool on) noexcept { popup_ = on; }

	/// @brief Moves the caret, once per request (a later update with the same request does nothing).
	/// @param line The line, from 0 (kept to the text).
	/// @param column The column, from 0 (kept to the line).
	/// @param request The request's number.
	void goTo(std::size_t line, std::size_t column, double request);

	/// @brief Replaces the selection and the code points just before it with a text, once per request (a completion).
	/// @param text The text (UTF-8).
	/// @param before How many code points before the selection go too.
	/// @param request The request's number.
	void insertText(std::string_view text, std::size_t before, double request);

	/// @brief Remembers the runs a document paints over the colours ("tokens").
	/// @param tokens Them, as given.
	/// @return Whether they differ from the last ones.
	bool tokensChanged(const json& tokens) {
		if(tokens == tokens_) return false;
		tokens_ = tokens;
		return true;
	}

	/// @brief The word that ends at the caret: the language's "wordPattern", or letters, digits and '_'.
	/// @return It, in UTF-8.
	[[nodiscard]] std::string wordBeforeCaret();

	/// @brief An event for the document: "onType" (`text`: what was typed, "" for a deletion; `word`), "onTextHover"
	/// (`over`), "onPopupKey" (`key`), with `line`, `column` (from 0) and the place's box (`x`, `y`, `top`).
	Signal<std::string_view, const json&> onCode;

protected:
	void keyEvent(const KeyEvent& event) override;
	void charEvent(const CharEvent& event) override;
	void newLine() override;

private:
	/// @brief How typing a character goes with the language's pairs.
	enum class PairTyping {
		None,     ///< Typed as usual.
		StepOver, ///< The same closer is already next: the caret steps over it.
		Wrap,     ///< An opener with a selection: the selection is wrapped in the pair.
		Close,    ///< An opener: its closer is added after the caret.
	};

	/// @brief The closer of an opening character in the language's pairs.
	/// @param c The character.
	/// @return It, or 0.
	[[nodiscard]] char32_t closerOf(char32_t c) const noexcept;
	/// @brief Whether a character closes one of the language's pairs.
	/// @param c The character.
	[[nodiscard]] bool isCloser(char32_t c) const noexcept;
	/// @brief How a typed character goes with the pairs.
	/// @param typed The character.
	[[nodiscard]] PairTyping pairTypingFor(char32_t typed) const;
	/// @brief Types a character with the pairs and the indentation rules.
	/// @param event The character's event.
	void typeCharacter(const CharEvent& event);
	/// @brief Deletes an empty pair around the caret (Backspace between "(" and ")").
	/// @return Whether it did.
	bool deleteEmptyPair();
	/// @brief Outdents the caret's line by a level when it now matches "indent.decrease".
	void outdentIfDecreasing();
	/// @brief Whether a line's text matches one of the language's indentation expressions.
	/// @param pattern The expression, or nullptr.
	/// @param line The line.
	/// @param upTo Its code points looked at.
	[[nodiscard]] bool lineMatches(const std::shared_ptr<const TextPattern>& pattern, std::size_t line,
	                               std::size_t upTo);
	/// @brief Runs the command a key is bound to.
	/// @param event The key.
	/// @return Whether one ran.
	bool runBoundCommand(const KeyEvent& event);
	/// @brief Gives a popup key to the document.
	/// @param event The key.
	/// @return Whether the popup took it.
	bool popupKey(const KeyEvent& event);
	/// @brief Reports typing as "onType".
	/// @param text What was typed (UTF-8), "" for a deletion.
	void reportTyping(std::string_view text);
	/// @brief An event for a place: `line`, `column` and its box.
	/// @param at The place.
	[[nodiscard]] json placeEvent(Position at) const;

	/* Commands (LanguageEditorCommands.cpp) */

	/// @brief Comments the selected lines out with the language's line comment, or back in if all are.
	void toggleComment();
	/// @brief Moves the selected lines up or down a line.
	/// @param down Which way.
	void moveLines(bool down);
	/// @brief Deletes the selected lines.
	void deleteLines();
	/// @brief Copies (or cuts) the caret's line when nothing is selected.
	/// @param cut Whether to delete it too.
	void copyLine(bool cut);
	/// @brief Puts a new line above or below the caret's, with its indentation, and the caret on it.
	/// @param below Which side.
	void openLine(bool below);

	std::shared_ptr<const Language> language_;                 ///< See setLanguage().
	std::map<std::string, std::string, std::less<>> commands_; ///< See setCommands().
	bool popup_ = false;                                       ///< See setPopup().
	std::optional<double> goToRequest_;                        ///< The last goTo() request applied.
	std::optional<double> insertRequest_;                      ///< The last insertText() request applied.
	json tokens_ = json::array();                              ///< See tokensChanged().
	PatternGroups groups_;                                     ///< For the language's expressions, step-limited.
};

} // namespace affresco::format
