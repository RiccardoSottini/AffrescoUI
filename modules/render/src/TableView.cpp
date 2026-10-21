/// @file
/// @brief TableView: sorting the rows, the header, resizing columns and drawing the cells.

#include "affresco/render/TableView.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Painter.hpp"
#include "detail/Utf8.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <string_view>
#include <utility>

namespace affresco {

namespace {

constexpr float edgeReach = 4.0f;              ///< How near a header's right edge, in pixels, a press resizes.
constexpr std::u32string_view ellipsis = U"…"; ///< Ends a cell's text cut short.

/// @brief A cell as a number, for sorting.
/// @param text UTF-8 text.
/// @return Its value when the whole text is a finite number, else NaN.
[[nodiscard]] double numberOf(const std::string& text) noexcept {
	if(text.empty() || text.size() > 64) return std::numeric_limits<double>::quiet_NaN();
	char* end = nullptr;
	const double value = std::strtod(text.c_str(), &end);
	const bool whole = end == text.c_str() + text.size() && std::isfinite(value);
	return whole ? value : std::numeric_limits<double>::quiet_NaN();
}

/// @brief An item's cell.
/// @param item The item.
/// @param cell The cell's index.
/// @return Its text, or empty when the item has fewer cells.
[[nodiscard]] std::string_view cellOf(const ListItem& item, std::size_t cell) noexcept {
	return cell < item.cells.size() ? std::string_view(item.cells[cell]) : std::string_view();
}

} // namespace

void TableView::setColumns(std::vector<TableColumn> columns) {
	if(columns == givenColumns_) return;
	for(TableColumn& column : columns) column.width = std::max(column.width, minimumColumnWidth());
	givenColumns_ = columns;
	columns_ = std::move(columns);
	titles_.resize(columns_.size());
	for(std::size_t i = 0; i < columns_.size(); ++i) detail::decodeInto(titles_[i], columns_[i].title);
	resizing_ = columns_.size();
	rebuildRows(false); // the cells drawn per row changed
}

void TableView::setSort(TableSort sort) {
	if(givenSort_ && sort == *givenSort_) return;
	givenSort_ = sort;
	if(sort == sort_) return;
	sort_ = std::move(sort);
	rebuildRows(false);
}

std::size_t TableView::sortColumn() const noexcept {
	if(sort_.column.empty()) return columns_.size();
	for(std::size_t i = 0; i < columns_.size(); ++i)
		if(columns_[i].id == sort_.column) return i;
	return columns_.size();
}

void TableView::buildRows(std::vector<std::size_t>& rows) {
	const std::size_t size = model() ? model()->size() : 0;
	rows.resize(size);
	std::iota(rows.begin(), rows.end(), std::size_t{0});
	const std::size_t column = sortColumn();
	if(column < columns_.size() && size > 1) {
		const ListModel& items = *model();
		numbers_.resize(size);
		for(std::size_t i = 0; i < size; ++i) {
			const ListItem& item = items.item(i);
			numbers_[i] = column < item.cells.size() ? numberOf(item.cells[column]) : std::nan("");
		}
		// Numbers by value before text, text by its bytes; stable, so equal cells keep the model's order
		const auto before = [&](std::size_t a, std::size_t b) {
			const double x = numbers_[a], y = numbers_[b];
			if(!std::isnan(x) || !std::isnan(y)) return std::isnan(y) ? !std::isnan(x) : !std::isnan(x) && x < y;
			return cellOf(items.item(a), column) < cellOf(items.item(b), column);
		};
		if(sort_.descending) std::stable_sort(rows.begin(), rows.end(), [&](auto a, auto b) { return before(b, a); });
		else std::stable_sort(rows.begin(), rows.end(), before);
	}
	rowOfItem_.resize(size);
	for(std::size_t row = 0; row < size; ++row) rowOfItem_[rows[row]] = row;
}

std::size_t TableView::rowOf(std::size_t item) const noexcept {
	return item < rowOfItem_.size() && rowOfItem_.size() == rowCount() ? rowOfItem_[item] : rowCount();
}

float TableView::columnLeft(std::size_t column) const noexcept {
	float left = 0.0f;
	for(std::size_t i = 0; i < column && i < columns_.size(); ++i) left += columns_[i].width;
	return left;
}

std::size_t TableView::edgeAt(float x) const noexcept {
	float right = frame().left;
	for(std::size_t i = 0; i < columns_.size(); ++i) {
		right += columns_[i].width;
		if(std::abs(x - right) <= edgeReach) return i;
	}
	return columns_.size();
}

std::size_t TableView::columnAt(float x) const noexcept {
	float right = frame().left;
	for(std::size_t i = 0; i < columns_.size(); ++i) {
		right += columns_[i].width;
		if(x < right) return i;
	}
	return columns_.size();
}

void TableView::sortBy(std::size_t column) {
	TableSort sort{columns_[column].id, false};
	if(sort_.column == sort.column) sort.descending = !sort_.descending;
	sort_ = sort;
	rebuildRows(false);
	onSortChanged(sort); // a copy: the handler may give the table a new order
}

void TableView::mouseButtonEvent(const MouseButtonEvent& event) {
	const bool inHeader = frame().contains(event.position) && event.position.y < frame().top + headerSize();
	if(event.button == MouseButton::Left && event.action == Action::Press && inHeader) {
		const std::size_t edge = edgeAt(event.position.x);
		const std::size_t column = columnAt(event.position.x);
		if(edge < columns_.size()) {
			resizing_ = edge;
			grab_ = event.position.x - (frame().left + columnLeft(edge + 1));
		} else if(column < columns_.size()) sortBy(column);
	} else if(event.button == MouseButton::Left && event.action == Action::Release && resizing_ < columns_.size()) {
		const ColumnResize change{columns_[resizing_].id, columns_[resizing_].width};
		resizing_ = columns_.size();
		onColumnResized(change);
	}
	ListView::mouseButtonEvent(event);
}

void TableView::mouseMoveEvent(const MouseMoveEvent& event) {
	if(resizing_ < columns_.size()) {
		const float left = frame().left + columnLeft(resizing_);
		const float width = std::max(std::round(event.position.x - grab_ - left), minimumColumnWidth());
		if(width != columns_[resizing_].width) {
			columns_[resizing_].width = width;
			invalidatePaint();
		}
	} else {
		const bool onEdge = event.position.y < frame().top + headerSize() && edgeAt(event.position.x) < columns_.size();
		setCursor(onEdge ? Cursor::ResizeHorizontal : Cursor::Arrow);
	}
	ListView::mouseMoveEvent(event);
}

void TableView::paintCell(Painter& painter, const Font& font, std::u32string_view text, float left, float width,
                          float baseline, Color color) {
	const float room = width - 2.0f * padding();
	if(room <= 0.0f || text.empty()) return;
	if(font.measure(text) <= room) {
		painter.drawText(font, text, {left + padding(), baseline}, color);
		return;
	}
	// The longest prefix that fits with the ellipsis, found by halving
	const float dots = font.measure(ellipsis);
	std::size_t low = 0, high = text.size();
	while(low < high) {
		const std::size_t middle = (low + high + 1) / 2;
		if(font.measure(text.substr(0, middle)) + dots <= room) low = middle;
		else high = middle - 1;
	}
	const std::u32string_view shown = text.substr(0, low);
	painter.drawText(font, shown, {left + padding(), baseline}, color);
	painter.drawText(font, ellipsis, {left + padding() + font.measure(shown), baseline}, color);
}

void TableView::paintRow(Painter& painter, const Font& font, std::size_t row, const Rect& area, Color color) const {
	const float baseline = std::round(area.top + (area.height() - font.lineHeight()) / 2.0f + font.ascent());
	float left = area.left;
	for(std::size_t i = 0; i < columns_.size() && left < area.right; ++i) {
		paintCell(painter, font, shownText(row, i), left, columns_[i].width, baseline, color);
		left += columns_[i].width;
	}
}

void TableView::paintSortMark(Painter& painter, float left, float middle) const {
	// A small triangle of pixel steps, pointing up when ascending, so it stays crisp without a path renderer
	const float top = std::round(middle) - 2.0f;
	for(int step = 0; step < 4; ++step) {
		const auto offset = static_cast<float>(step);
		const float y = sort_.descending ? top + 3.0f - offset : top + offset;
		painter.fillRect({left + 3.0f - offset, y, left + 4.0f + offset, y + 1.0f}, accent());
	}
}

void TableView::paintOverlay(Painter& painter, const Font& font) const {
	const Rect& f = frame();
	const Rect header{f.left, f.top, f.right, f.top + headerSize()};
	const Color line = accent().withAlpha(0.25f);
	painter.fillRect(header, color());
	painter.fillRect(header, accent().withAlpha(0.06f));
	const float baseline = std::round(header.top + (headerSize() - font.lineHeight()) / 2.0f + font.ascent());
	const std::size_t sorted = sortColumn();
	float left = f.left;
	for(std::size_t i = 0; i < columns_.size() && left < f.right; ++i) {
		const float width = columns_[i].width;
		const float arrow = i == sorted ? 12.0f : 0.0f; // room for the sort mark
		paintCell(painter, font, titles_[i], left, width - arrow, baseline, textColor());
		if(i == sorted) paintSortMark(painter, left + width - padding() - 7.0f, header.top + headerSize() / 2.0f);
		left += width;
		painter.fillRect({left - 1.0f, header.top + 6.0f, left, header.bottom - 6.0f}, line);
	}
	painter.fillRect({f.left, header.bottom - 1.0f, f.right, header.bottom}, line);
}

} // namespace affresco
