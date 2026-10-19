#pragma once

/// @file
/// @brief UTF-8 and UTF-32 conversion.

#include <cstddef>
#include <string>
#include <string_view>

namespace affresco::detail {

/// @brief Appends a code point as UTF-8.
/// @param out The string.
/// @param cp The code point; invalid ones become U+FFFD.
inline void appendUtf8(std::string& out, char32_t cp) {
	auto byte = [&](char32_t value) { out.push_back(static_cast<char>(value)); };

	if(cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = 0xFFFD; // not encodable: replacement character

	if(cp < 0x80) {
		byte(cp);
	} else if(cp < 0x800) {
		byte(0xC0 | (cp >> 6));
		byte(0x80 | (cp & 0x3F));
	} else if(cp < 0x10000) {
		byte(0xE0 | (cp >> 12));
		byte(0x80 | ((cp >> 6) & 0x3F));
		byte(0x80 | (cp & 0x3F));
	} else {
		byte(0xF0 | (cp >> 18));
		byte(0x80 | ((cp >> 12) & 0x3F));
		byte(0x80 | ((cp >> 6) & 0x3F));
		byte(0x80 | (cp & 0x3F));
	}
}

/// @brief Encodes code points as UTF-8.
/// @param text The code points.
/// @return UTF-8 text.
inline std::string toUtf8(std::u32string_view text) {
	std::string out;
	out.reserve(text.size());
	for(char32_t cp : text) appendUtf8(out, cp);
	return out;
}

/// @brief One decoded character and the bytes it took.
struct Decoded {
	char32_t codepoint; ///< The character, or U+FFFD for an invalid sequence.
	std::size_t length; ///< Bytes read: at least one.
};

/// @brief Decodes the character at the start of UTF-8 text, following the Unicode standard's well-formed sequences
/// (table 3-7): no overlong forms, no surrogates, nothing above U+10FFFF.
/// @param text UTF-8 text, not empty.
/// @return The character. An invalid sequence becomes one U+FFFD for its longest valid start (at least one byte), as
/// Unicode recommends, so the bytes after it are decoded as they are.
inline Decoded decodeUtf8(std::string_view text) noexcept {
	const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
	const unsigned char lead = byte(0);
	if(lead < 0x80) return {lead, 1};

	// The length and the range the second byte must be in, from the lead byte; later bytes are 0x80..0xBF
	std::size_t length = 0;
	unsigned char low = 0x80, high = 0xBF;
	char32_t cp = 0;
	if(lead >= 0xC2 && lead <= 0xDF) {
		length = 2;
		cp = lead & 0x1Fu;
	} else if(lead >= 0xE0 && lead <= 0xEF) {
		length = 3;
		cp = lead & 0x0Fu;
		if(lead == 0xE0) low = 0xA0;  // overlong below U+0800
		if(lead == 0xED) high = 0x9F; // surrogates U+D800..U+DFFF
	} else if(lead >= 0xF0 && lead <= 0xF4) {
		length = 4;
		cp = lead & 0x07u;
		if(lead == 0xF0) low = 0x90;  // overlong below U+10000
		if(lead == 0xF4) high = 0x8F; // above U+10FFFF
	} else {
		return {0xFFFD, 1}; // a continuation byte, or a lead byte no valid sequence has
	}

	for(std::size_t k = 1; k < length; ++k) {
		const unsigned char next = k < text.size() ? byte(k) : 0;
		const bool valid = k == 1 ? next >= low && next <= high : next >= 0x80 && next <= 0xBF;
		if(!valid) return {0xFFFD, k};
		cp = (cp << 6) | (next & 0x3Fu);
	}
	return {cp, length};
}

/// @brief Decodes UTF-8.
/// @param text UTF-8 text.
/// @return The code points; each invalid or truncated sequence becomes one U+FFFD (see decodeUtf8()).
inline std::u32string fromUtf8(std::string_view text) {
	std::u32string out;
	out.reserve(text.size());

	while(!text.empty()) {
		const Decoded decoded = decodeUtf8(text);
		out.push_back(decoded.codepoint);
		text.remove_prefix(decoded.length);
	}

	return out;
}

} // namespace affresco::detail
