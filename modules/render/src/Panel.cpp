#include "affresco/render/Panel.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>

namespace affresco {

namespace {

/// @brief The anchors_ slot of an edge.
/// @param edge The edge.
/// @return Its index.
constexpr std::size_t index(Edge edge) noexcept { return static_cast<std::size_t>(edge); }

} // namespace

Panel& Panel::addChild(std::unique_ptr<Panel> child) {
	Panel& ref = *child;
	adopt(std::move(child));
	return ref;
}

Panel::~Panel() {
	if(window_) window_->forget(*this); // no dangling focus/hover/capture pointers
}

Panel& Panel::replaceChild(std::size_t index, std::unique_ptr<Panel> child) {
	child->parent_ = this;
	child->attachTo(window_);
	children_.at(index) = std::move(child);
	invalidate();
	return *children_[index];
}

void Panel::removeChildren(std::size_t from) {
	if(from >= children_.size()) return;
	children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(from), children_.end());
	invalidate();
}

Panel* Panel::findById(std::string_view id) noexcept {
	if(id_ == id) return this;
	for(auto& child : children_)
		if(Panel* found = child->findById(id)) return found;
	return nullptr;
}

float Panel::measuredHeight(float reference) {
	prepareLayout();
	return std::max(height_.resolve(reference), minHeight_.resolve(reference));
}

void Panel::adopt(std::unique_ptr<Panel> child) {
	child->parent_ = this;
	child->attachTo(window_);
	children_.push_back(std::move(child));
	invalidate();
}

void Panel::attachTo(Window* window) noexcept {
	window_ = window;
	for(auto& child : children_) child->attachTo(window);
}

void Panel::invalidate() noexcept {
	if(window_) window_->invalidate();
}

/* Position */

void Panel::setPosition(Length x, Length y) {
	x_ = x;
	y_ = y;
	invalidate();
}

void Panel::setX(Length x) {
	x_ = x;
	invalidate();
}

void Panel::setY(Length y) {
	y_ = y;
	invalidate();
}

Vec2 Panel::relativePosition() const noexcept {
	if(!parent_) return {bounds_.left, bounds_.top};
	return {bounds_.left - parent_->bounds_.left, bounds_.top - parent_->bounds_.top};
}

/* Size */

void Panel::setSize(Length width, Length height) {
	width_ = width;
	height_ = height;
	invalidate();
}

void Panel::setWidth(Length width) {
	width_ = width;
	invalidate();
}

void Panel::setHeight(Length height) {
	height_ = height;
	invalidate();
}

void Panel::setMinSize(Length minWidth, Length minHeight) {
	minWidth_ = minWidth;
	minHeight_ = minHeight;
	invalidate();
}

void Panel::setMinWidth(Length minWidth) {
	minWidth_ = minWidth;
	invalidate();
}

void Panel::setMinHeight(Length minHeight) {
	minHeight_ = minHeight;
	invalidate();
}

/* Anchors */

void Panel::setAnchor(Edge edge, Length offset) {
	anchors_[index(edge)] = offset;
	invalidate();
}

void Panel::clearAnchor(Edge edge) {
	anchors_[index(edge)].reset();
	invalidate();
}

std::optional<Length> Panel::anchor(Edge edge) const noexcept { return anchors_[index(edge)]; }

/* Appearance */

void Panel::setColor(Color color) {
	color_ = color;
	invalidate();
}

void Panel::setCursor(Cursor cursor) {
	cursor_ = cursor;
	if(window_) window_->updateCursor();
}

void Panel::setVisible(bool visible) {
	visible_ = visible;
	invalidate();
}

/* Layout */

void Panel::layout(const Rect& parentBounds, Vec2 parentSize, bool parentShown) {
	prepareLayout();

	size_ = {std::max(width_.resolve(parentSize.x), minWidth_.resolve(parentSize.x)),
	         std::max(height_.resolve(parentSize.y), minHeight_.resolve(parentSize.y))};

	const auto& top = anchors_[index(Edge::Top)];
	const auto& bottom = anchors_[index(Edge::Bottom)];
	const auto& left = anchors_[index(Edge::Left)];
	const auto& right = anchors_[index(Edge::Right)];

	// Bottom/right anchors refer to the parent's unclipped size
	const float parentBottom = parentBounds.top + parentSize.y;
	const float parentRight = parentBounds.left + parentSize.x;

	Rect rect;

	if(top) rect.top = parentBounds.top + top->resolve(parentSize.y);
	else if(bottom) rect.top = parentBottom - bottom->resolve(parentSize.y) - size_.y;
	else rect.top = parentBounds.top + y_.resolve(parentSize.y);

	rect.bottom = bottom ? parentBottom - bottom->resolve(parentSize.y) : rect.top + size_.y;

	if(left) rect.left = parentBounds.left + left->resolve(parentSize.x);
	else if(right) rect.left = parentRight - right->resolve(parentSize.x) - size_.x;
	else rect.left = parentBounds.left + x_.resolve(parentSize.x);

	rect.right = right ? parentRight - right->resolve(parentSize.x) : rect.left + size_.x;

	// A panel anchored on opposite edges is stretched: its real size (what children measure against) comes from them
	size_ = {std::max(rect.width(), 0.0f), std::max(rect.height(), 0.0f)};

	frame_ = rect;

	// Clip to the parent
	rect.top = std::max(rect.top, parentBounds.top);
	rect.bottom = std::min(rect.bottom, parentBounds.bottom);
	rect.left = std::max(rect.left, parentBounds.left);
	rect.right = std::min(rect.right, parentBounds.right);

	bounds_ = rect;
	shown_ = parentShown && visible_ && rect.bottom >= rect.top && rect.right >= rect.left;

	for(auto& child : children_) child->layout(bounds_, size_, shown_);
}

Panel* Panel::hitTest(Vec2 point) noexcept {
	if(!shown_ || !bounds_.contains(point)) return nullptr;

	for(auto it = children_.rbegin(); it != children_.rend(); ++it) {
		if(Panel* hit = (*it)->hitTest(point)) return hit;
	}

	return this;
}

void Panel::paint(Painter& painter) const { painter.fillRect(bounds_, color_); }

bool Panel::hasFocus() const noexcept { return window_ && window_->focusedPanel() == this; }

} // namespace affresco
