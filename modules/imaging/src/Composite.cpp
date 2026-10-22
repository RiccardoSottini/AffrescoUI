/// @file
/// @brief The CPU compositor, flattening and thumbnails.

#include "affresco/imaging/Composite.hpp"

#include <algorithm>

namespace affresco::imaging {

Compositor::Compositor(const Document& document) : document_(document) {
	const auto depths = static_cast<std::size_t>(std::max(document.limits().groupDepth, 0)) + 2;
	groups_.resize(depths);
	bases_.resize(depths);
}

void Compositor::composite(const TileKey& key, std::span<Pixel> out) {
	std::fill(out.begin(), out.end(), Pixel{});
	compositeList(document_.structure().root, key, out, 0);
}

void Compositor::compositeList(const std::vector<std::uint64_t>& list, const TileKey& key, std::span<Pixel> backdrop,
                               std::size_t depth) {
	if(depth >= bases_.size()) return; // deeper than the limits allow: never made by a Document
	std::vector<float>& base = bases_[depth];
	base.assign(tilePixels, 0.0f);
	for(const std::uint64_t id : list) {
		const Layer* layer = document_.layer(id);
		if(layer == nullptr) continue;
		if(!layer->clipped && !layer->visible) std::fill(base.begin(), base.end(), 0.0f);
		if(layer->visible) compositeLayer(*layer, key, backdrop, depth);
	}
}

void Compositor::compositeLayer(const Layer& layer, const TileKey& key, std::span<Pixel> backdrop, std::size_t depth) {
	std::shared_ptr<const Tile> pixels;
	if(layer.kind == LayerKind::pixel)
		if(const Image* image = document_.image(layer.pixels); image != nullptr && image->contains(key))
			pixels = image->tile(key);
	std::shared_ptr<const Tile> mask;
	const Image* maskImage = layer.mask != 0 && layer.maskEnabled ? document_.image(layer.mask) : nullptr;
	if(maskImage != nullptr && maskImage->contains(key)) mask = maskImage->tile(key);
	if(layer.kind == LayerKind::group) {
		if(depth + 1 >= groups_.size()) return;
		std::vector<Pixel>& group = groups_[depth + 1];
		group.assign(tilePixels, Pixel{});
		compositeList(layer.children, key, group, depth + 1);
	}
	std::vector<float>& base = bases_[depth];
	for(int y = 0; y < tileSize; ++y)
		for(int x = 0; x < tileSize; ++x) {
			const auto at = static_cast<std::size_t>(y * tileSize + x);
			const float covered = maskImage == nullptr ? 1.0f : pixelOf(mask.get(), x, y).alpha;
			Pixel source;
			switch(layer.kind) {
			case LayerKind::pixel:
				source = pixelOf(pixels.get(), x, y);
				break;
			case LayerKind::group:
				source = groups_[depth + 1][at];
				break;
			case LayerKind::adjustment:
				source = adjustPixel(layer.adjustment, backdrop[at]);
				break;
			}
			const float own = (layer.kind == LayerKind::adjustment ? 1.0f : source.alpha) * covered;
			const float coverage = layer.clipped ? base[at] : 1.0f;
			if(!layer.clipped) base[at] = own;
			if(layer.kind == LayerKind::adjustment) source.alpha = backdrop[at].alpha;
			backdrop[at] = blendPixels(backdrop[at], source, layer.blend, layer.opacity * covered * coverage);
		}
}

Sampler::Sampler(const Document& document, std::uint64_t image)
    : document_(document), image_(image != 0 ? document.image(image) : nullptr), compositor_(document) {
	entries_.resize(kept);
}

Pixel Sampler::at(int x, int y) {
	if(x < 0 || y < 0 || x >= document_.width() || y >= document_.height()) return {};
	const Entry& last = entries_[last_];
	if(last.column == x / tileSize && last.row == y / tileSize)
		return last.pixels[static_cast<std::size_t>((y % tileSize) * tileSize + x % tileSize)];
	return tile(x / tileSize, y / tileSize)[static_cast<std::size_t>((y % tileSize) * tileSize + x % tileSize)];
}

std::span<const Pixel> Sampler::tile(int column, int row) {
	++clock_;
	std::size_t oldest = 0;
	for(std::size_t index = 0; index < entries_.size(); ++index) {
		Entry& entry = entries_[index];
		if(entry.column == column && entry.row == row) {
			entry.used = clock_, last_ = index;
			return entry.pixels;
		}
		if(entry.used < entries_[oldest].used) oldest = index;
	}
	Entry& entry = entries_[oldest];
	entry.column = column, entry.row = row, entry.used = clock_, last_ = oldest;
	entry.pixels.resize(tilePixels);
	if(image_ == nullptr) {
		compositor_.composite({0, column, row}, entry.pixels);
		return entry.pixels;
	}
	const std::shared_ptr<const Tile> source = image_->tile({0, column, row});
	for(int y = 0; y < tileSize; ++y)
		for(int x = 0; x < tileSize; ++x)
			entry.pixels[static_cast<std::size_t>(y * tileSize + x)] = pixelOf(source.get(), x, y);
	return entry.pixels;
}

std::vector<std::uint8_t> flatten(const Document& document, int level, int x, int y, int width, int height) {
	std::vector<std::uint8_t> out(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4, 0);
	Compositor compositor(document);
	std::vector<Pixel> tile(tilePixels);
	const int levelWidth = std::max(1, document.width() >> level),
	          levelHeight = std::max(1, document.height() >> level);
	const int right = std::min(x + width, levelWidth), bottom = std::min(y + height, levelHeight);
	for(int row = std::max(y, 0) / tileSize; row * tileSize < bottom; ++row)
		for(int column = std::max(x, 0) / tileSize; column * tileSize < right; ++column) {
			compositor.composite({level, column, row}, tile);
			const int top = std::max(row * tileSize, y), left = std::max(column * tileSize, x);
			for(int py = top; py < std::min((row + 1) * tileSize, bottom); ++py)
				for(int px = left; px < std::min((column + 1) * tileSize, right); ++px) {
					const Pixel& pixel =
					    tile[static_cast<std::size_t>((py - row * tileSize) * tileSize + px - column * tileSize)];
					std::uint8_t* to = &out[(static_cast<std::size_t>(py - y) * static_cast<std::size_t>(width) +
					                         static_cast<std::size_t>(px - x)) *
					                        4];
					to[0] = byteFromUnit(pixel.red), to[1] = byteFromUnit(pixel.green);
					to[2] = byteFromUnit(pixel.blue), to[3] = byteFromUnit(pixel.alpha);
				}
		}
	return out;
}

std::vector<std::uint8_t> thumbnail(const Document& document, std::uint64_t image, int side) {
	side = std::clamp(side, 8, 256);
	const Image* source = image != 0 ? document.image(image) : nullptr;
	if(image != 0 && source == nullptr) return std::vector<std::uint8_t>(static_cast<std::size_t>(side * side) * 4, 0);
	// The top mip level holds the whole image in one tile
	int level = 0;
	while(std::max(document.width(), document.height()) > (tileSize << level)) ++level;
	const int width = std::max(1, document.width() >> level), height = std::max(1, document.height() >> level);
	std::vector<std::uint8_t> top;
	if(source == nullptr) top = flatten(document, level, 0, 0, width, height);
	const std::shared_ptr<const Tile> tile = source != nullptr ? source->tile({level, 0, 0}) : nullptr;
	const float scale = static_cast<float>(std::max(width, height)) / static_cast<float>(side);
	const int fittedWidth = std::max(1, static_cast<int>(static_cast<float>(width) / scale));
	const int fittedHeight = std::max(1, static_cast<int>(static_cast<float>(height) / scale));
	std::vector<std::uint8_t> out(static_cast<std::size_t>(side * side) * 4, 0);
	for(int y = 0; y < fittedHeight; ++y)
		for(int x = 0; x < fittedWidth; ++x) {
			const int sx = std::min(width - 1, static_cast<int>((static_cast<float>(x) + 0.5f) * scale));
			const int sy = std::min(height - 1, static_cast<int>((static_cast<float>(y) + 0.5f) * scale));
			std::uint8_t* to = &out[static_cast<std::size_t>(
			    ((y + (side - fittedHeight) / 2) * side + x + (side - fittedWidth) / 2) * 4)];
			if(source == nullptr) {
				std::copy_n(&top[static_cast<std::size_t>((sy * width + sx) * 4)], 4, to);
				continue;
			}
			Pixel pixel = pixelOf(tile.get(), sx, sy);
			if(source->format() == PixelFormat::mask8) pixel = {pixel.alpha, pixel.alpha, pixel.alpha, 1.0f};
			to[0] = byteFromUnit(pixel.red), to[1] = byteFromUnit(pixel.green);
			to[2] = byteFromUnit(pixel.blue), to[3] = byteFromUnit(pixel.alpha);
		}
	return out;
}

} // namespace affresco::imaging
