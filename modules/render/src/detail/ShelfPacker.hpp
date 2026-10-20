#pragma once

/// @file
/// @brief detail::ShelfPacker: boxes packed into a square page in shelves, shared by the glyph atlas and the image
/// atlas.

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

namespace affresco::detail {

/// @brief Packs boxes into a square page in shelves: rows of boxes, each as tall as its tallest. Boxes are only ever
/// added, so a box keeps its place. Not thread-safe.
class ShelfPacker {
public:
	/// @brief An empty page.
	/// @param size Its width and height.
	/// @param top The first row boxes may use (rows above it are reserved).
	explicit ShelfPacker(int size, int top = 0) noexcept : size_(size), freeTop_(top) {}

	/// @brief Finds room for a box, on a shelf or on a new one.
	/// @param width Its width.
	/// @param height Its height.
	/// @param shelfHeight The height a new shelf gets, if the box is not taller.
	/// @return Its top-left corner, or nothing if the page is full.
	[[nodiscard]] std::optional<std::array<int, 2>> place(int width, int height, int shelfHeight) {
		if(width > size_) return std::nullopt;

		// The shortest shelf the box fits on wastes the fewest rows
		Shelf* best = nullptr;
		for(Shelf& shelf : shelves_)
			if(shelf.height >= height && shelf.right + width <= size_ && (!best || shelf.height < best->height))
				best = &shelf;

		if(!best) {
			const int rows = std::max(height, shelfHeight);
			if(freeTop_ + rows > size_) return std::nullopt;
			best = &shelves_.emplace_back(Shelf{freeTop_, rows, 0});
			freeTop_ += rows;
		}

		const std::array<int, 2> at{best->right, best->top};
		best->right += width;
		return at;
	}

	/// @brief The page's width and height.
	[[nodiscard]] int size() const noexcept { return size_; }

private:
	/// @brief A row of boxes.
	struct Shelf {
		int top;    ///< Its first row.
		int height; ///< Its rows.
		int right;  ///< The first free column.
	};

	int size_;                   ///< See size().
	std::vector<Shelf> shelves_; ///< Rows that boxes are added to.
	int freeTop_;                ///< The first row below every shelf.
};

} // namespace affresco::detail
