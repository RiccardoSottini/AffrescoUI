/// @file
/// @brief Image: copy-on-write tiles, the cache of loaded tiles and mips, and versions.

#include "affresco/imaging/Image.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace affresco::imaging {

namespace {

/// @brief Tiles needed to cover a length at a level.
/// @param length The length in pixels at level 0.
/// @param level The level.
/// @return At least 1.
[[nodiscard]] int tilesFor(int length, int level) noexcept {
	const int scaled = std::max(1, (length + (1 << level) - 1) >> level);
	return (scaled + tileSize - 1) / tileSize;
}

/// @brief The memory a cached tile takes.
/// @param tile The tile, or null.
[[nodiscard]] std::size_t bytesOf(const std::shared_ptr<const Tile>& tile) noexcept {
	return tile ? tile->memoryBytes() : sizeof(Tile);
}

} // namespace

Image::Image(int width, int height, PixelFormat format, const Limits& limits, std::shared_ptr<TileSource> source)
    : width_(width), height_(height), format_(format), limits_(limits), source_(std::move(source)) {
	if(width < 1 || height < 1 || width > limits.side || height > limits.side ||
	   static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > limits.pixels)
		throw std::invalid_argument("imaging: an image of " + std::to_string(width) + " by " + std::to_string(height) +
		                            " is beyond the limits (" + std::to_string(limits.side) + " a side, " +
		                            std::to_string(limits.pixels) + " pixels)");
	while(std::max(width_, height_) > (tileSize << (levels_ - 1))) ++levels_;
}

int Image::columns(int level) const noexcept { return tilesFor(width_, level); }

int Image::rows(int level) const noexcept { return tilesFor(height_, level); }

bool Image::contains(const TileKey& key) const noexcept {
	return key.level >= 0 && key.level < levels_ && key.column >= 0 && key.row >= 0 &&
	       key.column < columns(key.level) && key.row < rows(key.level);
}

std::shared_ptr<const Tile> Image::tile(const TileKey& key) const {
	if(!contains(key)) return nullptr;
	if(key.level == 0)
		if(const auto found = written_.find(key); found != written_.end()) return found->second.tile;
	return loadOrMake(key);
}

std::shared_ptr<const Tile> Image::loadOrMake(const TileKey& key) const {
	if(const auto found = cache_.find(key); found != cache_.end()) {
		recency_.splice(recency_.begin(), recency_, found->second.position);
		return found->second.tile;
	}
	std::shared_ptr<const Tile> made;
	const bool fromSource =
	    source_ && (key.level == 0 || (source_->providesLevel(key.level) && !mipVersions_.contains(key)));
	if(fromSource) {
		made = source_->load(key);
		if(made && made->format() != format_) throw std::runtime_error("imaging: a tile source gave another format");
	} else if(key.level > 0) made = makeMip(key);
	else return nullptr; // never written, no source: transparent, nothing to cache
	remember(key, made);
	return made;
}

std::shared_ptr<const Tile> Image::makeMip(const TileKey& key) const {
	std::array<std::shared_ptr<const Tile>, 4> children;
	for(int index = 0; index < 4; ++index)
		children[static_cast<std::size_t>(index)] =
		    tile({key.level - 1, key.column * 2 + (index & 1), key.row * 2 + (index >> 1)});
	return downsample(children, format_);
}

void Image::remember(const TileKey& key, std::shared_ptr<const Tile> tile) const {
	cachedBytes_ += bytesOf(tile);
	recency_.push_front(key);
	cache_[key] = Cached{std::move(tile), recency_.begin()};
	while(cachedBytes_ > limits_.cacheBytes && recency_.size() > 1) forget(recency_.back());
}

void Image::forget(const TileKey& key) const {
	const auto found = cache_.find(key);
	if(found == cache_.end()) return;
	cachedBytes_ -= bytesOf(found->second.tile);
	recency_.erase(found->second.position);
	cache_.erase(found);
}

void Image::check(int column, int row) const {
	if(!contains({0, column, row}))
		throw std::out_of_range("imaging: tile " + std::to_string(column) + ", " + std::to_string(row) +
		                        " is outside the image");
}

std::uint64_t Image::touched(int column, int row) {
	for(int level = 1; level < levels_; ++level) {
		const TileKey mip{level, column >> level, row >> level};
		forget(mip);
		mipVersions_[mip] = ++serial_;
	}
	return ++serial_;
}

Tile& Image::writableTile(int column, int row) {
	check(column, row);
	const TileKey key{0, column, row};
	auto found = written_.find(key);
	if(found == written_.end()) {
		const std::shared_ptr<const Tile> current = loadOrMake(key);
		forget(key);
		found = written_.emplace(key, Written{current ? std::make_shared<Tile>(*current) : nullptr, 0}).first;
	}
	Written& written = found->second;
	if(!written.tile) written.tile = std::make_shared<Tile>(format_);
	else if(written.tile.use_count() > 1) written.tile = std::make_shared<Tile>(*written.tile); // shared: copy on write
	written.version = touched(column, row);
	return *written.tile;
}

void Image::setTile(int column, int row, std::shared_ptr<const Tile> tile) {
	check(column, row);
	if(tile && tile->format() != format_) throw std::invalid_argument("imaging: a tile of another format");
	const TileKey key{0, column, row};
	forget(key);
	// Writing copies a shared tile first (writableTile), so holding it as writable is safe
	written_[key] = Written{std::const_pointer_cast<Tile>(std::move(tile)), touched(column, row)};
}

std::uint64_t Image::version(const TileKey& key) const noexcept {
	if(key.level == 0) {
		const auto found = written_.find(key);
		return found != written_.end() ? found->second.version : 0;
	}
	const auto found = mipVersions_.find(key);
	return found != mipVersions_.end() ? found->second : 0;
}

Pixel Image::pixel(int x, int y) const {
	if(x < 0 || y < 0 || x >= width_ || y >= height_) return {};
	const auto tile = this->tile({0, x / tileSize, y / tileSize});
	return pixelOf(tile.get(), x % tileSize, y % tileSize);
}

void Image::setPixel(int x, int y, const Pixel& pixel) {
	if(x < 0 || y < 0 || x >= width_ || y >= height_) return;
	writableTile(x / tileSize, y / tileSize).setPixel(x % tileSize, y % tileSize, pixel);
}

std::vector<TileKey> Image::writtenTiles() const {
	std::vector<TileKey> keys;
	keys.reserve(written_.size());
	for(const auto& [key, written] : written_) keys.push_back(key);
	return keys;
}

std::size_t Image::memoryBytes() const noexcept {
	std::size_t bytes = cachedBytes_;
	for(const auto& [key, written] : written_) bytes += written.tile ? written.tile->memoryBytes() : 0;
	return bytes;
}

std::shared_ptr<Tile> downsample(const std::array<std::shared_ptr<const Tile>, 4>& children, PixelFormat format) {
	if(!children[0] && !children[1] && !children[2] && !children[3]) return nullptr;
	auto tile = std::make_shared<Tile>(format);
	constexpr int half = tileSize / 2;
	for(std::size_t index = 0; index < 4; ++index) {
		const Tile* child = children[index].get();
		if(child == nullptr) continue;
		const int left = static_cast<int>(index & 1) * half, top = static_cast<int>(index >> 1) * half;
		for(int y = 0; y < half; ++y)
			for(int x = 0; x < half; ++x) {
				Pixel sum;
				float weight = 0.0f;
				for(int corner = 0; corner < 4; ++corner) {
					const Pixel pixel = child->pixel(x * 2 + (corner & 1), y * 2 + (corner >> 1));
					const float alpha = format == PixelFormat::mask8 ? 1.0f : pixel.alpha;
					sum = {sum.red + pixel.red * alpha, sum.green + pixel.green * alpha, sum.blue + pixel.blue * alpha,
					       sum.alpha + pixel.alpha};
					weight += alpha;
				}
				const float scale = weight > 0.0f ? 1.0f / weight : 0.0f;
				tile->setPixel(left + x, top + y,
				               {sum.red * scale, sum.green * scale, sum.blue * scale, sum.alpha * 0.25f});
			}
	}
	return tile;
}

} // namespace affresco::imaging
