#pragma once

/// @file
/// @brief Image: a sparse grid of copy-on-write tiles, loaded lazily from a TileSource, with mip levels made on demand.

#include "affresco/imaging/Limits.hpp"
#include "affresco/imaging/Tile.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

namespace affresco::imaging {

/// @brief Which tile: its mip level (0 the full size, each level half the one before) and its place in that level.
struct TileKey {
	int level = 0;  ///< The mip level.
	int column = 0; ///< The column of tiles, from the left.
	int row = 0;    ///< The row of tiles, from the top.

	/// @brief Whether two keys are the same.
	[[nodiscard]] friend constexpr bool operator==(const TileKey&, const TileKey&) noexcept = default;
};

/// @brief Hashes a TileKey.
struct TileKeyHash {
	/// @brief The hash.
	/// @param key The key.
	/// @return Its hash.
	[[nodiscard]] std::size_t operator()(const TileKey& key) const noexcept {
		const auto mix = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(key.column)) << 32) ^
		                 static_cast<std::uint32_t>(key.row) ^ (static_cast<std::uint64_t>(key.level) << 58);
		return std::hash<std::uint64_t>{}(mix);
	}
};

/// @brief Where an image's untouched tiles come from: a project file, a procedural image. Loaded on first use.
class TileSource {
public:
	virtual ~TileSource() = default;
	TileSource() = default;
	TileSource(const TileSource&) = delete;
	TileSource& operator=(const TileSource&) = delete;

	/// @brief Whether it gives the tiles of a mip level itself (else the image makes them from the level below).
	/// @param level The level, at least 1.
	[[nodiscard]] virtual bool providesLevel(int level) const = 0;
	/// @brief Loads a tile.
	/// @param key Which; its level is 0 or one providesLevel() said it gives.
	/// @return The tile in the image's format, or null for a transparent one.
	/// @throws std::runtime_error If it can't be read.
	[[nodiscard]] virtual std::shared_ptr<const Tile> load(const TileKey& key) = 0;
};

/// @brief An image of up to Limits::side pixels a side: tiles of tileSize pixels, absent ones transparent.
///
/// Tiles are shared, copy-on-write: copying an image, or keeping a tile for undo, copies pointers; writing to a
/// shared tile copies it first. Tiles not yet written come from the TileSource, if any, on first use, and stay in a
/// cache of Limits::cacheBytes, the least recently used going first; mip levels too, made from the level below
/// (alpha-weighted, so transparent pixels don't darken edges) unless the source gives them. Writing a tile makes its
/// mips again. Each tile has a version that changes when it does, for GPU caches. One thread at a time.
class Image {
public:
	/// @brief A transparent image, or one whose tiles come from a source.
	/// @param width Its width, 1 to limits.side.
	/// @param height Its height, 1 to limits.side.
	/// @param format The format of its tiles.
	/// @param limits Its bounds.
	/// @param source Where untouched tiles come from, or null.
	/// @throws std::invalid_argument If the size is beyond the limits.
	Image(int width, int height, PixelFormat format, const Limits& limits = {},
	      std::shared_ptr<TileSource> source = nullptr);

	/// @brief Its width, in pixels.
	[[nodiscard]] int width() const noexcept { return width_; }
	/// @brief Its height, in pixels.
	[[nodiscard]] int height() const noexcept { return height_; }
	/// @brief The format of its tiles.
	[[nodiscard]] PixelFormat format() const noexcept { return format_; }
	/// @brief How many mip levels it has: 1 + as many halvings as make it fit one tile.
	[[nodiscard]] int levels() const noexcept { return levels_; }
	/// @brief Columns of tiles at a level.
	/// @param level The level.
	[[nodiscard]] int columns(int level = 0) const noexcept;
	/// @brief Rows of tiles at a level.
	/// @param level The level.
	[[nodiscard]] int rows(int level = 0) const noexcept;
	/// @brief Whether a key is within the image.
	/// @param key The key.
	[[nodiscard]] bool contains(const TileKey& key) const noexcept;

	/// @brief A tile of any level, loaded or made if needed.
	/// @param key Which.
	/// @return The tile, or null if transparent or outside the image.
	[[nodiscard]] std::shared_ptr<const Tile> tile(const TileKey& key) const;
	/// @brief A level-0 tile to write, copied first if shared; its version and its mips' change.
	/// @param column Its column, within the image.
	/// @param row Its row, within the image.
	/// @return The tile, valid until the image next changes.
	/// @throws std::out_of_range Outside the image.
	[[nodiscard]] Tile& writableTile(int column, int row);
	/// @brief Puts a level-0 tile in place (undo, a file's tiles): shared, not copied.
	/// @param column Its column.
	/// @param row Its row.
	/// @param tile The tile in this image's format, or null for transparent.
	/// @throws std::out_of_range Outside the image; std::invalid_argument for another format.
	void setTile(int column, int row, std::shared_ptr<const Tile> tile);
	/// @brief A version of a tile that changes whenever the tile does: 0 for one never written.
	/// @param key Which.
	[[nodiscard]] std::uint64_t version(const TileKey& key) const noexcept;

	/// @brief The pixel at a place; outside the image, transparent.
	/// @param x Its column.
	/// @param y Its row.
	[[nodiscard]] Pixel pixel(int x, int y) const;
	/// @brief Writes a pixel; outside the image, nothing.
	/// @param x Its column.
	/// @param y Its row.
	/// @param pixel The pixel.
	void setPixel(int x, int y, const Pixel& pixel);

	/// @brief The level-0 tiles written since it was made (not those only in its source).
	[[nodiscard]] std::vector<TileKey> writtenTiles() const;
	/// @brief The tile source, or null.
	[[nodiscard]] const std::shared_ptr<TileSource>& source() const noexcept { return source_; }
	/// @brief Memory of the written tiles and of the cache (shared tiles counted in full).
	[[nodiscard]] std::size_t memoryBytes() const noexcept;
	/// @brief Memory of the cache alone: loaded tiles and mips.
	[[nodiscard]] std::size_t cachedBytes() const noexcept { return cachedBytes_; }
	/// @brief Its limits.
	[[nodiscard]] const Limits& limits() const noexcept { return limits_; }

private:
	/// @brief A tile written to this image.
	struct Written {
		std::shared_ptr<Tile> tile; ///< The tile, or null for transparent.
		std::uint64_t version = 0;  ///< Its version.
	};
	/// @brief A tile in the cache.
	struct Cached {
		std::shared_ptr<const Tile> tile;      ///< The tile, or null for transparent.
		std::list<TileKey>::iterator position; ///< Its place in recency_.
	};

	/// @brief A tile from the cache, the source, or made from the level below, cached.
	/// @param key Which (not a written level-0 tile).
	[[nodiscard]] std::shared_ptr<const Tile> loadOrMake(const TileKey& key) const;
	/// @brief Makes a mip tile from the four below it.
	/// @param key Which, level 1 or more.
	[[nodiscard]] std::shared_ptr<const Tile> makeMip(const TileKey& key) const;
	/// @brief Puts a tile in the cache and drops the least recently used beyond the budget.
	/// @param key Which.
	/// @param tile The tile, or null.
	void remember(const TileKey& key, std::shared_ptr<const Tile> tile) const;
	/// @brief Drops a tile from the cache.
	/// @param key Which.
	void forget(const TileKey& key) const;
	/// @brief After a level-0 tile changed: new versions for it and its mips, which are made again.
	/// @param column Its column.
	/// @param row Its row.
	/// @return The tile's new version.
	std::uint64_t touched(int column, int row);
	/// @brief Throws if a level-0 place is outside the image.
	/// @param column The column.
	/// @param row The row.
	void check(int column, int row) const;

	int width_;                                                           ///< See width().
	int height_;                                                          ///< See height().
	PixelFormat format_;                                                  ///< See format().
	int levels_ = 1;                                                      ///< See levels().
	Limits limits_;                                                       ///< See limits().
	std::shared_ptr<TileSource> source_;                                  ///< See source().
	std::unordered_map<TileKey, Written, TileKeyHash> written_;           ///< The level-0 tiles written.
	std::unordered_map<TileKey, std::uint64_t, TileKeyHash> mipVersions_; ///< Mips whose level-0 tiles changed.
	std::uint64_t serial_ = 0;                                            ///< The last version given.
	mutable std::unordered_map<TileKey, Cached, TileKeyHash> cache_;      ///< Loaded tiles and made mips.
	mutable std::list<TileKey> recency_;                                  ///< The cache's keys, most recent first.
	mutable std::size_t cachedBytes_ = 0;                                 ///< See cachedBytes().
};

/// @brief Downsamples four tiles into one of the next level, alpha-weighted.
/// @param children The top-left, top-right, bottom-left and bottom-right tiles (each may be null).
/// @param format The format.
/// @return The tile, or null if all four are null.
[[nodiscard]] std::shared_ptr<Tile> downsample(const std::array<std::shared_ptr<const Tile>, 4>& children,
                                               PixelFormat format);

} // namespace affresco::imaging
