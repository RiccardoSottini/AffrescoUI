#pragma once

/// @file
/// @brief Limits: every bound of the imaging module in one place, the rows SECURITY.md lists.

#include <cstddef>

namespace affresco::imaging {

/// @brief What a document, its files and its work may be. Anything beyond is refused with an error naming the bound.
struct Limits {
	int side = 32768;                                    ///< The widest and tallest image, in pixels.
	std::size_t pixels = std::size_t{1} << 30;           ///< The most pixels of one image (a gigapixel).
	std::size_t layers = 256;                            ///< The most layers and groups of a document.
	int groupDepth = 2;                                  ///< The deepest nesting of groups (GPU passes per level).
	std::size_t cacheBytes = std::size_t{512} << 20;     ///< CPU memory for loaded tiles and mips, per image.
	std::size_t gpuBytes = std::size_t{256} << 20;       ///< GPU memory for resident tiles, per view.
	std::size_t undoBytes = std::size_t{512} << 20;      ///< Memory of the undo history: the oldest steps go first.
	std::size_t undoSteps = 200;                         ///< Steps of the undo history.
	int filterRadius = 250;                              ///< The largest radius of a filter or a feather, in pixels.
	int brushSize = 5000;                                ///< The largest brush, in pixels.
	std::size_t imageFileBytes = std::size_t{256} << 20; ///< The largest PNG or JPEG file read.
	int imageFileSide = 30000;                           ///< The widest and tallest PNG or JPEG decoded.
	std::size_t projectFileBytes = std::size_t{4} << 30; ///< The largest project file read.
	std::size_t manifestBytes = std::size_t{16} << 20;   ///< The largest manifest of a project file.
	std::size_t profileBytes = std::size_t{4} << 20;     ///< The largest ICC profile parsed.
	std::size_t floodPixels = std::size_t{64} << 20;     ///< The most pixels a magic wand or fill visits.
	std::size_t polygonPoints = 100000;                  ///< The most points of a lasso or polygon.
};

} // namespace affresco::imaging
