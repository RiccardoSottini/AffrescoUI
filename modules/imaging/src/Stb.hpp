#pragma once

/// @file
/// @brief The imaging module's own build of stb_image and stb_image_write (private): straight alpha and 16 bits, which
/// render's Bitmap doesn't give, and zlib for ICC profiles and project tiles. Every allocation is bounded.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace affresco::imaging::stb {

/// @brief What a PNG or JPEG header says.
struct Header {
	int width = 0;          ///< Its width.
	int height = 0;         ///< Its height.
	bool sixteenBit = false; ///< Whether it has 16 bits a channel (PNG only).
};

/// @brief Decoded pixels: RGBA, straight alpha, top row first.
struct Decoded {
	int width = 0;                    ///< Their width.
	int height = 0;                   ///< Their height.
	std::vector<std::uint8_t> bytes8; ///< 8 bits a channel, when not sixteenBit.
	std::vector<std::uint16_t> bytes16; ///< 16 bits a channel, when the file has them.
};

/// @brief Reads a header without decoding.
/// @param file The PNG or JPEG.
/// @return The header, or nothing if it isn't one stb reads.
[[nodiscard]] std::optional<Header> header(std::span<const std::uint8_t> file) noexcept;

/// @brief Decodes a PNG or JPEG to RGBA.
/// @param file The file, whose header() was checked against the limits.
/// @param header Its header.
/// @return The pixels, or nothing if the file is broken.
[[nodiscard]] std::optional<Decoded> decode(std::span<const std::uint8_t> file, const Header& header);

/// @brief Inflates a zlib stream (with its header), at most a number of bytes.
/// @param stream The stream.
/// @param most The most bytes it may inflate to.
/// @return The bytes, or nothing if it is broken or inflates to more.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> inflate(std::span<const std::uint8_t> stream, std::size_t most);

/// @brief Deflates bytes to a zlib stream (with its header).
/// @param bytes The bytes, fewer than 2 GiB.
/// @return The stream.
[[nodiscard]] std::vector<std::uint8_t> deflate(std::span<const std::uint8_t> bytes);

/// @brief Encodes RGBA (8 bits, straight alpha) as a PNG.
/// @param rgba width * height * 4 bytes.
/// @param width The width.
/// @param height The height.
/// @return The file, or empty if stb failed.
[[nodiscard]] std::vector<std::uint8_t> writePng(std::span<const std::uint8_t> rgba, int width, int height);

/// @brief Encodes RGBA (8 bits; alpha dropped) as a baseline JPEG.
/// @param rgba width * height * 4 bytes.
/// @param width The width.
/// @param height The height.
/// @param quality 1 to 100.
/// @return The file, or empty if stb failed.
[[nodiscard]] std::vector<std::uint8_t> writeJpeg(std::span<const std::uint8_t> rgba, int width, int height,
                                                  int quality);

} // namespace affresco::imaging::stb
