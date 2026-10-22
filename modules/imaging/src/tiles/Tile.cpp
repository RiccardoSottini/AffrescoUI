/// @file
/// @brief Tile: reading and writing its pixels in each format.

#include "affresco/imaging/Tile.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace affresco::imaging {

namespace {

/// @brief Reads a little-endian 16-bit value.
/// @param bytes Where it is.
/// @return The value.
[[nodiscard]] std::uint16_t read16(const std::uint8_t* bytes) noexcept {
	return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

/// @brief Writes a little-endian 16-bit value.
/// @param bytes Where.
/// @param value The value.
void write16(std::uint8_t* bytes, std::uint16_t value) noexcept {
	bytes[0] = static_cast<std::uint8_t>(value & 0xffu);
	bytes[1] = static_cast<std::uint8_t>(value >> 8);
}

/// @brief The byte offset of a pixel.
/// @param x Its column.
/// @param y Its row.
/// @param format The format.
/// @return The offset.
[[nodiscard]] std::size_t offsetOf(int x, int y, PixelFormat format) noexcept {
	return (static_cast<std::size_t>(y) * tileSize + static_cast<std::size_t>(x)) * bytesPerPixel(format);
}

} // namespace

Tile::Tile(PixelFormat format) : format_(format), bytes_(tilePixels * bytesPerPixel(format), 0) {}

Tile::Tile(PixelFormat format, std::vector<std::uint8_t> bytes) : format_(format), bytes_(std::move(bytes)) {
	if(bytes_.size() != tilePixels * bytesPerPixel(format))
		throw std::invalid_argument("imaging: a tile of this format has " +
		                            std::to_string(tilePixels * bytesPerPixel(format)) + " bytes, not " +
		                            std::to_string(bytes_.size()));
}

std::shared_ptr<Tile> Tile::filled(PixelFormat format, const Pixel& pixel) {
	auto tile = std::make_shared<Tile>(format);
	tile->setPixel(0, 0, pixel);
	const std::size_t size = bytesPerPixel(format);
	std::uint8_t* bytes = tile->bytes_.data();
	for(std::size_t offset = size; offset < tile->bytes_.size(); offset += size)
		std::memcpy(bytes + offset, bytes, size);
	return tile;
}

Pixel Tile::pixel(int x, int y) const noexcept {
	const std::uint8_t* at = bytes_.data() + offsetOf(x, y, format_);
	switch(format_) {
	case PixelFormat::rgba8:
		return {unitFromByte(at[0]), unitFromByte(at[1]), unitFromByte(at[2]), unitFromByte(at[3])};
	case PixelFormat::rgba16f:
		return {floatFromHalf(read16(at)), floatFromHalf(read16(at + 2)), floatFromHalf(read16(at + 4)),
		        floatFromHalf(read16(at + 6))};
	case PixelFormat::mask8: {
		const float value = unitFromByte(at[0]);
		return {value, value, value, value};
	}
	}
	return {};
}

void Tile::setPixel(int x, int y, const Pixel& pixel) noexcept {
	std::uint8_t* at = bytes_.data() + offsetOf(x, y, format_);
	switch(format_) {
	case PixelFormat::rgba8:
		at[0] = byteFromUnit(pixel.red);
		at[1] = byteFromUnit(pixel.green);
		at[2] = byteFromUnit(pixel.blue);
		at[3] = byteFromUnit(pixel.alpha);
		return;
	case PixelFormat::rgba16f:
		write16(at, halfFromFloat(pixel.red));
		write16(at + 2, halfFromFloat(pixel.green));
		write16(at + 4, halfFromFloat(pixel.blue));
		write16(at + 6, halfFromFloat(std::clamp(pixel.alpha, 0.0f, 1.0f)));
		return;
	case PixelFormat::mask8:
		at[0] = byteFromUnit(pixel.alpha);
		return;
	}
}

std::optional<Pixel> Tile::uniform() const noexcept {
	const std::size_t size = bytesPerPixel(format_);
	const std::uint8_t* bytes = bytes_.data();
	for(std::size_t offset = size; offset < bytes_.size(); offset += size)
		if(std::memcmp(bytes + offset, bytes, size) != 0) return std::nullopt;
	return pixel(0, 0);
}

bool Tile::empty() const noexcept {
	return std::all_of(bytes_.begin(), bytes_.end(), [](std::uint8_t byte) { return byte == 0; });
}

std::shared_ptr<Tile> convertTile(const Tile& tile, PixelFormat format) {
	auto converted = std::make_shared<Tile>(format);
	for(int y = 0; y < tileSize; ++y)
		for(int x = 0; x < tileSize; ++x) {
			Pixel pixel = tile.pixel(x, y);
			if(tile.format() == PixelFormat::mask8) pixel = {1.0f, 1.0f, 1.0f, pixel.alpha};
			converted->setPixel(x, y, pixel);
		}
	return converted;
}

} // namespace affresco::imaging
