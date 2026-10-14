#include "sgl/Color.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace sgl {

namespace {

constexpr int hexDigit(char c) noexcept {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// Rounds instead of truncating: 0.941176 must become 0xf0, not 0xef
std::uint8_t toByte(float channel) noexcept {
	return static_cast<std::uint8_t>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
}

} // namespace

std::optional<Color> Color::parseHex(std::string_view text) {
	if(text.starts_with('#')) text.remove_prefix(1);
	if(text.size() != 3 && text.size() != 6 && text.size() != 8) return std::nullopt;

	std::array<int, 8> digits{};
	for(std::size_t i = 0; i < text.size(); ++i) {
		digits[i] = hexDigit(text[i]);
		if(digits[i] < 0) return std::nullopt;
	}

	auto byte = [&](std::size_t index) {
		const int value = text.size() == 3 ? digits[index] * 17 : digits[index * 2] * 16 + digits[index * 2 + 1];
		return static_cast<float>(value) / 255.0f;
	};

	return Color{byte(0), byte(1), byte(2), text.size() == 8 ? byte(3) : 1.0f};
}

std::array<std::uint8_t, 4> Color::toBytes() const noexcept { return {toByte(r), toByte(g), toByte(b), toByte(a)}; }

std::string Color::toHex() const {
	static constexpr std::string_view digits = "0123456789abcdef";

	std::string out = "#";
	const auto bytes = toBytes();
	const std::size_t channels = bytes[3] == 255 ? 3 : 4;
	for(std::size_t i = 0; i < channels; ++i) {
		out += digits[bytes[i] >> 4];
		out += digits[bytes[i] & 0x0f];
	}

	return out;
}

} // namespace sgl
