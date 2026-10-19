/// @file
/// @brief detail::placeGridItems, detail::sizeTracks and detail::trackStarts.

#include "layout/Grid.hpp"

#include <algorithm>
#include <limits>

namespace affresco::detail {

namespace {

/// @brief Which cells are taken, row after row; it grows a row at a time.
class Occupancy {
public:
	/// @brief An empty grid of some columns.
	/// @param columns At least 1.
	explicit Occupancy(std::size_t columns) : columns_(columns) {}

	/// @brief Whether an area is free (rows past the end are).
	/// @param area The area.
	/// @return true if no cell of it is taken.
	[[nodiscard]] bool isFree(const GridArea& area) const {
		for(std::size_t row = area.row; row < area.row + area.rowSpan && row < rows(); ++row)
			for(std::size_t column = area.column; column < area.column + area.columnSpan; ++column)
				if(taken_[row * columns_ + column]) return false;
		return true;
	}

	/// @brief Takes an area's cells.
	/// @param area The area.
	void take(const GridArea& area) {
		if(rows() < area.row + area.rowSpan) taken_.resize((area.row + area.rowSpan) * columns_, 0);
		for(std::size_t row = area.row; row < area.row + area.rowSpan; ++row)
			for(std::size_t column = area.column; column < area.column + area.columnSpan; ++column)
				taken_[row * columns_ + column] = 1;
	}

	/// @brief The rows that have a taken cell, or come before one.
	[[nodiscard]] std::size_t rows() const noexcept { return taken_.size() / columns_; }

private:
	std::size_t columns_;              ///< See Occupancy().
	std::vector<unsigned char> taken_; ///< Row after row; 1 where taken.
};

/// @brief A child's cells before placement: its spans within the grid, and where it asked to be (or not).
/// @param cell What it asked for.
/// @param columns The grid's columns.
/// @return Its area, with column and row 0 where it didn't ask.
GridArea requested(const GridCell& cell, std::size_t columns) {
	const auto clampedIndex = [](int value, std::size_t limit) {
		return std::min(static_cast<std::size_t>(std::max(value, 1) - 1), limit - 1);
	};
	GridArea area;
	area.columnSpan = std::clamp<std::size_t>(static_cast<std::size_t>(std::max(cell.columnSpan, 1)), 1, columns);
	area.rowSpan = std::min(static_cast<std::size_t>(std::max(cell.rowSpan, 1)), maxGridTracks);
	if(cell.column > 0) area.column = std::min(clampedIndex(cell.column, columns), columns - area.columnSpan);
	if(cell.row > 0) area.row = std::min(clampedIndex(cell.row, maxGridTracks), maxGridTracks - area.rowSpan);
	return area;
}

/// @brief Finds the first free position for an area, scanning rows from @p row and columns from @p column.
/// @param occupancy The taken cells.
/// @param area The area; its column and row are set.
/// @param columns The grid's columns.
/// @param fixedColumn Whether its column stays as it is.
/// @param fixedRow Whether its row stays as it is.
/// @param from Where to start scanning, row then column.
void findFree(const Occupancy& occupancy, GridArea& area, std::size_t columns, bool fixedColumn, bool fixedRow,
              GridArea from) {
	for(std::size_t row = fixedRow ? area.row : from.row; row < maxGridTracks; ++row) {
		const std::size_t firstColumn = fixedColumn ? area.column : row == from.row ? from.column : 0;
		const std::size_t lastColumn = fixedColumn ? area.column : columns - area.columnSpan;
		for(std::size_t column = firstColumn; column <= lastColumn; ++column) {
			GridArea candidate = area;
			candidate.row = std::min(row, maxGridTracks - area.rowSpan);
			candidate.column = column;
			if(occupancy.isFree(candidate)) {
				area = candidate;
				return;
			}
		}
		if(fixedRow) break; // no room in its row: it overlaps where it asked to be
	}
}

/// @brief The smallest a track may be, before its content is known.
/// @param bound Its minimum.
/// @param available The grid's size.
/// @return In pixels.
float fixedSize(const TrackSize& bound, float available) noexcept {
	switch(bound.unit) {
	case TrackUnit::Pixels:
		return std::max(bound.value, 0.0f);
	case TrackUnit::Percent:
		return std::max(available / 100.0f * bound.value, 0.0f);
	case TrackUnit::Fraction:
	case TrackUnit::Content:
		return 0.0f;
	}
	return 0.0f;
}

/// @brief One track while it is sized.
struct Sizing {
	float base = 0.0f;  ///< Its size so far.
	float limit = 0.0f; ///< What it may grow to before fractions share the rest.
	float flex = 0.0f;  ///< Its shares, if its maximum is a Fraction.
};

/// @brief Widens the content tracks that children spanning several tracks need more room from.
/// @param tracks The tracks.
/// @param sizing Their sizes so far.
/// @param contents What the children need.
/// @param gap Between tracks.
void fitSpanningContent(std::span<const Track> tracks, std::span<Sizing> sizing, std::span<const TrackContent> contents,
                        float gap) {
	const auto isContent = [&](std::size_t i) {
		return i >= tracks.size() || tracks[i].minimum.unit == TrackUnit::Content;
	};
	for(const TrackContent& content : contents) {
		if(content.span < 2) continue;
		float spanned = gap * static_cast<float>(content.span - 1);
		std::size_t growing = 0;
		for(std::size_t i = content.first; i < content.first + content.span; ++i) {
			spanned += sizing[i].base;
			if(isContent(i)) ++growing;
		}
		if(growing == 0 || content.size <= spanned) continue;
		const float share = (content.size - spanned) / static_cast<float>(growing);
		for(std::size_t i = content.first; i < content.first + content.span; ++i)
			if(isContent(i)) sizing[i].base += share;
	}
}

/// @brief Grows the tracks below their limit equally into the room left over (the fraction tracks wait).
/// @param sizing The tracks.
/// @param free The room left over.
void growToLimits(std::span<Sizing> sizing, float free) {
	while(free > 0.0f) {
		std::size_t growing = 0;
		for(const Sizing& track : sizing)
			if(track.flex <= 0.0f && track.limit > track.base) ++growing;
		if(growing == 0) return;
		const float share = free / static_cast<float>(growing);
		for(Sizing& track : sizing) {
			if(track.flex > 0.0f || track.limit <= track.base) continue;
			const float added = std::min(share, track.limit - track.base);
			track.base += added;
			free -= added;
		}
		if(share <= std::numeric_limits<float>::epsilon()) return;
	}
}

/// @brief Shares the room left over among the fraction tracks; a track whose minimum is more than its share keeps its
/// minimum and leaves the others the rest.
/// @param sizing The tracks.
/// @param room The grid's size less the gaps.
void shareFractions(std::span<Sizing> sizing, float room) {
	std::vector<bool> inflexible(sizing.size(), false);
	for(;;) {
		float leftover = room, shares = 0.0f;
		for(std::size_t i = 0; i < sizing.size(); ++i) {
			if(sizing[i].flex > 0.0f && !inflexible[i]) shares += sizing[i].flex;
			else leftover -= sizing[i].base;
		}
		if(shares <= 0.0f || leftover <= 0.0f) return;
		const float fraction = leftover / shares;
		bool tooSmall = false;
		for(std::size_t i = 0; i < sizing.size(); ++i) {
			if(sizing[i].flex <= 0.0f || inflexible[i] || sizing[i].base <= sizing[i].flex * fraction) continue;
			inflexible[i] = true;
			tooSmall = true;
		}
		if(tooSmall) continue;
		for(std::size_t i = 0; i < sizing.size(); ++i)
			if(sizing[i].flex > 0.0f && !inflexible[i])
				sizing[i].base = std::max(sizing[i].base, sizing[i].flex * fraction);
		return;
	}
}

} // namespace

std::size_t placeGridItems(std::size_t columns, std::span<const GridCell> cells, std::vector<GridArea>& areas) {
	columns = std::clamp<std::size_t>(columns, 1, maxGridTracks);
	areas.assign(cells.size(), GridArea{});
	Occupancy occupancy(columns);

	// Explicit both, then a row only, then a column only, then neither: each pass in the children's order
	const auto pass = [&](bool wantsColumn, bool wantsRow, bool autoCursor) {
		GridArea cursor;
		for(std::size_t i = 0; i < cells.size(); ++i) {
			if((cells[i].column > 0) != wantsColumn || (cells[i].row > 0) != wantsRow) continue;
			GridArea area = requested(cells[i], columns);
			if(!wantsColumn || !wantsRow)
				findFree(occupancy, area, columns, wantsColumn, wantsRow, autoCursor ? cursor : GridArea{});
			occupancy.take(area);
			areas[i] = area;
			if(autoCursor) cursor = {area.column + area.columnSpan, area.row, 1, 1};
		}
	};
	pass(true, true, false);
	pass(false, true, false);
	pass(true, false, false);
	pass(false, false, true);
	return occupancy.rows();
}

void sizeTracks(std::span<const Track> tracks, std::size_t count, std::span<const TrackContent> contents,
                float available, float gap, std::vector<float>& sizes) {
	count = std::min(std::max(count, tracks.size()), maxGridTracks);
	std::vector<Sizing> sizing(count);
	for(std::size_t i = 0; i < count; ++i) {
		const Track track = i < tracks.size() ? tracks[i] : Track::content();
		sizing[i].base = fixedSize(track.minimum, available);
		if(track.maximum.unit == TrackUnit::Fraction) sizing[i].flex = std::max(track.maximum.value, 0.0f);
		else sizing[i].limit = fixedSize(track.maximum, available);
	}

	// Content: the largest child in each track (its minimum, its maximum, or both)
	for(const TrackContent& content : contents) {
		if(content.span != 1 || content.first >= count) continue;
		const Track track = content.first < tracks.size() ? tracks[content.first] : Track::content();
		Sizing& target = sizing[content.first];
		if(track.minimum.unit == TrackUnit::Content) target.base = std::max(target.base, content.size);
		if(track.maximum.unit == TrackUnit::Content) target.limit = std::max(target.limit, content.size);
	}
	std::vector<TrackContent> spanning;
	for(const TrackContent& content : contents)
		if(content.span > 1 && content.first < count)
			spanning.push_back({content.first, std::min(content.span, count - content.first), content.size});
	fitSpanningContent(tracks, sizing, spanning, gap);

	const float gaps = count > 0 ? gap * static_cast<float>(count - 1) : 0.0f;
	float free = available - gaps;
	for(Sizing& track : sizing) {
		track.limit = std::max(track.limit, track.base);
		free -= track.base;
	}
	growToLimits(sizing, free);
	shareFractions(sizing, available - gaps);

	sizes.resize(count);
	for(std::size_t i = 0; i < count; ++i) sizes[i] = sizing[i].base;
}

// Each start adds the previous track and the gap as one step, as the Grid always has
void trackStarts(std::span<const float> sizes, float gap, std::vector<float>& starts) {
	starts.resize(sizes.size());
	float start = 0.0f;
	for(std::size_t i = 0; i < sizes.size(); ++i) {
		starts[i] = start;
		start += sizes[i] + gap;
	}
}

} // namespace affresco::detail
