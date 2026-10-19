#pragma once

/// @file
/// @brief Stack: a container that places its children in a column or a row (flexible layout); Grid: in columns and
/// rows of tracks.

#include "affresco/render/Layout.hpp"
#include "affresco/render/Panel.hpp"

#include <optional>
#include <vector>

namespace affresco {

/// @brief The axis a Stack places its children along.
enum class Direction {
	Column, ///< One below the other.
	Row,    ///< Side by side, left to right.
};

/// @brief Places its visible children one after the other, `gap` apart, in order: in a column (the default) or a row,
/// like a CSS flex container.
///
/// Along its direction the stack manages the children's anchors; across it they keep their own (in a column, their
/// width and horizontal anchors), unless the stack or the child chooses an alignment (setAlignItems(),
/// Panel::setAlignSelf()), which then sets the anchors across too. Each child starts at its basis or its own size,
/// within its minimum and maximum; children with a grow share take the room left over, and children that shrink give
/// up an overflow. The justification spreads them: at the start, the centre or the end (in a column, at the bottom like
/// a chat history: older children overflow at the top and get clipped), or with the room between them. A wrapping stack
/// starts a new line when the next child doesn't fit; each line flexes and is spread on its own.
class Stack : public Panel {
public:
	/// @brief An empty stack with a transparent background.
	Stack();

	/// @brief Sets the space between children, along the direction and between lines.
	/// @param gap In pixels.
	void setGap(float gap);
	/// @brief The space between children, in pixels.
	[[nodiscard]] float gap() const noexcept { return gap_; }
	/// @brief Sets the space between rows: between children in a column, between lines in a wrapping row.
	/// @param gap In pixels, or std::nullopt (the default) for gap().
	void setRowGap(std::optional<float> gap);
	/// @brief Sets the space between columns: between children in a row, between lines in a wrapping column.
	/// @param gap In pixels, or std::nullopt (the default) for gap().
	void setColumnGap(std::optional<float> gap);

	/// @brief Sets where the block of children sits along the direction (setJustify() with the three plain places).
	/// @param alignment Align::Start (top or left), Align::Center or Align::End (bottom or right).
	void setAlignment(Align alignment);
	/// @brief Where the block of children sits: Align::Start for the justifications that spread the room.
	[[nodiscard]] Align alignment() const noexcept;
	/// @brief Sets how the children are spread along the direction.
	/// @param justify The justification; Justify::Start is the default.
	void setJustify(Justify justify);
	/// @brief How the children are spread along the direction.
	[[nodiscard]] Justify justify() const noexcept { return justify_; }
	/// @brief Sets where the children sit across the direction, unless a child chooses (Panel::setAlignSelf()).
	/// @param align The place, or std::nullopt (the default): children keep their own anchors across.
	void setAlignItems(std::optional<CrossAlign> align);
	/// @brief Where the children sit across, if the stack chooses.
	[[nodiscard]] std::optional<CrossAlign> alignItems() const noexcept { return alignItems_; }

	/// @brief Sets the direction.
	/// @param direction Column or row.
	void setDirection(Direction direction);
	/// @brief The direction.
	[[nodiscard]] Direction direction() const noexcept { return direction_; }

	/// @brief Lets the stack continue on a new line when the next child doesn't fit (in a column: a new column).
	/// @param wrap true to wrap.
	void setWrap(bool wrap);

protected:
	/// @brief Anchors each visible child where the flexible layout places it, and gives it the sizes it decides.
	void arrange() override;

private:
	float gap_ = 0.0f;                        ///< See gap().
	std::optional<float> rowGap_;             ///< See setRowGap().
	std::optional<float> columnGap_;          ///< See setColumnGap().
	Justify justify_ = Justify::Start;        ///< See justify().
	std::optional<CrossAlign> alignItems_;    ///< See alignItems().
	Direction direction_ = Direction::Column; ///< See direction().
	bool wrap_ = false;                       ///< See setWrap().
};

/// @brief Places its visible children in columns and rows, `gap` apart, like a CSS grid: each in the cell it asks for
/// (Panel::setGridCell()) or the next free one, row after row.
///
/// Columns are tracks (setColumns()), equal fractions by default; rows are as tall as their tallest child, unless rows
/// are given (setRows()). A child is as wide as its cell and keeps its own height at the top of it, unless the grid
/// aligns it otherwise (setJustifyItems(), setAlignItems()).
class Grid : public Panel {
public:
	/// @brief An empty grid with a transparent background and 2 equal columns.
	Grid();

	/// @brief Sets equal columns.
	/// @param columns At least 1.
	void setColumns(int columns);
	/// @brief Sets the columns' tracks.
	/// @param columns At least one track (none gives one equal column); at most detail::maxGridTracks are used.
	void setColumns(std::vector<Track> columns);
	/// @brief Sets the rows' tracks; rows past them are as tall as their tallest child.
	/// @param rows The tracks; empty (the default) for rows as tall as their content.
	void setRows(std::vector<Track> rows);
	/// @brief The columns' tracks.
	[[nodiscard]] const std::vector<Track>& columns() const noexcept { return columns_; }
	/// @brief The rows' tracks.
	[[nodiscard]] const std::vector<Track>& rows() const noexcept { return rows_; }
	/// @brief Sets the space between cells, across and down.
	/// @param gap In pixels.
	void setGap(float gap);
	/// @brief Sets the space between rows.
	/// @param gap In pixels, or std::nullopt (the default) for the gap.
	void setRowGap(std::optional<float> gap);
	/// @brief Sets the space between columns.
	/// @param gap In pixels, or std::nullopt (the default) for the gap.
	void setColumnGap(std::optional<float> gap);
	/// @brief Sets where children sit across their cells (horizontally), unless a child chooses.
	/// @param align CrossAlign::Stretch (the default) makes them as wide as their cells.
	void setJustifyItems(CrossAlign align);
	/// @brief Sets where children sit down their cells (vertically), unless a child chooses (Panel::setAlignSelf()).
	/// @param align CrossAlign::Start (the default) keeps their own height at the top.
	void setAlignItems(CrossAlign align);

protected:
	/// @brief Sizes the tracks, then sizes each visible child to its cell and anchors it there.
	void arrange() override;

private:
	std::vector<Track> columns_;                    ///< See columns().
	std::vector<Track> rows_;                       ///< See rows().
	float gap_ = 0.0f;                              ///< See setGap().
	std::optional<float> rowGap_;                   ///< See setRowGap().
	std::optional<float> columnGap_;                ///< See setColumnGap().
	CrossAlign justifyItems_ = CrossAlign::Stretch; ///< See setJustifyItems().
	CrossAlign alignItems_ = CrossAlign::Start;     ///< See setAlignItems().
};

} // namespace affresco
