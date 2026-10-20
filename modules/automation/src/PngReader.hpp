#pragma once

/// @file
/// @brief detail::decodePng: reads the golden images image expectations compare with.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace affresco::automation::detail {

/// @brief The most pixels a decoded picture may have: 32 Mi, about 96 MiB of RGB, a 4,096 × 4,096 window at scale 2
/// twice over.
inline constexpr std::size_t maxPicturePixels = std::size_t{1} << 25;

/// @brief An 8-bit RGB picture, top row first.
struct Pixels {
	int width = 0;                 ///< In pixels.
	int height = 0;                ///< In pixels.
	std::vector<std::uint8_t> rgb; ///< width * height * 3 bytes.
};

/// @brief Decodes a PNG file: 8-bit RGB or RGBA (the alpha is dropped), not interlaced, any row filter. That is what
/// the rendering check and the library's encoder write; a golden image is any file in the goldens folder, so the
/// rest is refused, never trusted.
/// @param file The file's bytes.
/// @param error Receives why it can't be decoded.
/// @return The picture, or std::nullopt.
[[nodiscard]] std::optional<Pixels> decodePng(std::span<const std::uint8_t> file, std::string& error);

/// @brief Decompresses a zlib stream (RFC 1950 around RFC 1951 deflate), checking its Adler-32.
/// @param stream The stream.
/// @param expected How many bytes it must give: one more or fewer is an error, and the output never grows past it.
/// @param error Receives why it can't be decompressed.
/// @return The bytes, or std::nullopt.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> inflateZlib(std::span<const std::uint8_t> stream,
                                                                   std::size_t expected, std::string& error);

} // namespace affresco::automation::detail
