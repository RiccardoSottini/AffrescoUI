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

/// @brief A pair's value along a direction.
/// @param pair Width and height, or x and y.
/// @param row Whether the direction is a row.
/// @return Its x in a row, its y in a column.
float along(Vec2 pair, bool row) noexcept { return row ? pair.x : pair.y; }

/// @brief A pair's value across a direction.
/// @param pair Width and height, or x and y.
/// @param row Whether the direction is a row.
/// @return Its y in a row, its x in a column.
float across(Vec2 pair, bool row) noexcept { return row ? pair.y : pair.x; }

/// @brief Gives a child stretched across, with a ratio and no length of its own along the direction, the size along
/// it that the stretched size and the ratio make.
/// @param child The child.
/// @param row Whether the container is a row.
/// @param crossRoom The container's size across.
/// @param item The child's item, with its alignment.
void followStretchedRatio(const Panel& child, bool row, float crossRoom, detail::FlexItem& item) {
	const Length ownMain = row ? child.width() : child.height();
	if(!(child.aspectRatio() > 0.0f) || ownMain.value != 0.0f || child.basis() || item.align != CrossAlign::Stretch)
		return;
	const float stretched = std::max(std::min(crossRoom, item.maxCross), item.minCross);
	item.base = row ? stretched * child.aspectRatio() : stretched / child.aspectRatio();
	item.sized = true;
}

/// @brief What the flexible layout needs to know about a child.
/// @param child The child, measured with no arranged size.
/// @param row Whether the container is a row.
/// @param size The container's size.
/// @param align The container's alignment across.
/// @return The item.
detail::FlexItem flexItem(Panel& child, bool row, Vec2 size, std::optional<CrossAlign> align) {
	const Vec2 measured = child.measuredSize(size);
	const Vec2 minimum{child.minWidth().resolve(size.x), child.minHeight().resolve(size.y)};
	const Vec2 maximum{maximumOf(child.maxWidth(), size.x), maximumOf(child.maxHeight(), size.y)};
	const std::optional<Length> basis = child.basis();
	detail::FlexItem item{.base = basis ? basis->resolve(along(size, row)) : along(measured, row),
	                      .minMain = along(minimum, row),
	                      .maxMain = along(maximum, row),
	                      .cross = across(measured, row),
	                      .minCross = across(minimum, row),
	                      .maxCross = across(maximum, row),
	                      .grow = child.grow(),
	                      .shrink = child.shrink(),
	                      .align = child.alignSelf() ? child.alignSelf() : align,
	                      .sized = basis.has_value()};
	followStretchedRatio(child, row, across(size, row), item);
	return item;
}

/// @brief The edges a direction places children from: along it, and across it.
struct FlowEdges {
	Edge start;      ///< Where it starts along.
	Edge end;        ///< Where it ends along.
	Edge crossStart; ///< Where it starts across.
	Edge crossEnd;   ///< Where it ends across.
};

/// @brief The edges of a row or a column.
/// @param row Whether it is a row.
/// @return Left to right and top to bottom for a row; top to bottom and left to right for a column.
constexpr FlowEdges flowEdges(bool row) noexcept {
	if(row) return {Edge::Left, Edge::Right, Edge::Top, Edge::Bottom};
	return {Edge::Top, Edge::Bottom, Edge::Left, Edge::Right};
}

} // namespace

Stack::Stack() { setColor(colors::transparent); }

void Stack::setGap(float gap) { setProperty(gap_, gap); }

void Stack::setRowGap(std::optional<float> gap) { setProperty(rowGap_, gap); }

void Stack::setColumnGap(std::optional<float> gap) { setProperty(columnGap_, gap); }

void Stack::setAlignment(Align alignment) {
	setJustify(alignment == Align::Center ? Justify::Center : alignment == Align::End ? Justify::End : Justify::Start);
}

Align Stack::alignment() const noexcept {
	return justify_ == Justify::Center ? Align::Center : justify_ == Justify::End ? Align::End : Align::Start;
}

void Stack::setJustify(Justify justify) { setProperty(justify_, justify); }

void Stack::setAlignItems(std::optional<CrossAlign> align) { setProperty(alignItems_, align); }

void Stack::setDirection(Direction direction) { setProperty(direction_, direction); }

void Stack::setWrap(bool wrap) { setProperty(wrap_, wrap); }

void Stack::setFitContent(bool width, bool height) {
	setProperty(fitWidth_, width);
	setProperty(fitHeight_, height);
}

Vec2 Stack::fittedSize(Vec2 size, Vec2 reference) {
	const bool row = direction_ == Direction::Row;
	const std::vector<Panel*> children = placed(*this);
	float length = 0.0f, thickness = 0.0f;
	for(Panel* child : children) {
		const Vec2 measured = child->measuredSize(size);
		const std::optional<Length> basis = child->basis();
		length += basis ? basis->resolve(along(size, row)) : along(measured, row);
		thickness = std::max(thickness, across(measured, row));
	}
	if(children.size() > 1)
		length += static_cast<float>(children.size() - 1) * (row ? columnGap_.value_or(gap_) : rowGap_.value_or(gap_));
	const Vec2 content = row ? Vec2{length, thickness} : Vec2{thickness, length};
	if(fitWidth_)
		size.x = std::max(std::min(content.x, maximumOf(maxWidth(), reference.x)), minWidth().resolve(reference.x));
	if(fitHeight_)
		size.y = std::max(std::min(content.y, maximumOf(maxHeight(), reference.y)), minHeight().resolve(reference.y));
	return size;
}

void Stack::arrange() {
	const bool row = direction_ == Direction::Row;
	const auto [start, end, crossStart, crossEnd] = flowEdges(row);
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
	setColumns(std::vector<Track>(static_cast<std::size_t>(std::clamp(columns, 1, static_cast<int>(maxGridTracks))),
	                              Track::fraction(1.0f)));
}

void Grid::setColumns(std::vector<Track> columns) {
	if(columns.empty()) columns.push_back(Track::fraction(1.0f));
	if(columns.size() > maxGridTracks) columns.resize(maxGridTracks);
	setProperty(columns_, std::move(columns));
}

void Grid::setRows(std::vector<Track> rows) {
	if(rows.size() > maxGridTracks) rows.resize(maxGridTracks);
	setProperty(rows_, std::move(rows));
}

void Grid::setGap(float gap) { setProperty(gap_, gap); }

void Grid::setRowGap(std::optional<float> gap) { setProperty(rowGap_, gap); }

void Grid::setColumnGap(std::optional<float> gap) { setProperty(columnGap_, gap); }

void Grid::setJustifyItems(CrossAlign align) { setProperty(justifyItems_, align); }

void Grid::setAlignItems(CrossAlign align) { setProperty(alignItems_, align); }

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

/// @brief The cells the children ask for, and how many columns the grid has: children that ask for a column past the
/// declared ones add columns, up to maxGridTracks.
/// @param children The grid's placed children.
/// @param declared How many columns the grid declares.
/// @param cells Set to each child's cell, in order.
/// @return The number of columns.
std::size_t gridCells(const std::vector<Panel*>& children, std::size_t declared, std::vector<GridCell>& cells) {
	cells.reserve(children.size());
	std::size_t columnCount = declared;
	for(const Panel* child : children) {
		cells.push_back(child->gridCell());
		const GridCell& cell = cells.back();
		if(cell.column > 0)
			columnCount =
			    std::max(columnCount, static_cast<std::size_t>(cell.column - 1 + std::max(cell.columnSpan, 1)));
	}
	return std::min(columnCount, maxGridTracks);
}

} // namespace

void Grid::arrange() {
	const std::vector<Panel*> children = placed(*this);
	const Vec2 size = resolvedSize();
	const float rowGap = rowGap_.value_or(gap_), columnGap = columnGap_.value_or(gap_);

	std::vector<GridCell> cells;
	const std::size_t columnCount = gridCells(children, columns_.size(), cells);
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
