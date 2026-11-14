#pragma once

/// @file
/// @brief What the Lisp reader, its writer and the language (modules/lisp) agree on: which strings are written as bare
/// words, the words reserved for other values, and the words the language gives a meaning to.

#include "affresco/notation/words/Core.hpp"
#include "affresco/notation/words/Forms.hpp"
#include "affresco/notation/words/Functions.hpp"
#include "affresco/notation/words/Namespaces.hpp"
#include "affresco/notation/words/Reader.hpp"
#include "affresco/notation/words/Sets.hpp"
#include "affresco/notation/words/Text.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <span>
#include <string_view>

namespace affresco::notation {

/// The head of a node whose type isn't written as its head (README.md, "Nodes").
inline constexpr std::string_view untypedHead = "node";

/// @brief Tells whether a character can follow the first one of a bare word: a letter, a digit, '_', '.', '@', '$'
/// or '-'.
/// @param c The character.
/// @return true if it can.
[[nodiscard]] constexpr bool isWordCharacter(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
	       c == '@' || c == '$' || c == '-';
}

/// @brief Tells whether a text is one of the words that stand for other values: true, false and nil (null).
/// @param text The text.
/// @return true if it is.
[[nodiscard]] constexpr bool isReservedWord(std::string_view text) noexcept {
	return text == "true" || text == "false" || text == "nil";
}

/// @brief Tells whether a character can start a bare word: an ASCII letter, '_', '@' or '$'.
/// @param c The character.
/// @return true if it can.
[[nodiscard]] constexpr bool isWordStart(char c) noexcept {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '@' || c == '$';
}

/// @brief Tells whether a string is written as a bare word: a character that can start one (isWordStart()), then
/// word characters (isWordCharacter()), and not a reserved word. Anything else is written in quotes, as JSON writes
/// strings.
/// @param text The string.
/// @return true if it is.
[[nodiscard]] constexpr bool isPlainWord(std::string_view text) noexcept {
	return !text.empty() && !isReservedWord(text) && isWordStart(text.front()) &&
	       std::all_of(std::next(text.begin()), text.end(), isWordCharacter);
}

/// The lists of the plain words the language (modules/lisp) runs, one per area of it (words/), which the packages that
/// add to the language fill each its own. `list` is also a node type and is on none: in a data position it stays a
/// node, and running `list` there takes `~`. Nor are `boolean` and `vector`, the node types of the same names.
inline constexpr auto languageWordLists = std::to_array<std::span<const std::string_view>>(
    {coreWords, formsWords, functionsWords, textWords, readerWords, setsWords, namespacesWords});

/// The language's core words (words/Core.hpp), by the name a reader of this list had before it was split.
inline constexpr const auto& languageWords = coreWords;

/// @brief Tells whether a plain word is one the language runs (languageWordLists): in a data position of a language
/// file it isn't a node type, and the writer writes a node of that type as `(node :type word ...)`.
/// @param word The word.
/// @return true if it is.
[[nodiscard]] constexpr bool isLanguageWord(std::string_view word) noexcept {
	return std::ranges::any_of(languageWordLists, [&](std::span<const std::string_view> list) {
		return std::ranges::find(list, word) != list.end();
	});
}

} // namespace affresco::notation
