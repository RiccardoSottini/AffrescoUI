#pragma once

/// @file
/// @brief PPM image writing, for screenshots.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

namespace affresco::detail {

/// @brief Writes a binary PPM (P6) image.
/// @param path The file.
/// @param width In pixels.
/// @param height In pixels.
/// @param rgb 8-bit RGB, top row first.
/// @return false if @p rgb has the wrong size or the file can't be written.
inline bool writePpm(const std::filesystem::path& path, int width, int height, std::span<const std::uint8_t> rgb) {
	if(rgb.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3) return false;

	std::ofstream file(path, std::ios::binary);
	if(!file) return false;

	file << "P6\n" << width << ' ' << height << "\n255\n";
	file.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));

	return static_cast<bool>(file);
}

} // namespace affresco::detail
