#pragma once

/// @file
/// @brief The values of flexible and grid layout: how a row or a column spreads its children (Justify), where a child
/// sits across it (CrossAlign), the size of a grid's columns and rows (Track) and the cell a child takes (GridCell).

namespace affresco {

/// @brief How a row or a column spreads its children along its direction, like CSS justify-content.
enum class Justify {
	Start,        ///< Packed at the start (left or top): the default.
	Center,       ///< Packed in the middle.
	End,          ///< Packed at the end (right or bottom); children that don't fit overflow at the start.
	SpaceBetween, ///< The first at the start, the last at the end, the room left over shared between them.
	SpaceAround,  ///< The room left over shared around each child: half of a share at both ends.
	SpaceEvenly,  ///< The room left over shared equally between the children and both ends.
};

/// @brief Where a child sits across a row or a column (in a row: vertically), like CSS align-items and align-self.
enum class CrossAlign {
	Start,   ///< At the top of a row, the left of a column.
	Center,  ///< In the middle.
	End,     ///< At the bottom of a row, the right of a column.
	Stretch, ///< As tall as the row (as wide as the column), within its minimum and maximum size.
};

/// @brief The unit of one bound of a grid track.
enum class TrackUnit {
	Pixels,   ///< Window units.
	Percent,  ///< Of the grid's width (columns) or height (rows).
	Fraction, ///< A share of the room the other tracks leave (CSS `fr`); as a minimum it counts as 0.
	Content,  ///< As large as the largest child in the track (CSS `auto`).
};

/// @brief One bound of a grid track: a number and its unit.
struct TrackSize {
	float value = 0.0f;                  ///< Pixels, percent or shares; unused for Content.
	TrackUnit unit = TrackUnit::Content; ///< The unit.

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const TrackSize&) const noexcept = default;
};

/// @brief The size of a grid column or row: between a minimum and a maximum, like CSS `minmax(minimum, maximum)`.
///
/// A plain size is a track whose two bounds are the same; `1fr` is `minmax(0, 1fr)`, so equal fractions give equal
/// tracks whatever their content (write `minmax(auto, 1fr)` to keep a track as wide as its content).
struct Track {
	TrackSize minimum; ///< The smallest it may be.
	TrackSize maximum; ///< The largest it grows to; a Fraction maximum makes the track flexible.

	/// @brief A track of a fixed number of pixels.
	/// @param pixels Window units.
	/// @return The track.
	[[nodiscard]] static constexpr Track pixels(float pixels) noexcept {
		return {{pixels, TrackUnit::Pixels}, {pixels, TrackUnit::Pixels}};
	}
	/// @brief A track of a share of the grid's size.
	/// @param percent Of the grid's width or height.
	/// @return The track.
	[[nodiscard]] static constexpr Track percent(float percent) noexcept {
		return {{percent, TrackUnit::Percent}, {percent, TrackUnit::Percent}};
	}
	/// @brief A flexible track: a share of the room left over, from 0.
	/// @param shares How many shares (CSS `fr`).
	/// @return The track.
	[[nodiscard]] static constexpr Track fraction(float shares) noexcept {
		return {{0.0f, TrackUnit::Pixels}, {shares, TrackUnit::Fraction}};
	}
	/// @brief A track as large as its largest child (CSS `auto`).
	/// @return The track.
	[[nodiscard]] static constexpr Track content() noexcept { return {}; }
	/// @brief A track between two bounds (CSS `minmax`).
	/// @param minimum The smallest it may be (a Fraction counts as 0).
	/// @param maximum The largest it grows to.
	/// @return The track.
	[[nodiscard]] static constexpr Track between(TrackSize minimum, TrackSize maximum) noexcept {
		return {minimum, maximum};
	}

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const Track&) const noexcept = default;
};

/// @brief The cell a child takes in a grid: its first column and row (from 1), and how many it spans.
struct GridCell {
	int column = 0;     ///< Its first column, from 1; 0 lets the grid place it in the next free cell.
	int row = 0;        ///< Its first row, from 1; 0 lets the grid place it.
	int columnSpan = 1; ///< How many columns it takes, at least 1.
	int rowSpan = 1;    ///< How many rows it takes, at least 1.

	/// @brief Equal when every member is equal.
	constexpr bool operator==(const GridCell&) const noexcept = default;
};

} // namespace affresco
