#pragma once

/// @file
/// @brief What the Lisp reader, its writer and the language (modules/lisp) agree on: which strings are written as bare
/// words, the words reserved for other values, and the words the language gives a meaning to.

#include <algorithm>
#include <array>
#include <iterator>
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

/// The plain words the language (modules/lisp) runs. `list` is also a node type and isn't here: in a data position it
/// stays a node, and running `list` there takes `~`. A test in tests/unit/lisp.cpp checks this list against the
/// language.
// clang-format off
inline constexpr auto languageWords = std::to_array<std::string_view>({
    "abs", "add-hook", "advice", "and", "apply", "assoc", "assoc-in", "backquote", "catch", "concat", "cond", "conj",
    "cons", "count", "dec", "def", "defcommand", "defcomponent", "defcomputed", "defcustom", "defdefaults",
    "defhook", "defkey", "defmacro", "defmenu", "defn", "defpart", "defscreen", "defstate", "defstyle", "deftokens",
    "defview", "dissoc", "do", "drop", "filter", "find", "first", "fn", "gensym", "get", "get-in", "handler",
    "identity", "if", "inc", "include", "join", "keys", "keyword", "last", "let", "loop", "lower-case", "map",
    "map-indexed", "mapcat", "max", "merge", "min", "mod", "name", "not", "nth", "or", "or-else", "pr-str", "quote",
    "range", "recur", "reduce", "rem", "remove", "remove-hook", "repeat", "rest", "reverse", "second", "some",
    "sort", "sort-by", "str", "subs", "symbol", "take", "throw", "try", "unquote", "unquote-splicing", "update",
    "update-in", "upper-case", "vals", "vec", "when", "zipmap"});
// clang-format on

/// @brief Tells whether a plain word is one the language runs (languageWords): in a data position of a language file
/// it isn't a node type, and the writer writes a node of that type as `(node :type word ...)`.
/// @param word The word.
/// @return true if it is.
[[nodiscard]] constexpr bool isLanguageWord(std::string_view word) noexcept {
	return std::ranges::find(languageWords, word) != languageWords.end();
}

} // namespace affresco::notation
