#pragma once

/// @file
/// @brief RowWindow: where the rows of a long list sit and which of them a viewport shows, shared by every view that
/// builds or draws only its visible rows.

#include <cstddef>
#include <vector>

namespace affresco {

/// @brief The rows a viewport shows: [first, last).
struct RowRange {
	std::size_t first = 0; ///< The first row shown.
	std::size_t last = 0;  ///< One past the last row shown.

	/// @brief Equal when both ends are.
	bool operator==(const RowRange&) const = default;
};

/// @brief The vertical positions of a list of rows, for views that show a few of many. Every row has the same height
/// until one is measured (setHeight()); measured heights are kept in a Fenwick tree, so a row's offset, the row at an
/// offset and a height change each cost O(log n), and the fixed-height case stays O(1).
class RowWindow {
public:
	/// @brief Sets the number of rows and their height, forgetting measured heights.
	/// @param count How many rows.
	/// @param rowHeight Each row's height, in pixels; at least 1.
	void reset(std::size_t count, float rowHeight);
	/// @brief Sets one row's measured height.
	/// @param row The row; one past the end is ignored.
	/// @param height Its height, in pixels; at least 0.
	void setHeight(std::size_t row, float height);

	/// @brief How many rows.
	[[nodiscard]] std::size_t count() const noexcept { return count_; }
	/// @brief The height rows have until measured.
	[[nodiscard]] float rowHeight() const noexcept { return rowHeight_; }
	/// @brief Whether some row has its own height.
	[[nodiscard]] bool measured() const noexcept { return !tree_.empty(); }
	/// @brief A row's height.
	/// @param row The row.
	[[nodiscard]] float heightOf(std::size_t row) const noexcept;
	/// @brief The top of a row, from the top of the first.
	/// @param row The row, or count() for the bottom of the last.
	[[nodiscard]] float offsetOf(std::size_t row) const noexcept;
	/// @brief The height of every row together.
	[[nodiscard]] float total() const noexcept { return offsetOf(count_); }
	/// @brief The row at a distance from the top of the first.
	/// @param offset In pixels.
	/// @return The row, clamped to the rows there are; count() when there are none.
	[[nodiscard]] std::size_t rowAt(float offset) const noexcept;
	/// @brief The rows a viewport shows, plus a margin before and after.
	/// @param scroll The viewport's top, from the top of the first row.
	/// @param viewport The viewport's height.
	/// @param margin How many more rows to include on each side.
	[[nodiscard]] RowRange visible(float scroll, float viewport, std::size_t margin) const noexcept;

private:
	std::size_t count_ = 0;     ///< See count().
	float rowHeight_ = 1.0f;    ///< See rowHeight().
	std::vector<float> heights_; ///< Each row's height once one is measured; empty before.
	std::vector<float> tree_;    ///< A Fenwick tree over heights_, 1-based; empty before a row is measured.
};

} // namespace affresco
