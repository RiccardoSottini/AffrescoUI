#pragma once

/// @file
/// @brief PNG encoding and PPM reading, for pictures of the screen sent to peers.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace affresco::detail {

/// @brief An 8-bit RGB image, top row first.
struct RgbImage {
	int width = 0;                 ///< In pixels.
	int height = 0;                ///< In pixels.
	std::vector<std::uint8_t> rgb; ///< width * height * 3 bytes.
};

/// @brief Reads a binary PPM (P6, maximum value 255), as Window::saveScreenshot writes it.
/// @param path The file.
/// @return The image, or std::nullopt if the file can't be read or isn't such a PPM.
[[nodiscard]] std::optional<RgbImage> readPpm(const std::filesystem::path& path);

/// @brief Encodes an image as a PNG file: 8-bit RGB, each row filtered, then compressed with fixed-Huffman deflate.
///
/// Screens are mostly flat colour, which the filters turn into runs of zeros that compress well: no library needed.
/// @param width In pixels (at least 1).
/// @param height In pixels (at least 1).
/// @param rgb width * height * 3 bytes, top row first.
/// @return The file's bytes, or an empty string if @p rgb has the wrong size.
[[nodiscard]] std::string encodePng(int width, int height, std::span<const std::uint8_t> rgb);

} // namespace affresco::detail
