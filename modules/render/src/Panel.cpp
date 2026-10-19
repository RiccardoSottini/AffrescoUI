#include "affresco/render/Panel.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace affresco {

namespace {

/// @brief The anchors_ slot of an edge.
/// @param edge The edge.
/// @return Its index.
constexpr std::size_t index(Edge edge) noexcept { return static_cast<std::size_t>(edge); }

/// @brief The position of a rectangle's edge.
/// @param rect The rectangle.
/// @param edge The edge.
/// @return Its coordinate.
constexpr float edgeOf(const Rect& rect, Edge edge) noexcept {
	switch(edge) {
	case Edge::Top:
		return rect.top;
	case Edge::Bottom:
		return rect.bottom;
	case Edge::Left:
		return rect.left;
	case Edge::Right:
		return rect.right;
	}
	return rect.top;
}

constexpr float scrollBarWidth = 4.0f; ///< Thickness of the scroll bar, in pixels.

/// @brief A size within its bounds; the minimum wins over the maximum, as in CSS.
/// @param size The size.
/// @param minimum The minimum.
/// @param maximum The maximum, if any.
/// @param reference What percentages refer to.
/// @return In pixels.
float bounded(Length size, Length minimum, const std::optional<Length>& maximum, float reference) noexcept {
	const float own = size.resolve(reference);
	return std::max(maximum ? std::min(own, maximum->resolve(reference)) : own, minimum.resolve(reference));
}

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

void Panel::moveChild(std::size_t from, std::size_t to) {
	if(from >= children_.size() || to >= children_.size()) throw std::out_of_range("Panel::moveChild: no such child");
	if(from == to) return;
	std::unique_ptr<Panel> child = std::move(children_[from]);
	children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(from));
	children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(to), std::move(child));
	invalidate();
}

Panel* Panel::findById(std::string_view id) noexcept {
	if(id_ == id) return this;
	for(auto& child : children_)
		if(Panel* found = child->findById(id)) return found;
	return nullptr;
}

Vec2 Panel::measuredSize(Vec2 reference) {
	prepareLayout();
	const Vec2 size{bounded(width_, minWidth_, maxWidth_, reference.x),
	                bounded(height_, minHeight_, maxHeight_, reference.y)};
	return withAspectRatio(size, width_.value != 0.0f, height_.value != 0.0f, reference);
}

float Panel::measuredHeight(float reference) { return measuredSize({parent_ ? parent_->size_.x : 0.0f, reference}).y; }

float Panel::measuredWidth(float reference) { return measuredSize({reference, parent_ ? parent_->size_.y : 0.0f}).x; }

// The axis that is set leads; with both or neither set there is nothing to follow
Vec2 Panel::withAspectRatio(Vec2 size, bool widthSet, bool heightSet, Vec2 reference) const noexcept {
	if(!(aspectRatio_ > 0.0f) || widthSet == heightSet) return size;
	if(widthSet) size.y = bounded(px(size.x / aspectRatio_), minHeight_, maxHeight_, reference.y);
	else size.x = bounded(px(size.y * aspectRatio_), minWidth_, maxWidth_, reference.x);
	return size;
}

void Panel::arrangeSize(Panel& child, std::optional<float> width, std::optional<float> height) noexcept {
	child.arrangedWidth_ = width;
	child.arrangedHeight_ = height;
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

void Panel::setMaxSize(std::optional<Length> maxWidth, std::optional<Length> maxHeight) {
	maxWidth_ = maxWidth;
	maxHeight_ = maxHeight;
	invalidate();
}

void Panel::setMaxWidth(std::optional<Length> maxWidth) {
	maxWidth_ = maxWidth;
	invalidate();
}

void Panel::setMaxHeight(std::optional<Length> maxHeight) {
	maxHeight_ = maxHeight;
	invalidate();
}

void Panel::setAspectRatio(float ratio) {
	aspectRatio_ = std::isfinite(ratio) ? std::max(ratio, 0.0f) : 0.0f;
	invalidate();
}

/* Anchors */

void Panel::setAnchor(Edge edge, Length offset) {
	anchors_[index(edge)] = offset;
	targets_[index(edge)].reset();
	invalidate();
}

void Panel::setAnchorTo(Edge edge, std::string id, Edge targetEdge, float offset) {
	anchors_[index(edge)] = px(0.0f);
	targets_[index(edge)] = AnchorTarget{std::move(id), targetEdge, offset};
	invalidate();
}

void Panel::clearAnchor(Edge edge) {
	anchors_[index(edge)].reset();
	targets_[index(edge)].reset();
	invalidate();
}

std::optional<Length> Panel::anchor(Edge edge) const noexcept { return anchors_[index(edge)]; }

void Panel::setGrow(float grow) {
	grow_ = std::max(grow, 0.0f);
	invalidate();
}

void Panel::setShrink(float shrink) {
	shrink_ = std::max(shrink, 0.0f);
	invalidate();
}

void Panel::setBasis(std::optional<Length> basis) {
	basis_ = basis;
	invalidate();
}

void Panel::setAlignSelf(std::optional<CrossAlign> align) {
	alignSelf_ = align;
	invalidate();
}

void Panel::setGridCell(GridCell cell) {
	gridCell_ = cell;
	invalidate();
}

/* Scrolling */

void Panel::setScrollable(bool horizontal, bool vertical) {
	scrollX_ = horizontal;
	scrollY_ = vertical;
	if(!scrollX_) scroll_.x = 0.0f;
	if(!scrollY_) scroll_.y = 0.0f;
	invalidate();
}

// Within the content laid out last time: the offset ranges over the part that doesn't fit
bool Panel::scrollBy(Vec2 delta) {
	const Vec2 before = scroll_;
	if(scrollX_)
		scroll_.x = std::clamp(scroll_.x + delta.x, std::min(contentMin_.x, 0.0f),
		                       std::max(contentMax_.x - size_.x, std::min(contentMin_.x, 0.0f)));
	if(scrollY_)
		scroll_.y = std::clamp(scroll_.y + delta.y, std::min(contentMin_.y, 0.0f),
		                       std::max(contentMax_.y - size_.y, std::min(contentMin_.y, 0.0f)));
	if(scroll_ == before) return false;
	invalidate();
	return true;
}

void Panel::scrollIntoView() {
	for(Panel* ancestor = parent_; ancestor; ancestor = ancestor->parent_) {
		if(!ancestor->isScrollable()) continue;
		const Rect& area = ancestor->frame_;
		Vec2 delta;
		if(ancestor->scrollY_) {
			if(frame_.bottom > area.bottom) delta.y = frame_.bottom - area.bottom;
			if(frame_.top - delta.y < area.top) delta.y = frame_.top - area.top;
		}
		if(ancestor->scrollX_) {
			if(frame_.right > area.right) delta.x = frame_.right - area.right;
			if(frame_.left - delta.x < area.left) delta.x = frame_.left - area.left;
		}
		ancestor->scrollBy(delta);
	}
}

/* Overlays */

void Panel::setOverlay(std::string anchorId, Placement placement) {
	overlay_ = true;
	overlayAnchor_ = std::move(anchorId);
	placement_ = placement;
	invalidate();
}

void Panel::clearOverlay() {
	if(!overlay_) return;
	overlay_ = false;
	overlayAnchor_.clear();
	invalidate();
}

void Panel::setTooltip(std::string text) { tooltip_ = std::move(text); }

/* Appearance */

void Panel::setColor(Color color) {
	color_ = color;
	invalidate();
}

void Panel::setCornerRadii(CornerRadii radii) {
	radii_ = radii;
	invalidate();
}

void Panel::setBorder(float width, Color color) {
	borderWidth_ = width;
	borderColor_ = color;
	invalidate();
}

void Panel::setShadow(const Shadow& shadow) {
	shadow_ = shadow;
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

// Parent anchors are measured from the parent's (scrolled) frame; bottom/right ones from its unclipped size
std::optional<float> Panel::anchoredEdge(Edge edge, const Rect& origin, Vec2 parentSize) const {
	const auto& anchor = anchors_[index(edge)];
	if(!anchor) return std::nullopt;

	if(const auto& target = targets_[index(edge)]) {
		const Panel* other = window_ ? window_->root().findById(target->id) : nullptr;
		if(!other) return std::nullopt;
		if(other->layoutPass_ != window_->layoutPass_) window_->forwardAnchor_ = true; // a later panel: lay out again
		return edgeOf(other->frame_, target->edge) + target->offset;
	}

	switch(edge) {
	case Edge::Top:
		return origin.top + anchor->resolve(parentSize.y);
	case Edge::Bottom:
		return origin.top + parentSize.y - anchor->resolve(parentSize.y);
	case Edge::Left:
		return origin.left + anchor->resolve(parentSize.x);
	case Edge::Right:
		return origin.left + parentSize.x - anchor->resolve(parentSize.x);
	}
	return std::nullopt;
}

void Panel::layout(const Rect& origin, const Rect& clip, Vec2 parentSize, bool parentShown) {
	prepareLayout();
	if(window_) layoutPass_ = window_->layoutPass_;

	size_ = {arrangedWidth_ ? *arrangedWidth_ : bounded(width_, minWidth_, maxWidth_, parentSize.x),
	         arrangedHeight_ ? *arrangedHeight_ : bounded(height_, minHeight_, maxHeight_, parentSize.y)};

	const auto top = anchoredEdge(Edge::Top, origin, parentSize);
	const auto bottom = anchoredEdge(Edge::Bottom, origin, parentSize);
	const auto left = anchoredEdge(Edge::Left, origin, parentSize);
	const auto right = anchoredEdge(Edge::Right, origin, parentSize);
	if(aspectRatio_ > 0.0f) {
		// An axis stretched by its anchors is set too: the other one follows its stretched size
		const bool stretchedX = left && right && !arrangedWidth_, stretchedY = top && bottom && !arrangedHeight_;
		if(stretchedX) size_.x = std::max(*right - *left, 0.0f);
		if(stretchedY) size_.y = std::max(*bottom - *top, 0.0f);
		size_ = withAspectRatio(size_, arrangedWidth_ || stretchedX || width_.value != 0.0f,
		                        arrangedHeight_ || stretchedY || height_.value != 0.0f, parentSize);
	}

	Rect rect;

	if(top) rect.top = *top;
	else if(bottom) rect.top = *bottom - size_.y;
	else rect.top = origin.top + y_.resolve(parentSize.y);

	rect.bottom = bottom && (!top || !arrangedHeight_) ? *bottom : rect.top + size_.y;

	if(left) rect.left = *left;
	else if(right) rect.left = *right - size_.x;
	else rect.left = origin.left + x_.resolve(parentSize.x);

	rect.right = right && (!left || !arrangedWidth_) ? *right : rect.left + size_.x;

	// A panel anchored on opposite edges is stretched: its real size (what children measure against) comes from them
	size_ = {std::max(rect.width(), 0.0f), std::max(rect.height(), 0.0f)};

	frame_ = rect;

	// Clip to the parent
	rect.top = std::max(rect.top, clip.top);
	rect.bottom = std::min(rect.bottom, clip.bottom);
	rect.left = std::max(rect.left, clip.left);
	rect.right = std::min(rect.right, clip.right);

	bounds_ = rect;
	shown_ = parentShown && visible_ && rect.bottom >= rect.top && rect.right >= rect.left;

	arrange();
	layoutChildren();
}

void Panel::layoutChildren() {
	auto pass = [this] {
		const Rect origin{frame_.left - scroll_.x, frame_.top - scroll_.y, frame_.right - scroll_.x,
		                  frame_.bottom - scroll_.y};
		for(auto& child : children_)
			if(!child->overlay_) child->layout(origin, bounds_, size_, shown_);
	};
	pass();
	if(!isScrollable()) return;

	// The content's extent, as if unscrolled; an offset the content no longer allows is pulled back
	contentMin_ = contentMax_ = Vec2{};
	for(const auto& child : children_) {
		if(child->overlay_ || !child->visible_) continue;
		const Rect& f = child->frame_;
		contentMin_ = {std::min(contentMin_.x, f.left + scroll_.x - frame_.left),
		               std::min(contentMin_.y, f.top + scroll_.y - frame_.top)};
		contentMax_ = {std::max(contentMax_.x, f.right + scroll_.x - frame_.left),
		               std::max(contentMax_.y, f.bottom + scroll_.y - frame_.top)};
	}
	if(scrollBy({0.0f, 0.0f})) pass();
}

Panel* Panel::hitTest(Vec2 point) noexcept {
	if(!shown_ || !bounds_.contains(point)) return nullptr;

	for(auto it = children_.rbegin(); it != children_.rend(); ++it) {
		if((*it)->overlay_) continue; // hit-tested by the window, above everything
		if(Panel* hit = (*it)->hitTest(point)) return hit;
	}

	return this;
}

// Square corners keep the plain fill, so panels without the new properties look exactly as they always have
void Panel::paint(Painter& painter) const {
	if(shadow_.isVisible()) painter.drawShadow(frame_, radii_, shadow_);
	if(radii_.isSquare()) painter.fillRect(bounds_, color_);
	else painter.fillRoundedRect(frame_, radii_, color_);
	if(borderWidth_ > 0.0f) painter.strokeRoundedRect(frame_, radii_, borderWidth_, borderColor_);
}

void Panel::paintOver(Painter& painter) const {
	if(!isScrollable()) return;
	const Color thumb = colors::muted.withAlpha(0.7f);

	// A thumb as long as the visible share of the content, where the visible part is
	const float contentHeight = contentMax_.y - std::min(contentMin_.y, 0.0f);
	if(scrollY_ && contentHeight > size_.y + 0.5f) {
		const float length = std::max(size_.y * size_.y / contentHeight, 16.0f);
		const float top =
		    frame_.top + (scroll_.y - std::min(contentMin_.y, 0.0f)) / (contentHeight - size_.y) * (size_.y - length);
		painter.fillRect({frame_.right - scrollBarWidth - 2.0f, top, frame_.right - 2.0f, top + length}, thumb);
	}
	const float contentWidth = contentMax_.x - std::min(contentMin_.x, 0.0f);
	if(scrollX_ && contentWidth > size_.x + 0.5f) {
		const float length = std::max(size_.x * size_.x / contentWidth, 16.0f);
		const float left =
		    frame_.left + (scroll_.x - std::min(contentMin_.x, 0.0f)) / (contentWidth - size_.x) * (size_.x - length);
		painter.fillRect({left, frame_.bottom - scrollBarWidth - 2.0f, left + length, frame_.bottom - 2.0f}, thumb);
	}
}

bool Panel::hasFocus() const noexcept { return window_ && window_->focusedPanel() == this; }

} // namespace affresco
