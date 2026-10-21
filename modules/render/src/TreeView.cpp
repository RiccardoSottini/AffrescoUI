/// @file
/// @brief TreeView: rows from the open part of a pre-order list, chevrons, guides, and Left and Right.

#include "affresco/render/TreeView.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Painter.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_set>

namespace affresco {

namespace {

constexpr float chevronSize = 10.0f; ///< The chevron's box, in pixels.
constexpr float chevronGap = 4.0f;   ///< Between the chevron and the label.

} // namespace

void TreeView::setExpanded(const std::vector<std::string>& ids) {
	const std::size_t size = model() ? model()->size() : 0;
	if(ids == lastGiven_ && expandedFor_ == model().get()) return;
	lastGiven_ = ids;
	expandedFor_ = model().get();
	const std::unordered_set<std::string_view> open(ids.begin(), ids.end());
	std::vector<char> flags(size, 0);
	for(std::size_t i = 0; i < size; ++i) flags[i] = open.contains(model()->item(i).id) ? 1 : 0;
	if(flags == expanded_) return;
	// Unchanged flags keep the rows: setModel() already built them
	expanded_ = std::move(flags);
	rebuildRows();
}

void TreeView::expand(std::size_t item, bool expanded) {
	if(!model() || item >= model()->size() || !model()->item(item).expandable) return;
	if(expanded_.size() != model()->size()) expanded_.assign(model()->size(), 0);
	if(isExpanded(item) == expanded) return;
	expanded_[item] = expanded ? 1 : 0;
	rebuildRows();
	const ExpandChange change{model()->item(item).id, expanded}; // the handler may change the items
	onExpandedChanged(change);
}

void TreeView::buildRows(std::vector<std::size_t>& rows) const {
	const ListModel& items = *model();
	const std::size_t size = items.size();
	// One pass in pre-order: a closed item's subtree is skipped whole
	for(std::size_t i = 0; i < size;) {
		rows.push_back(i);
		const ListItem& item = items.item(i);
		i = isExpanded(i) ? i + 1 : std::max(item.end, i + 1);
	}
}

std::size_t TreeView::parentOf(std::size_t item) const noexcept {
	const std::uint32_t depth = model()->item(item).depth;
	if(depth == 0) return model()->size();
	for(std::size_t i = item; i-- > 0;)
		if(model()->item(i).depth < depth) return i;
	return model()->size();
}

float TreeView::chevronLeft(std::size_t item, float left) const noexcept {
	return left + padding() + static_cast<float>(model()->item(item).depth) * indent();
}

bool TreeView::rowKey(const KeyEvent& event) {
	const std::size_t item = cursor();
	if(!model() || item >= model()->size() || (event.key != Key::Right && event.key != Key::Left)) return false;
	const ListItem& current = model()->item(item);
	if(event.key == Key::Right) {
		if(current.expandable && !isExpanded(item)) expand(item, true);
		else if(isExpanded(item) && current.end > item + 1) moveCursor(rowOf(item + 1), {});
		return true;
	}
	if(isExpanded(item)) {
		expand(item, false);
		return true;
	}
	const std::size_t parent = parentOf(item);
	if(parent < model()->size()) moveCursor(rowOf(parent), {});
	return true;
}

bool TreeView::rowPress(std::size_t row, Vec2 position) {
	const std::size_t item = itemAt(row);
	if(!model()->item(item).expandable) return false;
	const float left = chevronLeft(item, frame().left);
	if(position.x < left - 2.0f || position.x > left + chevronSize + 2.0f) return false;
	expand(item, !isExpanded(item));
	return true;
}

void TreeView::paintRow(Painter& painter, const Font& font, std::size_t row, const Rect& area, Color color) const {
	const std::size_t item = itemAt(row);
	const ListItem& entry = model()->item(item);
	const float middle = std::round(area.top + area.height() / 2.0f);
	// Indent guides: one line per level above this item
	for(std::uint32_t level = 0; level < entry.depth; ++level) {
		const float x = std::round(area.left + padding() + static_cast<float>(level) * indent() + chevronSize / 2.0f);
		painter.fillRect({x, area.top, x + 1.0f, area.bottom}, accent().withAlpha(0.25f));
	}
	const float left = chevronLeft(item, area.left);
	if(entry.expandable) paintChevron(painter, left, middle, isExpanded(item), color);
	const float baseline = std::round(area.top + (area.height() - font.lineHeight()) / 2.0f + font.ascent());
	painter.drawText(font, shownText(row, 0), {left + chevronSize + chevronGap, baseline}, color);
}

void TreeView::paintChevron(Painter& painter, float left, float middle, bool open, Color color) {
	// A small triangle of pixel steps, so it stays crisp without a path renderer
	constexpr int steps = 4;
	for(int step = 0; step < steps; ++step) {
		const auto offset = static_cast<float>(step);
		const float span = static_cast<float>(2 * (steps - step) - 1);
		if(open) {
			const float x = std::round(left + chevronSize / 2.0f - span / 2.0f);
			const float y = middle - 2.0f + offset;
			painter.fillRect({x, y, x + span, y + 1.0f}, color);
		} else {
			const float y = std::round(middle - span / 2.0f);
			const float x = left + 3.0f + offset;
			painter.fillRect({x, y, x + 1.0f, y + span}, color);
		}
	}
}

} // namespace affresco
