#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

namespace sgl::detail {

// Writes 8-bit RGB pixels (top row first) as a binary PPM (P6) image
inline bool writePpm(const std::filesystem::path& path, int width, int height, std::span<const std::uint8_t> rgb) {
	if(rgb.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3) return false;

	std::ofstream file(path, std::ios::binary);
	if(!file) return false;

	file << "P6\n" << width << ' ' << height << "\n255\n";
	file.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));

	return static_cast<bool>(file);
}

} // namespace sgl::detail
