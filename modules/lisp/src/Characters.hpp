#pragma once

/// @file
/// @brief Text counted in characters, as `count`, `subs` and the text functions count it: a character is a whole UTF-8
/// sequence, never a byte of one.

#include <cstddef>
#include <string_view>

namespace affresco::lisp {

/// @brief Tells whether a byte continues a UTF-8 sequence rather than starting a character.
/// @param byte The byte.
/// @return true if it continues one.
[[nodiscard]] constexpr bool continuesCharacter(char byte) noexcept {
	return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

/// @brief Where the character after the one at an offset starts.
/// @param text The string.
/// @param at The offset of a character, below the string's size.
/// @return The next one's offset, or the string's size.
[[nodiscard]] constexpr std::size_t nextCharacter(std::string_view text, std::size_t at) noexcept {
	++at;
	while(at < text.size() && continuesCharacter(text[at])) ++at;
	return at;
}

/// @brief Where a character starts in a string.
/// @param text The string.
/// @param index Which character.
/// @return Its offset; the string's size for the one past the end, and past that npos.
[[nodiscard]] constexpr std::size_t offsetOf(std::string_view text, std::size_t index) noexcept {
	std::size_t at = 0;
	for(std::size_t passed = 0; passed < index; ++passed) {
		if(at >= text.size()) return std::string_view::npos;
		at = nextCharacter(text, at);
	}
	return at;
}

/// @brief How many characters a string has.
/// @param text The string.
/// @return The count.
[[nodiscard]] constexpr std::size_t charactersIn(std::string_view text) noexcept {
	std::size_t count = 0;
	for(const char byte : text) count += continuesCharacter(byte) ? 0U : 1U;
	return count;
}

} // namespace affresco::lisp
