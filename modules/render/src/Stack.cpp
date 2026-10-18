#include "affresco/render/Stack.hpp"

#include <algorithm>
#include <vector>

namespace affresco {

namespace {

/// @brief The children a container places: visible, and not overlays.
/// @param panel The container.
/// @return The children, in order.
std::vector<Panel*> placed(const Panel& panel) {
	std::vector<Panel*> result;
	for(const auto& child : panel.children())
		if(child->isVisible() && !child->isOverlay()) result.push_back(child.get());
	return result;
}

} // namespace

Stack::Stack() { setColor(colors::transparent); }

void Stack::setGap(float gap) {
	gap_ = gap;
	invalidate();
}

void Stack::setAlignment(Align alignment) {
	alignment_ = alignment;
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
	const float room = row ? resolvedSize().x : resolvedSize().y;
	const float across = row ? resolvedSize().y : resolvedSize().x;
	const std::vector<Panel*> children = placed(*this);

	// Each child's own length along the direction, then spare room shared by the growing ones
	std::vector<float> lengths;
	float total = 0.0f, shares = 0.0f;
	for(Panel* child : children) {
		arrangeSize(*child, std::nullopt, std::nullopt);
		lengths.push_back(row ? child->measuredWidth(room) : child->measuredHeight(room));
		total += lengths.back();
		shares += child->grow();
	}
	if(!children.empty()) total += gap_ * static_cast<float>(children.size() - 1);

	if(row && wrap_) {
		// Lines left to right, top to bottom; the stack sets both anchors of each child
		float x = 0.0f, y = 0.0f, lineHeight = 0.0f;
		for(std::size_t i = 0; i < children.size(); ++i) {
			const float height = children[i]->measuredHeight(across);
			if(x > 0.0f && x + lengths[i] > room) {
				x = 0.0f;
				y += lineHeight + gap_;
				lineHeight = 0.0f;
			}
			children[i]->clearAnchor(Edge::Right);
			children[i]->clearAnchor(Edge::Bottom);
			children[i]->setAnchor(Edge::Left, px(x));
			children[i]->setAnchor(Edge::Top, px(y));
			x += lengths[i] + gap_;
			lineHeight = std::max(lineHeight, height);
		}
		return;
	}

	if(shares > 0.0f && room > total) {
		const float spare = room - total;
		for(std::size_t i = 0; i < children.size(); ++i) {
			if(children[i]->grow() <= 0.0f) continue;
			lengths[i] += spare * children[i]->grow() / shares;
			if(row) arrangeSize(*children[i], lengths[i], std::nullopt);
			else arrangeSize(*children[i], std::nullopt, lengths[i]);
		}
		total = room;
	}

	float offset = 0.0f;
	if(alignment_ == Align::End) {
		for(std::size_t i = children.size(); i-- > 0;) { // from the end back
			children[i]->clearAnchor(start);
			children[i]->setAnchor(end, px(offset));
			offset += lengths[i] + gap_;
		}
		return;
	}
	if(alignment_ == Align::Center) offset = std::max((room - total) / 2.0f, 0.0f);
	for(std::size_t i = 0; i < children.size(); ++i) {
		children[i]->clearAnchor(end);
		children[i]->setAnchor(start, px(offset));
		offset += lengths[i] + gap_;
	}
}

Grid::Grid() { setColor(colors::transparent); }

void Grid::setColumns(int columns) {
	columns_ = std::max(columns, 1);
	invalidate();
}

void Grid::setGap(float gap) {
	gap_ = gap;
	invalidate();
}

void Grid::arrange() {
	const std::vector<Panel*> children = placed(*this);
	const auto columns = static_cast<std::size_t>(columns_);
	const float cell =
	    std::max((resolvedSize().x - gap_ * static_cast<float>(columns - 1)) / static_cast<float>(columns), 0.0f);

	float top = 0.0f;
	for(std::size_t first = 0; first < children.size(); first += columns) {
		float rowHeight = 0.0f;
		for(std::size_t i = first; i < std::min(first + columns, children.size()); ++i) {
			Panel& child = *children[i];
			arrangeSize(child, cell, std::nullopt);
			child.clearAnchor(Edge::Right);
			child.clearAnchor(Edge::Bottom);
			child.setAnchor(Edge::Left, px(static_cast<float>(i - first) * (cell + gap_)));
			child.setAnchor(Edge::Top, px(top));
			rowHeight = std::max(rowHeight, child.measuredHeight(resolvedSize().y));
		}
		top += rowHeight + gap_;
	}
}

} // namespace affresco
