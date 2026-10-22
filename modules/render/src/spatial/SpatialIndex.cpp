/// @file
/// @brief SpatialIndex: building the grid and querying it.

#include "spatial/SpatialIndex.hpp"

#include <algorithm>
#include <cmath>

namespace affresco {
namespace {

/// @brief Whether a rectangle can be indexed: finite, and not inverted.
/// @param rect The rectangle.
/// @return true if it can.
bool indexable(const Rect& rect) noexcept {
	return std::isfinite(rect.left) && std::isfinite(rect.top) && std::isfinite(rect.right) &&
	       std::isfinite(rect.bottom) && rect.right >= rect.left && rect.bottom >= rect.top;
}

/// @brief Whether two rectangles meet, edges included.
/// @param a One.
/// @param b The other.
/// @return true if they meet.
bool meet(const Rect& a, const Rect& b) noexcept {
	return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
}

/// @brief A cell number along one axis, clipped to the grid.
/// @param position The coordinate, relative to the grid's start.
/// @param cellSize The cell size.
/// @param cells The cells along the axis (at least 1).
/// @return The cell.
std::uint32_t cellOf(float position, float cellSize, std::uint32_t cells) noexcept {
	const float cell = std::floor(position / cellSize);
	if(!(cell > 0.0f)) return 0;
	return std::min(static_cast<std::uint32_t>(std::min(cell, 4.0e9f)), cells - 1);
}

/// @brief Calls @p visit with the number of each cell of a block, row after row.
/// @param range The block (a SpatialIndex::CellRange).
/// @param columns The grid's columns.
/// @param visit What to call.
template <typename Range, typename Visit>
void forEachCell(const Range& range, std::uint32_t columns, const Visit& visit) {
	for(std::uint32_t row = range.firstRow; row <= range.lastRow; ++row)
		for(std::uint32_t column = range.firstColumn; column <= range.lastColumn; ++column)
			visit(std::size_t{row} * columns + column);
}

} // namespace

void SpatialIndex::build(std::span<const Rect> bounds) {
	bounds = bounds.first(std::min(bounds.size(), maximumItems));
	bounds_.assign(bounds.begin(), bounds.end());
	large_.clear();
	chooseGrid();
	const std::size_t cells = std::size_t{columns_} * rows_;
	cellStarts_.assign(cells + 1, 0);
	cellItems_.clear();
	if(cells == 0) return;

	// Count each cell's items, then lay the cells out one after another, then fill them
	countCells();
	for(std::size_t cell = 1; cell <= cells; ++cell) cellStarts_[cell] += cellStarts_[cell - 1];
	fillCells();
	marks_.assign(bounds_.size(), 0);
	mark_ = 0;
}

std::optional<SpatialIndex::CellRange> SpatialIndex::itemCells(std::uint32_t item) const noexcept {
	return indexable(bounds_[item]) ? cellRange(bounds_[item]) : std::nullopt;
}

void SpatialIndex::countCells() {
	const auto count = static_cast<std::uint32_t>(bounds_.size());
	for(std::uint32_t item = 0; item < count; ++item) {
		const auto range = itemCells(item);
		if(!range) continue;
		if(range->count() > largeSpan) large_.push_back(item);
		else forEachCell(*range, columns_, [starts = cellStarts_.data()](std::size_t cell) { ++starts[cell + 1]; });
	}
}

void SpatialIndex::fillCells() {
	cellItems_.resize(cellStarts_.back());
	cursors_.assign(cellStarts_.begin(), cellStarts_.end() - 1);
	// Held in locals, so the stores need not reload the vectors (the compiler can't tell they don't alias them)
	std::uint32_t* const items = cellItems_.data();
	std::uint32_t* const cursors = cursors_.data();
	const auto count = static_cast<std::uint32_t>(bounds_.size());
	for(std::uint32_t item = 0; item < count; ++item) {
		const auto range = itemCells(item);
		if(!range || range->count() > largeSpan) continue;
		forEachCell(*range, columns_, [&](std::size_t cell) { items[cursors[cell]++] = item; });
	}
}

void SpatialIndex::chooseGrid() {
	extent_ = {};
	cellSize_ = 0.0f;
	columns_ = rows_ = 0;
	bool any = false;
	double sizes = 0.0;
	std::size_t counted = 0;
	for(const Rect& rect : bounds_) {
		if(!indexable(rect)) continue;
		extent_ = any ? Rect{std::min(extent_.left, rect.left), std::min(extent_.top, rect.top),
		                     std::max(extent_.right, rect.right), std::max(extent_.bottom, rect.bottom)}
		              : rect;
		any = true;
		sizes += std::max(rect.width(), rect.height());
		++counted;
	}
	if(!any) return;

	// Cells about the size of an average item, but no more of them than the budget allows
	const double width = std::max(double{extent_.width()}, 1e-3), height = std::max(double{extent_.height()}, 1e-3);
	const double budget = static_cast<double>(std::min(maximumCells, std::max<std::size_t>(4 * counted, 1)));
	double cell = std::max(sizes / static_cast<double>(counted), 1e-3);
	const double wanted = std::ceil(width / cell) * std::ceil(height / cell);
	if(wanted > budget) cell *= std::sqrt(wanted / budget) * 1.01;
	columns_ = static_cast<std::uint32_t>(std::clamp(std::ceil(width / cell), 1.0, budget));
	rows_ = static_cast<std::uint32_t>(std::clamp(std::ceil(height / cell), 1.0, std::max(budget / columns_, 1.0)));
	cellSize_ = static_cast<float>(cell);
}

std::optional<SpatialIndex::CellRange> SpatialIndex::cellRange(const Rect& area) const noexcept {
	if(columns_ == 0 || !meet(area, extent_)) return std::nullopt;
	return CellRange{
	    cellOf(area.left - extent_.left, cellSize_, columns_), cellOf(area.top - extent_.top, cellSize_, rows_),
	    cellOf(area.right - extent_.left, cellSize_, columns_), cellOf(area.bottom - extent_.top, cellSize_, rows_)};
}

void SpatialIndex::query(Rect area, std::vector<std::uint32_t>& found) const {
	found.clear();
	if(!indexable(area)) return;
	if(++mark_ == 0) { // wrapped: forget every old mark
		std::ranges::fill(marks_, 0);
		mark_ = 1;
	}
	if(const auto range = cellRange(area)) {
		for(std::uint32_t row = range->firstRow; row <= range->lastRow; ++row) {
			const std::size_t first = std::size_t{row} * columns_;
			const std::uint32_t from = cellStarts_[first + range->firstColumn];
			const std::uint32_t to = cellStarts_[first + range->lastColumn + 1];
			for(std::uint32_t at = from; at < to; ++at) {
				const std::uint32_t item = cellItems_[at];
				if(marks_[item] == mark_ || !meet(bounds_[item], area)) continue;
				marks_[item] = mark_;
				found.push_back(item);
			}
		}
	}
	for(const std::uint32_t item : large_)
		if(meet(bounds_[item], area)) found.push_back(item);
	std::ranges::sort(found);
}

} // namespace affresco
