/// @file
/// @brief Stack and Grid: they measure their children, hand the sizes to the pure layout functions (layout/Flex,
/// layout/Grid), and turn the placements into anchors and arranged sizes.

#include "affresco/render/Stack.hpp"

#include "layout/Flex.hpp"
#include "layout/Grid.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace affresco {

namespace {

/// @brief The children a container places: visible, and not overlays.
/// @param panel The container.
/// @return The children, in order.
std::vector<Panel*> placed(const Panel& panel) {
	std::vector<Panel*> result;
	result.reserve(panel.children().size());
	for(const auto& child : panel.children())
		if(child->isVisible() && !child->isOverlay()) result.push_back(child.get());
	return result;
}

/// @brief A maximum, resolved.
/// @param maximum The maximum, if any.
/// @param reference What percentages refer to.
/// @return In pixels; infinity for none.
float maximumOf(const std::optional<Length>& maximum, float reference) noexcept {
	return maximum ? maximum->resolve(reference) : std::numeric_limits<float>::infinity();
}

/// @brief What the flexible layout needs to know about a child.
/// @param child The child, measured with no arranged size.
/// @param row Whether the container is a row.
/// @param size The container's size.
/// @param align The container's alignment across.
/// @return The item.
detail::FlexItem flexItem(Panel& child, bool row, Vec2 size, std::optional<CrossAlign> align) {
	const Vec2 measured = child.measuredSize(size);
	const float room = row ? size.x : size.y, across = row ? size.y : size.x;
	detail::FlexItem item{.base = row ? measured.x : measured.y,
	                      .minMain = (row ? child.minWidth() : child.minHeight()).resolve(room),
	                      .maxMain = maximumOf(row ? child.maxWidth() : child.maxHeight(), room),
	                      .cross = row ? measured.y : measured.x,
	                      .minCross = (row ? child.minHeight() : child.minWidth()).resolve(across),
	                      .maxCross = maximumOf(row ? child.maxHeight() : child.maxWidth(), across),
	                      .grow = child.grow(),
	                      .shrink = child.shrink(),
	                      .align = child.alignSelf() ? child.alignSelf() : align,
	                      .sized = child.basis().has_value()};
	if(const auto basis = child.basis()) item.base = basis->resolve(room);

	// Stretched across, a child with a ratio and no length of its own along the direction follows the stretched size
	const Length ownMain = row ? child.width() : child.height();
	if(child.aspectRatio() > 0.0f && ownMain.value == 0.0f && !child.basis() && item.align == CrossAlign::Stretch) {
		const float stretched = std::max(std::min(across, item.maxCross), item.minCross);
		item.base = row ? stretched * child.aspectRatio() : stretched / child.aspectRatio();
		item.sized = true;
	}
	return item;
}

} // namespace

Stack::Stack() { setColor(colors::transparent); }

void Stack::setGap(float gap) {
	gap_ = gap;
	invalidate();
}

void Stack::setRowGap(std::optional<float> gap) {
	rowGap_ = gap;
	invalidate();
}

void Stack::setColumnGap(std::optional<float> gap) {
	columnGap_ = gap;
	invalidate();
}

void Stack::setAlignment(Align alignment) {
	setJustify(alignment == Align::Center ? Justify::Center : alignment == Align::End ? Justify::End : Justify::Start);
}

Align Stack::alignment() const noexcept {
	return justify_ == Justify::Center ? Align::Center : justify_ == Justify::End ? Align::End : Align::Start;
}

void Stack::setJustify(Justify justify) {
	justify_ = justify;
	invalidate();
}

void Stack::setAlignItems(std::optional<CrossAlign> align) {
	alignItems_ = align;
	invalidate();
}

void Stack::setDirection(Direction direction) {
	direction_ = direction;
	invalidate();
}

void Stack::setWrap(bool wrap) {
	wrap_ = wrap;
	invalidate();
}

void Stack::arrange() {
	const bool row = direction_ == Direction::Row;
	const Edge start = row ? Edge::Left : Edge::Top, end = row ? Edge::Right : Edge::Bottom;
	const Edge crossStart = row ? Edge::Top : Edge::Left, crossEnd = row ? Edge::Bottom : Edge::Right;
	const Vec2 size = resolvedSize();
	const std::vector<Panel*> children = placed(*this);

	std::vector<detail::FlexItem> items;
	items.reserve(children.size());
	for(Panel* child : children) {
		arrangeSize(*child, std::nullopt, std::nullopt); // measured as itself, not as last pass arranged it
		items.push_back(flexItem(*child, row, size, alignItems_));
	}

	const float rowGap = rowGap_.value_or(gap_), columnGap = columnGap_.value_or(gap_);
	const detail::FlexOptions options{.main = row ? size.x : size.y,
	                                  .cross = row ? size.y : size.x,
	                                  .gap = row ? columnGap : rowGap,
	                                  .lineGap = row ? rowGap : columnGap,
	                                  .justify = justify_,
	                                  .wrap = wrap_};
	std::vector<detail::FlexPlacement> placements;
	detail::layoutFlex(options, items, placements);

	for(std::size_t i = 0; i < children.size(); ++i) {
		Panel& child = *children[i];
		const detail::FlexPlacement& placement = placements[i];
		child.clearAnchor(placement.fromEnd ? start : end);
		child.setAnchor(placement.fromEnd ? end : start, px(placement.main));
		if(placement.crossOffset) {
			child.clearAnchor(crossEnd);
			child.setAnchor(crossStart, px(*placement.crossOffset));
		}
		if(row) arrangeSize(child, placement.mainSize, placement.crossSize);
		else arrangeSize(child, placement.crossSize, placement.mainSize);
	}
}

Grid::Grid() : columns_(2, Track::fraction(1.0f)) { setColor(colors::transparent); }

void Grid::setColumns(int columns) {
	setColumns(
	    std::vector<Track>(static_cast<std::size_t>(std::clamp(columns, 1, static_cast<int>(detail::maxGridTracks))),
		                   Track::fraction(1.0f)));
}

void Grid::setColumns(std::vector<Track> columns) {
	if(columns.empty()) columns.push_back(Track::fraction(1.0f));
	if(columns.size() > detail::maxGridTracks) columns.resize(detail::maxGridTracks);
	columns_ = std::move(columns);
	invalidate();
}

void Grid::setRows(std::vector<Track> rows) {
	if(rows.size() > detail::maxGridTracks) rows.resize(detail::maxGridTracks);
	rows_ = std::move(rows);
	invalidate();
}

void Grid::setGap(float gap) {
	gap_ = gap;
	invalidate();
}

void Grid::setRowGap(std::optional<float> gap) {
	rowGap_ = gap;
	invalidate();
}

void Grid::setColumnGap(std::optional<float> gap) {
	columnGap_ = gap;
	invalidate();
}

void Grid::setJustifyItems(CrossAlign align) {
	justifyItems_ = align;
	invalidate();
}

void Grid::setAlignItems(CrossAlign align) {
	alignItems_ = align;
	invalidate();
}

namespace {

/// @brief Where a child sits in its cell along one axis.
/// @param align Its alignment.
/// @param start The cell's start.
/// @param cell The cell's size.
/// @param own The child's own size.
/// @return Its offset from the grid's edge.
float placedIn(CrossAlign align, float start, float cell, float own) noexcept {
	switch(align) {
	case CrossAlign::Center:
		return start + (cell - own) / 2.0f;
	case CrossAlign::End:
		return start + cell - own;
	case CrossAlign::Start:
	case CrossAlign::Stretch:
		return start;
	}
	return start;
}

/// @brief Whether any of a grid's tracks takes its size from its content.
/// @param tracks The declared tracks.
/// @param count How many tracks there are (the implicit ones are content tracks).
/// @return true if so.
bool hasContentTracks(const std::vector<Track>& tracks, std::size_t count) {
	return count > tracks.size() || std::ranges::any_of(tracks, [](const Track& track) {
		       return track.minimum.unit == TrackUnit::Content || track.maximum.unit == TrackUnit::Content;
	       });
}

/// @brief The size of the tracks from one to another, with the gaps between them.
/// @param starts Where each track starts.
/// @param sizes Each track's size.
/// @param first The first track.
/// @param span How many tracks.
/// @return The size.
float spanned(const std::vector<float>& starts, const std::vector<float>& sizes, std::size_t first, std::size_t span) {
	const std::size_t last = first + span - 1;
	return starts[last] + sizes[last] - starts[first];
}

} // namespace

void Grid::arrange() {
	const std::vector<Panel*> children = placed(*this);
	const Vec2 size = resolvedSize();
	const float rowGap = rowGap_.value_or(gap_), columnGap = columnGap_.value_or(gap_);

	// Children that ask for a column past the declared ones add columns
	std::vector<GridCell> cells;
	cells.reserve(children.size());
	std::size_t columnCount = columns_.size();
	for(const Panel* child : children) {
		cells.push_back(child->gridCell());
		const GridCell& cell = cells.back();
		if(cell.column > 0)
			columnCount =
			    std::max(columnCount, static_cast<std::size_t>(cell.column - 1 + std::max(cell.columnSpan, 1)));
	}
	columnCount = std::min(columnCount, detail::maxGridTracks);
	std::vector<detail::GridArea> areas;
	const std::size_t rowsUsed = detail::placeGridItems(columnCount, cells, areas);

	// Columns first: the children's heights may depend on the width their cells give them
	std::vector<Vec2> own(children.size());
	std::vector<detail::TrackContent> contents;
	const bool measureWidths = hasContentTracks(columns_, columnCount) || justifyItems_ != CrossAlign::Stretch;
	for(std::size_t i = 0; i < children.size(); ++i) {
		arrangeSize(*children[i], std::nullopt, std::nullopt);
		if(!measureWidths) continue;
		own[i].x = children[i]->measuredWidth(size.x);
		contents.push_back({areas[i].column, areas[i].columnSpan, own[i].x});
	}
	std::vector<float> columnSizes, columnStarts;
	detail::sizeTracks(columns_, columnCount, contents, size.x, columnGap, columnSizes);
	detail::trackStarts(columnSizes, columnGap, columnStarts);

	contents.clear();
	for(std::size_t i = 0; i < children.size(); ++i) {
		if(justifyItems_ == CrossAlign::Stretch)
			arrangeSize(*children[i], spanned(columnStarts, columnSizes, areas[i].column, areas[i].columnSpan),
			            std::nullopt);
		own[i].y = children[i]->measuredHeight(size.y);
		contents.push_back({areas[i].row, areas[i].rowSpan, own[i].y});
	}
	std::vector<float> rowSizes, rowStarts;
	detail::sizeTracks(rows_, std::max(rowsUsed, rows_.size()), contents, size.y, rowGap, rowSizes);
	detail::trackStarts(rowSizes, rowGap, rowStarts);

	for(std::size_t i = 0; i < children.size(); ++i) {
		Panel& child = *children[i];
		const detail::GridArea& area = areas[i];
		const float cellWidth = spanned(columnStarts, columnSizes, area.column, area.columnSpan);
		const float cellHeight = spanned(rowStarts, rowSizes, area.row, area.rowSpan);
		const CrossAlign vertical = child.alignSelf().value_or(alignItems_);
		const bool stretchX = justifyItems_ == CrossAlign::Stretch, stretchY = vertical == CrossAlign::Stretch;
		arrangeSize(child, stretchX ? std::optional(cellWidth) : std::nullopt,
		            stretchY ? std::optional(cellHeight) : std::nullopt);
		child.clearAnchor(Edge::Right);
		child.clearAnchor(Edge::Bottom);
		child.setAnchor(Edge::Left, px(placedIn(justifyItems_, columnStarts[area.column], cellWidth, own[i].x)));
		child.setAnchor(Edge::Top, px(placedIn(vertical, rowStarts[area.row], cellHeight, own[i].y)));
	}
}

} // namespace affresco
