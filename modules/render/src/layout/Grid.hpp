#pragma once

/// @file
/// @brief detail::placeGridItems and detail::sizeTracks: grid layout as pure functions of cells and measured sizes.

#include "affresco/render/Layout.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace affresco::detail {

/// @brief The cells a child takes, counted from 0.
struct GridArea {
	std::size_t column = 0;     ///< Its first column.
	std::size_t row = 0;        ///< Its first row.
	std::size_t columnSpan = 1; ///< How many columns.
	std::size_t rowSpan = 1;    ///< How many rows.

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const GridArea&) const noexcept = default;
};

/// @brief What the children of a grid need from one track, or from the tracks they span.
struct TrackContent {
	std::size_t first = 0; ///< The first track it is in.
	std::size_t span = 1;  ///< How many tracks it spans.
	float size = 0.0f;     ///< Its measured size along the tracks.
};

/// @brief Places a grid's children in cells: first those that name their column and row, then those that name only a
/// row, then only a column, then the others row after row in the next free cells (the sparse placement of CSS).
/// @param columns The grid's columns (at least 1); children are kept inside them, spans shortened to fit.
/// @param cells What each child asks for, in order.
/// @param areas Receives each child's cells, in the same order.
/// @return The number of rows used.
std::size_t placeGridItems(std::size_t columns, std::span<const GridCell> cells, std::vector<GridArea>& areas);

/// @brief Sizes a grid's columns or rows, like CSS grid: fixed and percentage tracks first, content tracks as large as
/// their largest child (spanning children widen the content tracks they span), tracks with room to grow towards their
/// maximum, then the fraction tracks share what is left, none below its minimum.
/// @param tracks The tracks the grid declares; tracks past them are Content tracks.
/// @param count How many tracks there are in all (at least tracks.size(), at most maxGridTracks).
/// @param contents What the children need.
/// @param available The grid's width (columns) or height (rows).
/// @param gap Between tracks.
/// @param sizes Receives each track's size.
void sizeTracks(std::span<const Track> tracks, std::size_t count, std::span<const TrackContent> contents,
                float available, float gap, std::vector<float>& sizes);

/// @brief Where each track starts.
/// @param sizes The tracks' sizes.
/// @param gap Between tracks.
/// @param starts Receives each track's offset from the grid's edge.
void trackStarts(std::span<const float> sizes, float gap, std::vector<float>& starts);

} // namespace affresco::detail
