#pragma once

/// @file
/// @brief UTF-8 and UTF-32 conversion.

#include <string>
#include <string_view>

namespace sgl::detail {

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

/// @brief Decodes UTF-8.
/// @param text UTF-8 text.
/// @return The code points; invalid or truncated sequences become U+FFFD.
inline std::u32string fromUtf8(std::string_view text) {
	std::u32string out;
	out.reserve(text.size());

	for(std::size_t i = 0; i < text.size();) {
		const auto lead = static_cast<unsigned char>(text[i]);
		std::size_t length = 0;
		char32_t cp = 0;

		if(lead < 0x80) {
			length = 1;
			cp = lead;
		} else if((lead >> 5) == 0x6) {
			length = 2;
			cp = lead & 0x1Fu;
		} else if((lead >> 4) == 0xE) {
			length = 3;
			cp = lead & 0x0Fu;
		} else if((lead >> 3) == 0x1E) {
			length = 4;
			cp = lead & 0x07u;
		}

		bool valid = length > 0 && i + length <= text.size();
		for(std::size_t k = 1; valid && k < length; ++k) {
			const auto next = static_cast<unsigned char>(text[i + k]);
			valid = (next >> 6) == 0x2;
			cp = (cp << 6) | (next & 0x3Fu);
		}

		out.push_back(valid ? cp : char32_t{0xFFFD});
		i += valid ? length : 1;
	}

	return out;
}

} // namespace sgl::detail
