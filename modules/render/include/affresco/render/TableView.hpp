#pragma once

/// @file
/// @brief TableView: a virtualised table: a ListView with columns, a header, sorting and resizable columns.

#include "affresco/render/ListView.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace affresco {

/// @brief A column of a table: which cell of each item it shows, under which title.
struct TableColumn {
	std::string id;       ///< Names it in the sort and the events.
	std::string title;    ///< Shown in the header, UTF-8.
	float width = 120.0f; ///< In pixels; at least TableView::minimumColumnWidth().

	/// @brief Whether two columns are the same.
	bool operator==(const TableColumn&) const = default;
};

/// @brief How a table's rows are sorted.
struct TableSort {
	std::string column;      ///< The column's id; empty for the model's order.
	bool descending = false; ///< Whether the largest comes first.

	/// @brief Whether two orders are the same.
	bool operator==(const TableSort&) const = default;
};

/// @brief A column the user made wider or narrower.
struct ColumnResize {
	std::string column; ///< Its id.
	float width = 0.0f; ///< Its width now, in pixels.
};

/// @brief A table of items of which only the rows in view are drawn. Column i shows each item's cell i (ListItem),
/// truncated with an ellipsis when it doesn't fit. A click on a header sorts by its column, ascending then descending:
/// numbers by value before text, text by its bytes, equal cells in the model's order. Dragging a header's right edge
/// resizes its column. The columns and the order are given (setColumns(), setSort()) and the table reports what the
/// user does (onSortChanged, onColumnResized); the user's changes stay until different ones are given.
class TableView : public ListView {
public:
	/// @brief Sets the columns. Giving the same ones again keeps the widths the user dragged to.
	/// @param columns The columns, left to right.
	void setColumns(std::vector<TableColumn> columns);
	/// @brief The columns, with the widths shown now.
	[[nodiscard]] const std::vector<TableColumn>& columns() const noexcept { return columns_; }
	/// @brief Sets the order of the rows. Giving the same one again keeps the order the user clicked.
	/// @param sort The order.
	void setSort(TableSort sort);
	/// @brief The order shown now.
	[[nodiscard]] const TableSort& sort() const noexcept { return sort_; }
	/// @brief The narrowest a column can be dragged to, in pixels.
	[[nodiscard]] static constexpr float minimumColumnWidth() noexcept { return 24.0f; }
	/// @brief The height of the header, in pixels.
	[[nodiscard]] static constexpr float headerSize() noexcept { return 28.0f; }
	/// @brief The row that shows an item.
	/// @param item The item's index in the model.
	/// @return The row, or rowCount().
	[[nodiscard]] std::size_t rowOf(std::size_t item) const noexcept override;

	Signal<const TableSort&> onSortChanged;      ///< The user clicked a header.
	Signal<const ColumnResize&> onColumnResized; ///< The user dragged a column's edge and let go.

protected:
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	/// @brief Sorts the items by the sort column, keeping the model's order among equal cells.
	/// @param rows Empty; gets the items' indices.
	void buildRows(std::vector<std::size_t>& rows) override;
	void paintRow(Painter& painter, const Font& font, std::size_t row, const Rect& area, Color color) const override;
	void paintOverlay(Painter& painter, const Font& font) const override;
	[[nodiscard]] float headerHeight() const noexcept override { return headerSize(); }
	[[nodiscard]] std::size_t shownCells() const noexcept override { return std::max<std::size_t>(columns_.size(), 1); }

private:
	/// @brief The column whose right edge is near a point in the header.
	/// @param x In window units.
	/// @return Its index, or the column count for none.
	[[nodiscard]] std::size_t edgeAt(float x) const noexcept;
	/// @brief The column under a point.
	/// @param x In window units.
	/// @return Its index, or the column count for none.
	[[nodiscard]] std::size_t columnAt(float x) const noexcept;
	/// @brief Where a column starts, from the table's left.
	/// @param column Its index; the column count gives where the last one ends.
	[[nodiscard]] float columnLeft(std::size_t column) const noexcept;
	/// @brief Sorts by a column, as a click on its header does, and fires onSortChanged.
	/// @param column Its index.
	void sortBy(std::size_t column);
	/// @brief The index of the sort column.
	/// @return It, or the column count for the model's order.
	[[nodiscard]] std::size_t sortColumn() const noexcept;
	/// @brief Draws the sort mark: a triangle pointing up for ascending, down for descending.
	/// @param painter Where.
	/// @param left Its left edge.
	/// @param middle Its vertical centre.
	void paintSortMark(Painter& painter, float left, float middle) const;
	/// @brief Draws text in a cell, cut short with an ellipsis when it is wider.
	/// @param painter Where.
	/// @param font The font.
	/// @param text The text.
	/// @param left Where the cell starts.
	/// @param width The cell's width.
	/// @param baseline The baseline.
	/// @param color The colour.
	static void paintCell(Painter& painter, const Font& font, std::u32string_view text, float left, float width,
	                      float baseline, Color color);

	std::vector<TableColumn> columns_;      ///< See columns().
	std::vector<TableColumn> givenColumns_; ///< What setColumns() last got.
	std::vector<std::u32string> titles_;    ///< The columns' titles, decoded.
	TableSort sort_;                        ///< See sort().
	std::optional<TableSort> givenSort_;    ///< What setSort() last got.
	std::vector<std::size_t> rowOfItem_;    ///< Each item's row, or the row count when it isn't shown.
	std::vector<double> numbers_;           ///< The sort column's cells as numbers (NaN for text), while sorting.
	std::size_t resizing_ = 0;              ///< The column being resized, or the column count for none.
	float grab_ = 0.0f;                     ///< How far right of the edge the drag started.
};

} // namespace affresco
