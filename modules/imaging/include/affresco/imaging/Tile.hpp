#pragma once

/// @file
/// @brief Tile: tileSize by tileSize pixels of one format, the unit an Image stores, shares, uploads and undoes.

#include "affresco/imaging/Pixels.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace affresco::imaging {

/// @brief A square of tileSize by tileSize pixels, straight alpha, top row first.
///
/// Images share tiles as std::shared_ptr<const Tile>: a copy of an image, an undo step and a mip all point at the
/// same tile until one of them writes, which copies it first (Image::writableTile).
class Tile {
public:
	/// @brief A transparent tile (all bytes zero).
	/// @param format Its format.
	explicit Tile(PixelFormat format);
	/// @brief A tile of the given bytes.
	/// @param format Its format.
	/// @param bytes tilePixels * bytesPerPixel(format) bytes.
	/// @throws std::invalid_argument If the size is wrong.
	Tile(PixelFormat format, std::vector<std::uint8_t> bytes);

	/// @brief A tile of one pixel everywhere.
	/// @param format Its format.
	/// @param pixel The pixel.
	/// @return The tile.
	[[nodiscard]] static std::shared_ptr<Tile> filled(PixelFormat format, const Pixel& pixel);

	/// @brief Its format.
	[[nodiscard]] PixelFormat format() const noexcept { return format_; }
	/// @brief Its bytes, tilePixels * bytesPerPixel(format()).
	[[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }
	/// @brief Its bytes, to write.
	[[nodiscard]] std::span<std::uint8_t> bytes() noexcept { return bytes_; }

	/// @brief The pixel at a place.
	/// @param x Its column, 0 to tileSize - 1.
	/// @param y Its row, 0 to tileSize - 1.
	/// @return The pixel; a mask's value is in alpha (and in red, green and blue).
	[[nodiscard]] Pixel pixel(int x, int y) const noexcept;
	/// @brief Writes a pixel; rgba8 and mask8 clamp it to 0 to 1.
	/// @param x Its column, 0 to tileSize - 1.
	/// @param y Its row, 0 to tileSize - 1.
	/// @param pixel The pixel; a mask takes alpha.
	void setPixel(int x, int y, const Pixel& pixel) noexcept;

	/// @brief The pixel every place holds, if they all hold the same bytes.
	[[nodiscard]] std::optional<Pixel> uniform() const noexcept;
	/// @brief Whether every byte is zero (transparent, or an empty mask).
	[[nodiscard]] bool empty() const noexcept;
	/// @brief The memory it takes.
	[[nodiscard]] std::size_t memoryBytes() const noexcept { return sizeof(Tile) + bytes_.capacity(); }

private:
	PixelFormat format_;              ///< See format().
	std::vector<std::uint8_t> bytes_; ///< See bytes().
};

/// @brief Reads one pixel of a tile that may be absent (transparent).
/// @param tile The tile, or null.
/// @param x Its column.
/// @param y Its row.
/// @return The pixel, or transparent.
[[nodiscard]] inline Pixel pixelOf(const Tile* tile, int x, int y) noexcept {
	return tile != nullptr ? tile->pixel(x, y) : Pixel{};
}

/// @brief The tile of a format, converted from another: rgba to mask takes alpha, mask to rgba makes white.
/// @param tile The tile.
/// @param format The format wanted.
/// @return A new tile.
[[nodiscard]] std::shared_ptr<Tile> convertTile(const Tile& tile, PixelFormat format);

} // namespace affresco::imaging
