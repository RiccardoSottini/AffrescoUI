#pragma once

/// @file
/// @brief The Studio's evidence pictures (studio.cpp): decoded from the host's base64, the changed region outlined.

#include <affresco/affresco.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace examples::studio {

/// @brief Decodes base64 (RFC 4648), as the host sends pictures.
/// @param text The text.
/// @return The bytes.
/// @throws std::invalid_argument If it isn't base64.
inline std::vector<std::uint8_t> fromBase64(std::string_view text) {
	constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::vector<std::uint8_t> bytes;
	std::uint32_t bits = 0;
	int count = 0;
	for(const char c : text) {
		if(c == '=') break;
		const std::size_t value = alphabet.find(c);
		if(value == std::string_view::npos) throw std::invalid_argument("not base64");
		bits = (bits << 6) | static_cast<std::uint32_t>(value);
		count += 6;
		if(count >= 8) {
			count -= 8;
			bytes.push_back(static_cast<std::uint8_t>((bits >> count) & 0xFF));
		}
	}
	return bytes;
}

/// @brief Draws the changed region's outline on a picture, so a reviewer sees at once what moved.
/// @param picture The picture, opaque.
/// @param region `[x, y, width, height]`, in its pixels; nothing is drawn when it's empty.
/// @return Its pixels with the outline, red, green, blue and alpha.
inline std::vector<std::uint8_t> outlined(const affresco::Bitmap& picture, const std::array<int, 4>& region) {
	std::vector<std::uint8_t> rgba(picture.pixels().begin(), picture.pixels().end());
	const auto [x, y, w, h] = region;
	if(w <= 0 || h <= 0) return rgba;
	constexpr int thickness = 3;
	const auto mark = [&](int px, int py) {
		if(px < 0 || py < 0 || px >= picture.width() || py >= picture.height()) return;
		const auto at = static_cast<std::size_t>(py * picture.width() + px) * 4;
		const std::array<std::uint8_t, 4> magenta = {236, 64, 122, 255};
		std::ranges::copy(magenta, rgba.begin() + static_cast<std::ptrdiff_t>(at));
	};
	for(int t = 1; t <= thickness; ++t) {
		for(int px = x - t; px < x + w + t; ++px) {
			mark(px, y - t);
			mark(px, y + h - 1 + t);
		}
		for(int py = y - t; py < y + h + t; ++py) {
			mark(x - t, py);
			mark(x + w - 1 + t, py);
		}
	}
	return rgba;
}

} // namespace examples::studio
