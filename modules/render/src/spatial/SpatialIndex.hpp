#pragma once

/// @file
/// @brief SpatialIndex: a uniform grid over rectangles, so a canvas finds what a rectangle (the visible one, a
/// marquee) touches without looking at every item.

#include <affresco/render/Geometry.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace affresco {

/// @brief A uniform grid over world rectangles, stored flat (each cell's items in one array, cells by start).
///
/// Building is O(n) and reuses the buffers of the last build, so rebuilding at the same size allocates nothing; the
/// cell size follows the items' average size and the cells are bounded (maximumCells, and 4 a item). An item that
/// spans more than largeSpan cells is kept aside and tested on every query. Queries allocate nothing once their output
/// has grown to the largest answer. Main thread only (queries reuse a mark array).
class SpatialIndex {
public:
	static constexpr std::size_t maximumCells = std::size_t{1} << 20; ///< Cells at most, whatever the items.
	static constexpr std::size_t maximumItems = std::size_t{1} << 24; ///< Items indexed at most; the rest are left out.
	static constexpr std::size_t largeSpan = 16; ///< Cells an item may span before it is kept aside.

	/// @brief Indexes rectangles in place of the last ones. Rectangles that aren't finite, or are inverted, are left
	/// out (no query finds them).
	/// @param bounds The rectangles; an item's number is its place here.
	void build(std::span<const Rect> bounds);

	/// @brief The items whose rectangle meets an area, edges included.
	/// @param area The area, in the rectangles' units.
	/// @param found Cleared, then filled with the items' numbers in ascending order (their drawing order).
	void query(Rect area, std::vector<std::uint32_t>& found) const;

	/// @brief The number of items the last build was given.
	[[nodiscard]] std::size_t size() const noexcept { return bounds_.size(); }
	/// @brief The grid's cell size, in the rectangles' units (0 before the first build or with no items).
	[[nodiscard]] float cellSize() const noexcept { return cellSize_; }

private:
	/// @brief A block of cells, last ones included.
	struct CellRange {
		std::uint32_t firstColumn = 0; ///< The left column.
		std::uint32_t firstRow = 0;    ///< The top row.
		std::uint32_t lastColumn = 0;  ///< The right column.
		std::uint32_t lastRow = 0;     ///< The bottom row.

		/// @brief The number of cells.
		[[nodiscard]] std::size_t count() const noexcept {
			return std::size_t{lastColumn - firstColumn + 1} * std::size_t{lastRow - firstRow + 1};
		}
	};

	/// @brief Chooses the grid from the items' extent and average size.
	void chooseGrid();
	/// @brief The cells a rectangle covers, clipped to the grid.
	/// @param area The rectangle.
	/// @return The range, or std::nullopt if the rectangle misses the grid.
	[[nodiscard]] std::optional<CellRange> cellRange(const Rect& area) const noexcept;
	/// @brief The cells an item covers.
	/// @param item The item's number.
	/// @return The range, or std::nullopt if it can't be indexed or misses the grid.
	[[nodiscard]] std::optional<CellRange> itemCells(std::uint32_t item) const noexcept;
	/// @brief Counts the items of each cell into cellStarts_ (one place on) and keeps large items aside, in large_.
	void countCells();
	/// @brief Fills each cell's items in cellItems_, once cellStarts_ holds where each cell starts.
	void fillCells();

	std::vector<Rect> bounds_{};                 ///< The items' rectangles, as built.
	std::vector<std::uint32_t> cellStarts_{};    ///< Where each cell's items start in cellItems_; one more than cells.
	std::vector<std::uint32_t> cellItems_{};     ///< Every cell's items, cell after cell.
	std::vector<std::uint32_t> cursors_{};       ///< Scratch for build().
	std::vector<std::uint32_t> large_{};         ///< Items spanning more than largeSpan cells.
	mutable std::vector<std::uint32_t> marks_{}; ///< The query that last found each item, so it is reported once.
	mutable std::uint32_t mark_ = 0;             ///< The current query's mark.
	Rect extent_{};                              ///< The rectangle all the indexed items fit in.
	float cellSize_ = 0.0f;                      ///< See cellSize().
	std::uint32_t columns_ = 0;                  ///< Cells across.
	std::uint32_t rows_ = 0;                     ///< Cells down.
};

} // namespace affresco
