/// @file
/// @brief The magic wand, colour range, and selections combined into a document's.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Selection.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <vector>

namespace affresco::imaging {

namespace {

/// @brief The largest difference of a channel between two colours, 0 to 255.
/// @param left One.
/// @param right The other.
[[nodiscard]] float difference(const Pixel& left, const Pixel& right) noexcept {
	return 255.0f * std::max({std::abs(left.red - right.red), std::abs(left.green - right.green),
	                          std::abs(left.blue - right.blue), std::abs(left.alpha - right.alpha)});
}

/// @brief The image a selection tool samples: the active pixel layer's, or 0 for every layer flattened.
/// @param document The document.
/// @param allLayers Whether every layer is sampled.
[[nodiscard]] std::uint64_t sampledImage(const Document& document, bool allLayers) noexcept {
	const Layer* active = document.activeLayer();
	if(allLayers || active == nullptr || active->kind != LayerKind::pixel) return 0;
	return active->pixels;
}

/// @brief The covered pixels of a selection tile, scanning in from each edge until a covered pixel.
/// @param tile A mask8 tile.
/// @return Left, top, right and bottom (exclusive) within the tile, or nothing when it covers nothing.
[[nodiscard]] std::optional<std::array<int, 4>> coveredBounds(const Tile& tile) {
	const auto bytes = tile.bytes();
	const auto covered = [&](int x, int y) { return bytes[static_cast<std::size_t>(y * tileSize + x)] != 0; };
	const auto rowCovered = [&](int y) {
		for(int x = 0; x < tileSize; ++x)
			if(covered(x, y)) return true;
		return false;
	};
	int top = 0, bottom = tileSize;
	while(top < tileSize && !rowCovered(top)) ++top;
	if(top == tileSize) return std::nullopt;
	while(!rowCovered(bottom - 1)) --bottom;
	const auto columnCovered = [&](int x) {
		for(int y = top; y < bottom; ++y)
			if(covered(x, y)) return true;
		return false;
	};
	int left = 0, right = tileSize;
	while(!columnCovered(left)) ++left;
	while(!columnCovered(right - 1)) --right;
	return std::array<int, 4>{left, top, right, bottom};
}

/// @brief Marks the filled pixels of a contiguous fill, making their tiles.
class Marks {
public:
	/// @brief Nothing marked.
	explicit Marks(const Limits& limits) : limits_(limits) {}

	/// @brief Whether a pixel is marked.
	[[nodiscard]] bool marked(int x, int y) const {
		const auto found = tiles_.find({x / tileSize, y / tileSize});
		return found != tiles_.end() &&
		       found->second->bytes()[static_cast<std::size_t>((y % tileSize) * tileSize + x % tileSize)] != 0;
	}

	/// @brief Marks a pixel.
	/// @throws DocumentError beyond floodPixels.
	void mark(int x, int y) {
		if(++count_ > limits_.floodPixels)
			throw DocumentError("imaging: a region of more than " + std::to_string(limits_.floodPixels) + " pixels");
		auto& tile = tiles_[{x / tileSize, y / tileSize}];
		if(!tile) tile = std::make_shared<Tile>(PixelFormat::mask8);
		tile->bytes()[static_cast<std::size_t>((y % tileSize) * tileSize + x % tileSize)] = 255;
	}

	/// @brief The marks as a coverage.
	[[nodiscard]] Coverage coverage() && {
		Coverage coverage;
		for(auto& [place, tile] : tiles_) coverage.emplace(place, std::move(tile));
		return coverage;
	}

private:
	const Limits& limits_;                                       ///< The limits.
	std::map<std::pair<int, int>, std::shared_ptr<Tile>> tiles_; ///< The marked tiles.
	std::size_t count_ = 0;                                      ///< The pixels marked.
};

/// @brief A scanline flood fill from a pixel over the pixels like it.
/// @param document The document.
/// @param sampler What is sampled.
/// @param x The pixel across.
/// @param y The pixel down.
/// @param tolerance The largest difference.
[[nodiscard]] Coverage flood(const Document& document, Sampler& sampler, int x, int y, float tolerance) {
	Marks marks(document.limits());
	const Pixel seed = sampler.at(x, y);
	const auto like = [&](int px, int py) {
		return px >= 0 && px < document.width() && !marks.marked(px, py) &&
		       difference(sampler.at(px, py), seed) <= tolerance;
	};
	std::vector<std::pair<int, int>> stack{{x, y}};
	while(!stack.empty()) {
		auto [px, py] = stack.back();
		stack.pop_back();
		if(!like(px, py)) continue;
		int left = px, right = px;
		while(like(left - 1, py)) --left;
		while(like(right + 1, py)) ++right;
		for(int fill = left; fill <= right; ++fill) marks.mark(fill, py);
		for(const int next : {py - 1, py + 1}) {
			if(next < 0 || next >= document.height()) continue;
			for(int fill = left; fill <= right; ++fill)
				if(like(fill, next) && (fill == left || !like(fill - 1, next))) stack.emplace_back(fill, next);
		}
	}
	return std::move(marks).coverage();
}

/// @brief Each tile's coverage by a function of its pixels, absent where it selects nothing.
/// @param document The document.
/// @param sampler What is sampled.
/// @param selects The selection of a pixel, 0 to 1.
template <typename Selects>
[[nodiscard]] Coverage byPixel(const Document& document, Sampler& sampler, const Selects& selects) {
	Coverage coverage;
	const Image& shape = document.selection();
	for(int row = 0; row < shape.rows(); ++row)
		for(int column = 0; column < shape.columns(); ++column) {
			const std::span<const Pixel> pixels = sampler.tile(column, row);
			auto tile = std::make_shared<Tile>(PixelFormat::mask8);
			bool any = false;
			for(std::size_t at = 0; at < tilePixels; ++at) {
				tile->bytes()[at] = byteFromUnit(selects(pixels[at]));
				any = any || tile->bytes()[at] != 0;
			}
			if(any) coverage.emplace(std::pair{column, row}, std::move(tile));
		}
	return coverage;
}

/// @brief One tile of a selection combined with a coverage's.
/// @param old The selection's tile (null for none).
/// @param added The coverage's tile (null for none).
/// @param mode How they combine.
/// @return The tile, null when it selects nothing.
[[nodiscard]] std::shared_ptr<const Tile> combine(const std::shared_ptr<const Tile>& old,
                                                  const std::shared_ptr<const Tile>& added, SelectionMode mode) {
	if(mode == SelectionMode::replace || (mode == SelectionMode::add && !old)) return added;
	if((mode == SelectionMode::add && !added) || (mode == SelectionMode::subtract && !added)) return old;
	if(!old || (mode == SelectionMode::intersect && !added)) return nullptr;
	auto tile = std::make_shared<Tile>(PixelFormat::mask8);
	bool any = false;
	for(std::size_t at = 0; at < tilePixels; ++at) {
		const std::uint8_t was = old->bytes()[at], with = added->bytes()[at];
		std::uint8_t& to = tile->bytes()[at];
		switch(mode) {
		case SelectionMode::add:
			to = std::max(was, with);
			break;
		case SelectionMode::subtract:
			to = std::min(was, static_cast<std::uint8_t>(255 - with));
			break;
		default:
			to = std::min(was, with);
			break;
		}
		any = any || to != 0;
	}
	return any ? tile : nullptr;
}

} // namespace

Coverage wandCoverage(const Document& document, int x, int y, const SampleOptions& options) {
	if(x < 0 || y < 0 || x >= document.width() || y >= document.height()) return {};
	Sampler sampler(document, sampledImage(document, options.allLayers));
	const auto tolerance = static_cast<float>(std::clamp(options.tolerance, 0, 255)) + 0.5f;
	if(options.contiguous) return flood(document, sampler, x, y, tolerance);
	const Pixel seed = sampler.at(x, y);
	return byPixel(document, sampler,
	               [&](const Pixel& pixel) { return difference(pixel, seed) <= tolerance ? 1.0f : 0.0f; });
}

Coverage colorRangeCoverage(const Document& document, const Pixel& colour, int fuzziness, bool allLayers) {
	Sampler sampler(document, sampledImage(document, allLayers));
	const auto fuzz = static_cast<float>(std::clamp(fuzziness, 1, 255));
	return byPixel(document, sampler,
	               [&](const Pixel& pixel) { return clamped(1.0f - difference(pixel, colour) / fuzz); });
}

void select(Document& document, const std::string& name, const Coverage& coverage, SelectionMode mode) {
	const Image& selection = document.selection();
	if(!document.hasSelection() && mode != SelectionMode::subtract) mode = SelectionMode::replace;
	std::set<std::pair<int, int>> places;
	for(const auto& [place, tile] : coverage) places.insert(place);
	if(mode == SelectionMode::replace || mode == SelectionMode::intersect)
		for(const TileKey& key : selection.writtenTiles()) places.insert({key.column, key.row});
	document.begin(name);
	for(const auto& [column, row] : places) {
		if(!selection.contains({0, column, row})) continue;
		const std::shared_ptr<const Tile> there = selection.tile({0, column, row});
		const auto found = coverage.find({column, row});
		std::shared_ptr<const Tile> tile =
		    combine(document.hasSelection() ? there : nullptr, found != coverage.end() ? found->second : nullptr, mode);
		if(tile != there) document.replaceTile(Document::selectionId, column, row, std::move(tile));
	}
	document.commit(selectsAnything(selection));
}

void selectAll(Document& document) {
	const Image& selection = document.selection();
	const std::shared_ptr<const Tile> white = Tile::filled(PixelFormat::mask8, {1.0f, 1.0f, 1.0f, 1.0f});
	Coverage coverage;
	for(int row = 0; row < selection.rows(); ++row)
		for(int column = 0; column < selection.columns(); ++column) coverage.emplace(std::pair{column, row}, white);
	select(document, "Select All", coverage, SelectionMode::replace);
}

void deselect(Document& document) {
	if(!document.hasSelection()) return;
	select(document, "Deselect", {}, SelectionMode::replace);
}

void invertSelection(Document& document) {
	const Image& selection = document.selection();
	const bool had = document.hasSelection();
	const std::shared_ptr<const Tile> white = Tile::filled(PixelFormat::mask8, {1.0f, 1.0f, 1.0f, 1.0f});
	Coverage coverage;
	for(int row = 0; row < selection.rows(); ++row)
		for(int column = 0; column < selection.columns(); ++column) {
			const std::shared_ptr<const Tile> old = had ? selection.tile({0, column, row}) : nullptr;
			if(!old) {
				coverage.emplace(std::pair{column, row}, white);
				continue;
			}
			auto tile = std::make_shared<Tile>(PixelFormat::mask8);
			bool any = false;
			for(std::size_t at = 0; at < tilePixels; ++at)
				any = (tile->bytes()[at] = static_cast<std::uint8_t>(255 - old->bytes()[at])) != 0 || any;
			if(any) coverage.emplace(std::pair{column, row}, std::move(tile));
		}
	select(document, "Inverse", coverage, SelectionMode::replace);
}

bool selectsAnything(const Image& selection) {
	for(const TileKey& key : selection.writtenTiles())
		if(const auto tile = selection.tile(key); tile && !tile->empty()) return true;
	return false;
}

std::array<int, 4> selectionBounds(const Document& document) {
	if(!document.hasSelection()) return {0, 0, 0, 0};
	int left = document.width(), top = document.height(), right = 0, bottom = 0;
	for(const TileKey& key : document.selection().writtenTiles()) {
		const auto tile = document.selection().tile(key);
		if(!tile) continue;
		const std::optional<std::array<int, 4>> inside = coveredBounds(*tile);
		if(!inside) continue;
		const int x = key.column * tileSize, y = key.row * tileSize;
		left = std::min(left, x + (*inside)[0]), top = std::min(top, y + (*inside)[1]);
		right = std::max(right, std::min(document.width(), x + (*inside)[2]));
		bottom = std::max(bottom, std::min(document.height(), y + (*inside)[3]));
	}
	if(right <= left || bottom <= top) return {0, 0, 0, 0};
	return {left, top, right - left, bottom - top};
}

} // namespace affresco::imaging
