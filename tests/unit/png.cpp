/// @file
/// @brief Unit tests for the PNG encoder behind pictures of the screen ("render"): every image is decoded again
/// here, chunk by chunk, and compared pixel for pixel.

#include <Png.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using affresco::detail::encodePng;

namespace {

/// @brief Reads a 32-bit number stored most significant byte first.
/// @param bytes The bytes.
/// @param at Where it starts.
/// @return The number.
std::uint32_t bigEndian(const std::string& bytes, std::size_t at) {
	std::uint32_t value = 0;
	for(std::size_t i = 0; i < 4; ++i) value = (value << 8) | static_cast<std::uint8_t>(bytes.at(at + i));
	return value;
}

/// @brief CRC-32 (the PNG polynomial), bit by bit: slow, and independent of the encoder's table.
/// @param bytes The bytes.
/// @return The checksum.
std::uint32_t crc32(std::string_view bytes) {
	std::uint32_t crc = 0xFFFFFFFFu;
	for(const char byte : bytes) {
		crc ^= static_cast<std::uint8_t>(byte);
		for(int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
	}
	return ~crc;
}

/// @brief Reads a deflate stream's bits, least significant first.
struct BitReader {
	std::span<const std::uint8_t> data; ///< The stream.
	std::size_t position = 0;           ///< In bits.

	/// @brief Reads one bit.
	std::uint32_t bit() {
		if(position / 8 >= data.size()) throw std::runtime_error("the deflate stream ends early");
		const std::uint32_t value = (data[position / 8] >> (position % 8)) & 1;
		++position;
		return value;
	}
	/// @brief Reads a number stored least significant bit first.
	/// @param count How many bits.
	std::uint32_t bits(int count) {
		std::uint32_t value = 0;
		for(int i = 0; i < count; ++i) value |= bit() << i;
		return value;
	}
	/// @brief Reads a Huffman code, most significant bit first.
	/// @param count How many bits.
	std::uint32_t code(int count) {
		std::uint32_t value = 0;
		for(int i = 0; i < count; ++i) value = (value << 1) | bit();
		return value;
	}
};

/// @brief Reads a literal/length symbol in deflate's fixed Huffman code (RFC 1951, 3.2.6).
/// @param in The stream.
/// @return 0..287.
int fixedSymbol(BitReader& in) {
	std::uint32_t code = in.code(7);
	if(code <= 0x17) return static_cast<int>(256 + code);
	code = (code << 1) | in.bit();
	if(code >= 0x30 && code <= 0xBF) return static_cast<int>(code - 0x30);
	if(code >= 0xC0 && code <= 0xC7) return static_cast<int>(280 + code - 0xC0);
	code = (code << 1) | in.bit();
	return static_cast<int>(144 + code - 0x190);
}

/// @brief Decompresses a zlib stream made of fixed-Huffman blocks, and checks its header and Adler-32.
/// @param stream The stream.
/// @return The bytes.
std::vector<std::uint8_t> inflate(const std::string& stream) {
	static constexpr std::array<int, 29> lengthBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
	                                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
	static constexpr std::array<int, 29> lengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
	                                                    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
	static constexpr std::array<int, 30> distanceBase = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
	                                                     33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
	                                                     1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
	static constexpr std::array<int, 30> distanceExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
	                                                      6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
	REQUIRE(stream.size() >= 6);
	const auto cmf = static_cast<std::uint8_t>(stream[0]), flags = static_cast<std::uint8_t>(stream[1]);
	CHECK((cmf & 0x0F) == 8); // deflate
	CHECK((cmf * 256 + flags) % 31 == 0);

	BitReader in{std::span(reinterpret_cast<const std::uint8_t*>(stream.data()) + 2, stream.size() - 6)};
	std::vector<std::uint8_t> out;
	for(bool last = false; !last;) {
		last = in.bit() == 1;
		REQUIRE(in.bits(2) == 1); // fixed Huffman codes
		for(int symbol = fixedSymbol(in); symbol != 256; symbol = fixedSymbol(in)) {
			if(symbol < 256) {
				out.push_back(static_cast<std::uint8_t>(symbol));
				continue;
			}
			const auto l = static_cast<std::size_t>(symbol - 257);
			const int length = lengthBase.at(l) + static_cast<int>(in.bits(lengthExtra.at(l)));
			const auto d = static_cast<std::size_t>(in.code(5));
			const auto distance =
			    static_cast<std::size_t>(distanceBase.at(d) + static_cast<int>(in.bits(distanceExtra.at(d))));
			if(distance > out.size()) throw std::runtime_error("a match refers back before the start");
			for(int i = 0; i < length; ++i) out.push_back(out[out.size() - distance]);
		}
	}

	std::uint32_t a = 1, b = 0;
	for(const std::uint8_t byte : out) {
		a = (a + byte) % 65521;
		b = (b + a) % 65521;
	}
	CHECK(bigEndian(stream, stream.size() - 4) == ((b << 16) | a));
	return out;
}

/// @brief Decodes a PNG the encoder wrote (8-bit RGB, one IDAT), checking every chunk's CRC.
/// @param png The file.
/// @param width Receives its width.
/// @param height Receives its height.
/// @return Its pixels, top row first.
std::vector<std::uint8_t> decode(const std::string& png, int& width, int& height) {
	REQUIRE(png.substr(0, 8) == std::string("\x89PNG\r\n\x1a\n"));
	std::string idat;
	std::vector<std::string> types;
	for(std::size_t at = 8; at < png.size();) {
		const std::uint32_t length = bigEndian(png, at);
		const std::string type = png.substr(at + 4, 4);
		const std::string data = png.substr(at + 8, length);
		CHECK(bigEndian(png, at + 8 + length) == crc32(png.substr(at + 4, 4 + length)));
		if(type == "IHDR") {
			width = static_cast<int>(bigEndian(data, 0));
			height = static_cast<int>(bigEndian(data, 4));
			CHECK(data.substr(8) == std::string("\x08\x02\x00\x00\x00", 5)); // 8-bit RGB, no interlace
		}
		if(type == "IDAT") idat += data;
		types.push_back(type);
		at += 12 + length;
	}
	CHECK(types == std::vector<std::string>{"IHDR", "IDAT", "IEND"});

	const std::vector<std::uint8_t> filtered = inflate(idat);
	const std::size_t stride = static_cast<std::size_t>(width) * 3;
	REQUIRE(filtered.size() == (stride + 1) * static_cast<std::size_t>(height));
	std::vector<std::uint8_t> rgb(stride * static_cast<std::size_t>(height));
	for(std::size_t y = 0; y < static_cast<std::size_t>(height); ++y) {
		const std::uint8_t filter = filtered[y * (stride + 1)];
		if(filter > 2) throw std::runtime_error("a row filter other than None, Sub or Up");
		for(std::size_t x = 0; x < stride; ++x) {
			const std::uint8_t left = x >= 3 ? rgb[y * stride + x - 3] : 0;
			const std::uint8_t up = y > 0 ? rgb[(y - 1) * stride + x] : 0;
			const std::uint8_t predicted = filter == 1 ? left : filter == 2 ? up : 0;
			rgb[y * stride + x] = static_cast<std::uint8_t>(filtered[y * (stride + 1) + 1 + x] + predicted);
		}
	}
	return rgb;
}

/// @brief Something like a screen: a background, a panel, and lines of grey shades standing in for text.
/// @param width In pixels.
/// @param height In pixels.
/// @return Its pixels.
std::vector<std::uint8_t> screenLike(int width, int height) {
	static constexpr std::array<std::uint8_t, 3> background = {0xF4, 0xF5, 0xF7}, panel = {0x2A, 0x5B, 0xC8};
	std::vector<std::uint8_t> rgb;
	rgb.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3);
	std::mt19937 random(7);
	for(int y = 0; y < height; ++y) {
		const bool panelRow = y > height / 5 && y < height / 2;
		const bool textRow = y % 20 < 8;
		for(int x = 0; x < width; ++x) {
			const auto& colour = panelRow && x > width / 4 ? panel : background;
			const bool ink = textRow && x % 90 < 60 && random() % 3 == 0;
			const auto shade = static_cast<std::uint8_t>(0x30 + 0x40 * (random() % 4)); // antialiased edges
			for(const std::uint8_t channel : colour) rgb.push_back(ink ? shade : channel);
		}
	}
	return rgb;
}

} // namespace

TEST_CASE("png: an image decodes to the same pixels") {
	struct Case {
		int width, height;             ///< Its size.
		std::vector<std::uint8_t> rgb; ///< Its pixels.
	};
	std::vector<std::uint8_t> noise(97 * 41 * 3);
	std::mt19937 random(42);
	for(std::uint8_t& byte : noise) byte = static_cast<std::uint8_t>(random());

	for(const Case& image :
	    {Case{1, 1, {0x12, 0x34, 0x56}}, Case{97, 41, noise}, Case{640, 400, screenLike(640, 400)}}) {
		CAPTURE(image.width);
		const std::string png = encodePng(image.width, image.height, image.rgb);
		int width = 0, height = 0;
		CHECK(decode(png, width, height) == image.rgb);
		CHECK(width == image.width);
		CHECK(height == image.height);
	}
}

TEST_CASE("png: flat areas, as on a screen, compress well") {
	const std::vector<std::uint8_t> rgb = screenLike(640, 400);
	const std::string png = encodePng(640, 400, rgb);
	CHECK(png.size() * 8 < rgb.size()); // an eighth at most, even with text scattered at random
}

TEST_CASE("png: pixels that don't match the size give no file") {
	const std::vector<std::uint8_t> rgb(10 * 10 * 3 - 1);
	CHECK(encodePng(10, 10, rgb).empty());
	CHECK(encodePng(0, 10, {}).empty());
}
