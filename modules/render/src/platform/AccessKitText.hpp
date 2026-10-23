#pragma once

/// @file
/// @brief Text as AccessKit takes it (platform/AccessKitTree.cpp), apart from AccessKit's header so the unit tests
/// reach it: AccessKit reads C strings of UTF-8, and a text run's character lengths must add up to its value.

#include "detail/Utf8.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::platform {

/// @brief Text as AccessKit takes it: each invalid UTF-8 sequence and each NUL becomes U+FFFD, so the C string holds
/// all of the text and a character stays one character.
/// @param text The text, as a panel gives it.
/// @return Valid UTF-8 without a NUL.
[[nodiscard]] inline std::string accessKitText(std::string_view text) {
	std::string out;
	out.reserve(text.size());
	while(!text.empty()) {
		const detail::Decoded decoded = detail::decodeUtf8(text);
		detail::appendUtf8(out, decoded.codepoint == 0 ? char32_t{0xFFFD} : decoded.codepoint);
		text.remove_prefix(decoded.length);
	}
	return out;
}

/// @brief The UTF-8 length of each code point of a text, as AccessKit counts a text run's characters.
/// @param text Valid UTF-8 (accessKitText()).
/// @return The lengths; they add up to the text's size.
[[nodiscard]] inline std::vector<std::uint8_t> characterLengths(std::string_view text) {
	std::vector<std::uint8_t> lengths;
	while(!text.empty()) {
		const std::size_t length = detail::decodeUtf8(text).length;
		lengths.push_back(static_cast<std::uint8_t>(length));
		text.remove_prefix(length);
	}
	return lengths;
}

} // namespace affresco::platform
