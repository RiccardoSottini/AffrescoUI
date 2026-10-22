#pragma once

/// @file
/// @brief The Studio's evidence pictures (studio.cpp): decoded from the host's base64, the changed region outlined.

#include <affresco/affresco.hpp>

#include <algorithm>
#include <array>
#include <climits>
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

/// @brief The changed region of a proposal's evidence.
/// @param evidence The evidence: `{"region": [x, y, width, height]}`, from the connected peer.
/// @return The region; empty if the evidence has none.
/// @throws std::out_of_range If a coordinate is a number beyond int, whose conversion C++ leaves undefined.
/// @throws affresco::json::exception If the region isn't four numbers.
inline std::array<int, 4> regionOf(const affresco::json& evidence) {
	const affresco::json region = evidence.value("region", affresco::json::array({0, 0, 0, 0}));
	for(const affresco::json& value : region)
		if(value.is_number_float()) {
			const double number = value.get<double>();
			const bool fits = number > double{INT_MIN} - 1 && number < double{INT_MAX} + 1; // and NaN doesn't
			if(!fits) throw std::out_of_range("the evidence's region is beyond the picture's coordinates");
		}
	return region.get<std::array<int, 4>>();
}

/// @brief Draws the changed region's outline on a picture, so a reviewer sees at once what moved.
/// @param picture The picture, opaque.
/// @param region `[x, y, width, height]`, in its pixels; nothing is drawn when it's empty. It comes from the peer the
/// Studio is connected to, so it may be anything: the outline is computed in 64 bits and clipped to the picture.
/// @return Its pixels with the outline, red, green, blue and alpha.
inline std::vector<std::uint8_t> outlined(const affresco::Bitmap& picture, const std::array<int, 4>& region) {
	std::vector<std::uint8_t> rgba(picture.pixels().begin(), picture.pixels().end());
	const auto [x, y, w, h] = region;
	if(w <= 0 || h <= 0) return rgba;
	constexpr std::int64_t thickness = 3;
	const std::int64_t width = picture.width(), height = picture.height();
	const auto mark = [&](std::int64_t px, std::int64_t py) {
		if(px < 0 || py < 0 || px >= width || py >= height) return;
		const auto at = static_cast<std::size_t>(py * width + px) * 4;
		const std::array<std::uint8_t, 4> magenta = {236, 64, 122, 255};
		std::ranges::copy(magenta, rgba.begin() + static_cast<std::ptrdiff_t>(at));
	};
	const std::int64_t left = x, top = y, right = left + w - 1, bottom = top + h - 1; // the region's edges, inclusive
	for(std::int64_t t = 1; t <= thickness; ++t) {
		// only the part of each line inside the picture: a huge region is no slower than one that covers it
		for(std::int64_t px = std::max(left - t, std::int64_t{0}); px <= std::min(right + t, width - 1); ++px) {
			mark(px, top - t);
			mark(px, bottom + t);
		}
		for(std::int64_t py = std::max(top - t, std::int64_t{0}); py <= std::min(bottom + t, height - 1); ++py) {
			mark(left - t, py);
			mark(right + t, py);
		}
	}
	return rgba;
}

} // namespace examples::studio
