#pragma once

/// @file
/// @brief What the Lisp reader and writer agree on: which strings are written as bare words, and the words reserved
/// for other values.

#include <algorithm>
#include <iterator>
#include <string_view>

namespace affresco::notation::detail {

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

} // namespace affresco::notation::detail
