#pragma once

/// @file
/// @brief The CPU compositor: a document's layers flattened, tile by tile at any mip level. It is the reference the
/// GPU compositor is tested against, and what export, thumbnails and the eyedropper's "all layers" read.

#include "affresco/imaging/Document.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace affresco::imaging {

/// @brief Flattens a document's layers; keeps its scratch buffers between calls.
///
/// Groups are isolated (their layers composite together, then onto what is below with the group's mode, opacity and
/// mask). A clipped layer's coverage is multiplied by its base's: the alpha of the nearest unclipped layer below it,
/// after that layer's mask. An adjustment layer blends the adjusted colour below with its mode, opacity and mask.
class Compositor {
public:
	/// @brief A compositor of a document, which must outlive it.
	/// @param document The document.
	explicit Compositor(const Document& document);

	/// @brief Composites one tile.
	/// @param key The tile (level 0 full size; a level n tile covers 2^n times as much).
	/// @param out tilePixels pixels, straight alpha, top row first.
	void composite(const TileKey& key, std::span<Pixel> out);

private:
	/// @brief Composites a list of layers onto a backdrop.
	/// @param list The layers, the bottom first.
	/// @param key The tile.
	/// @param backdrop The backdrop, changed.
	/// @param depth The list's depth (0 the top level).
	void compositeList(const std::vector<std::uint64_t>& list, const TileKey& key, std::span<Pixel> backdrop,
	                   std::size_t depth);
	/// @brief Composites one layer onto a backdrop.
	/// @param layer The layer.
	/// @param key The tile.
	/// @param backdrop The backdrop, changed.
	/// @param depth The layer's depth.
	void compositeLayer(const Layer& layer, const TileKey& key, std::span<Pixel> backdrop, std::size_t depth);

	const Document& document_;                ///< The document.
	std::vector<std::vector<Pixel>> groups_;  ///< A group's own pixels, per depth.
	std::vector<std::vector<float>> bases_;   ///< The clipping base's alpha, per depth.
};

/// @brief A region of a document flattened into rgba8, straight alpha.
/// @param document The document.
/// @param level The mip level (the region in that level's pixels).
/// @param x The region's left.
/// @param y The region's top.
/// @param width Its width.
/// @param height Its height.
/// @return width × height × 4 bytes, top row first.
[[nodiscard]] std::vector<std::uint8_t> flatten(const Document& document, int level, int x, int y, int width,
                                                int height);

/// @brief A thumbnail: an image (a layer's, a mask, or the whole document for 0) fitted into a square, rgba8.
/// @param document The document.
/// @param image The image's id, or 0 for the flattened document.
/// @param side The square's side, 8 to 256.
/// @return side × side × 4 bytes, the image centred on transparency.
[[nodiscard]] std::vector<std::uint8_t> thumbnail(const Document& document, std::uint64_t image, int side);

} // namespace affresco::imaging
