/// @file
/// @brief Bitmap::fromDataUri: a PNG or JPEG image given inline, as a base64 `data:` URI.

#include "affresco/render/Bitmap.hpp"

#include <array>
#include <stdexcept>
#include <string>

namespace affresco {

namespace {

/// @brief Decodes base64 (RFC 4648): padding only at the end, nothing but the alphabet before it.
/// @param text The text.
/// @return The bytes.
/// @throws std::runtime_error If it isn't base64.
std::vector<std::uint8_t> fromBase64(std::string_view text) {
	constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::size_t end = text.size();
	while(end > 0 && text.size() - end < 2 && text[end - 1] == '=') --end;
	std::vector<std::uint8_t> bytes;
	bytes.reserve(end / 4 * 3 + 3);
	std::uint32_t bits = 0;
	int count = 0;
	for(const char c : text.substr(0, end)) {
		const std::size_t value = alphabet.find(c);
		if(value == std::string_view::npos) throw std::runtime_error("the image's data isn't base64");
		bits = ((bits << 6) | static_cast<std::uint32_t>(value)) & 0xFFFFFFu;
		count += 6;
		if(count >= 8) {
			count -= 8;
			bytes.push_back(static_cast<std::uint8_t>((bits >> count) & 0xFFu));
		}
	}
	return bytes;
}

} // namespace

std::shared_ptr<const Bitmap> Bitmap::fromDataUri(std::string_view uri, const Limits& limits) {
	static constexpr std::array<std::string_view, 2> prefixes{"data:image/png;base64,", "data:image/jpeg;base64,"};
	const bool declaredPng = uri.starts_with(prefixes[0]);
	if(!declaredPng && !uri.starts_with(prefixes[1]))
		throw std::runtime_error("not a base64 data: URI of a PNG or JPEG image");
	const std::string_view data = uri.substr((declaredPng ? prefixes[0] : prefixes[1]).size());
	if(data.size() / 4 * 3 > limits.fileBytes) throw std::runtime_error("the image's data is too large");
	const std::vector<std::uint8_t> bytes = fromBase64(data);
	const bool png = bytes.size() >= 4 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G';
	const bool jpeg = bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xD8;
	if(declaredPng ? !png : !jpeg) throw std::runtime_error("the image's data isn't the type its URI declares");
	return decode(bytes, limits);
}

} // namespace affresco
