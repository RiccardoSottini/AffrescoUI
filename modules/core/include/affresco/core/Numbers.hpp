#pragma once

/// @file
/// @brief readNumber: strtod() for text from outside the app, which never reads a number longer than
/// maxNumberLength.

#include "affresco/core/Json.hpp"

#include <cstdlib>
#include <string_view>

namespace affresco {

/// @brief Reads the number text starts with, as std::strtod() does, unless it is longer than maxNumberLength.
///
/// Some platforms' strtod() stops the program on certain numbers a few hundred digits long, so every number read
/// from a document, a peer or a file is bounded first, as JSON's are. The number's extent is measured generously
/// (letters, digits, '+', '-' and '.' after any leading spaces), so hexadecimal and exponent forms are counted too.
/// @param text A terminated string.
/// @param end Gets where the number ends; `text` when none was read (none there, or too long).
/// @return The number, or 0 when none was read.
[[nodiscard]] inline double readNumber(const char* text, char** end) noexcept {
	const char* start = text;
	while(*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' || *start == '\f' || *start == '\v')
		++start;
	std::size_t length = 0;
	for(const char* at = start; length <= maxNumberLength; ++at, ++length) {
		const char c = *at;
		const bool part = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '+' ||
		                  c == '-' || c == '.';
		if(!part) break;
	}
	if(length > maxNumberLength) {
		*end = const_cast<char*>(text); // NOLINT(cppcoreguidelines-pro-type-const-cast): strtod()'s own contract
		return 0.0;
	}
	return std::strtod(text, end);
}

/// @brief Whether a whole text is short enough to be read as a number by strtod(), stof() or stod().
/// @param text The text.
/// @return Whether it is at most maxNumberLength characters.
[[nodiscard]] constexpr bool numberLengthFits(std::string_view text) noexcept { return text.size() <= maxNumberLength; }

} // namespace affresco
