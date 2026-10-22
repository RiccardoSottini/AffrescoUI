#include "affresco/render/Panel.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "RetainedGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace affresco {

namespace {

/// @brief The anchors_ slot of an edge.
/// @param edge The edge.
/// @return Its index.
constexpr std::size_t index(Edge edge) noexcept { return static_cast<std::size_t>(edge); }

/// @brief The bit of an edge in Panel::targetEdges_.
/// @param edge The edge.
/// @return Its bit.
constexpr std::uint8_t edgeBit(Edge edge) noexcept { return static_cast<std::uint8_t>(1u << index(edge)); }

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

// Out of line, like the destructor: the geometry it owns is a type only the renderer and this file see
Panel::Panel() = default;

Panel::~Panel() {
	if(window_) window_->forget(*this); // no dangling focus/hover/capture pointers
}

Panel& Panel::replaceChild(std::size_t index, std::unique_ptr<Panel> child) {
	child->parent_ = this;
	child->attachTo(window_);
	children_.at(index) = std::move(child);
	treeChanged();
	invalidate();
	return *children_[index];
}

void Panel::removeChildren(std::size_t from) {
	if(from >= children_.size()) return;
	children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(from), children_.end());
	treeChanged();
	invalidate();
}

void Panel::moveChild(std::size_t from, std::size_t to) {
	if(from >= children_.size() || to >= children_.size()) throw std::out_of_range("Panel::moveChild: no such child");
	if(from == to) return;
	std::unique_ptr<Panel> child = std::move(children_[from]);
	children_.erase(children_.begin() + static_cast<std::ptrdiff_t>(from));
	children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(to), std::move(child));
	treeChanged(); // the first panel of an id, depth first, may be another one now
	invalidate();
}

Panel* Panel::findById(std::string_view id) noexcept {
	if(id_ == id) return this;
	for(auto& child : children_)
		if(Panel* found = child->findById(id)) return found;
	return nullptr;
}

// Kept until the panel changes (invalidate()) or the window starts a new generation (Window::invalidate())
Vec2 Panel::measuredSize(Vec2 reference) {
	const std::uint64_t generation = window_ ? window_->measureGeneration_ : 0;
	if(measured_ && measured_->reference == reference && measured_->generation == generation) return measured_->size;
	prepareLayout();
	Vec2 size{bounded(width_, minWidth_, maxWidth_, reference.x),
	          bounded(height_, minHeight_, maxHeight_, reference.y)};
	if(aspectRatio_ > 0.0f) size = withAspectRatio(size, width_.value != 0.0f, height_.value != 0.0f, reference);
	measured_ = MeasuredSize{reference, size, generation};
	return size;
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
	treeChanged();
	invalidate();
}

void Panel::attachTo(Window* window) noexcept {
	window_ = window;
	for(auto& child : children_) child->attachTo(window);
}

void Panel::setId(std::string id) { id_ = std::move(id); }

void Panel::treeChanged() noexcept {
	if(window_) ++window_->treeGeneration_;
}

void Panel::invalidate() noexcept {
	measured_.reset();
	markLayoutDirty();
	markPaintDirty();
	if(window_) window_->requestLayout();
}

void Panel::invalidatePaint() noexcept {
	markPaintDirty();
	if(window_) window_->requestFrame();
}

void Panel::markPaintDirty() noexcept {
	paintStale_ = true;
	for(Panel* ancestor = parent_; ancestor && !ancestor->paintBelow_; ancestor = ancestor->parent_)
		ancestor->paintBelow_ = true;
}

// Ancestors already marked have theirs marked too, so the walk stops at the first one
void Panel::markLayoutDirty() noexcept {
	layoutDirty_ = true;
	for(Panel* ancestor = parent_; ancestor && !ancestor->layoutBelow_; ancestor = ancestor->parent_)
		ancestor->layoutBelow_ = true;
}

/* Position */

void Panel::setPosition(Length x, Length y) {
	setProperty(x_, x);
	setProperty(y_, y);
}

void Panel::setX(Length x) { setProperty(x_, x); }

void Panel::setY(Length y) { setProperty(y_, y); }

Vec2 Panel::relativePosition() const noexcept {
	if(!parent_) return {bounds_.left, bounds_.top};
	return {bounds_.left - parent_->bounds_.left, bounds_.top - parent_->bounds_.top};
}

/* Size */

void Panel::setSize(Length width, Length height) {
	setProperty(width_, width);
	setProperty(height_, height);
}

void Panel::setWidth(Length width) { setProperty(width_, width); }

void Panel::setHeight(Length height) { setProperty(height_, height); }

void Panel::setMinSize(Length minWidth, Length minHeight) {
	setProperty(minWidth_, minWidth);
	setProperty(minHeight_, minHeight);
}

void Panel::setMinWidth(Length minWidth) { setProperty(minWidth_, minWidth); }

void Panel::setMinHeight(Length minHeight) { setProperty(minHeight_, minHeight); }

void Panel::setMaxSize(std::optional<Length> maxWidth, std::optional<Length> maxHeight) {
	setProperty(maxWidth_, maxWidth);
	setProperty(maxHeight_, maxHeight);
}

void Panel::setMaxWidth(std::optional<Length> maxWidth) { setProperty(maxWidth_, maxWidth); }

void Panel::setMaxHeight(std::optional<Length> maxHeight) { setProperty(maxHeight_, maxHeight); }

void Panel::setAspectRatio(float ratio) {
	setProperty(aspectRatio_, std::isfinite(ratio) ? std::max(ratio, 0.0f) : 0.0f);
}

/* Anchors */

// Containers set their children's anchors on every pass: an anchor that stays the same changes nothing
void Panel::setAnchor(Edge edge, Length offset) {
	if(anchors_[index(edge)] == offset && (targetEdges_ & edgeBit(edge)) == 0) return;
	anchors_[index(edge)] = offset;
	targets_[index(edge)].reset();
	targetEdges_ &= static_cast<std::uint8_t>(~edgeBit(edge));
	invalidate();
}

void Panel::setAnchorTo(Edge edge, std::string id, Edge targetEdge, float offset) {
	AnchorTarget target{std::move(id), targetEdge, offset};
	if(anchors_[index(edge)] == px(0.0f) && targets_[index(edge)] == target) return;
	anchors_[index(edge)] = px(0.0f);
	targets_[index(edge)] = std::move(target);
	targetEdges_ |= edgeBit(edge);
	invalidate();
}

void Panel::clearAnchor(Edge edge) {
	if(!anchors_[index(edge)] && (targetEdges_ & edgeBit(edge)) == 0) return;
	anchors_[index(edge)].reset();
	targets_[index(edge)].reset();
	targetEdges_ &= static_cast<std::uint8_t>(~edgeBit(edge));
	invalidate();
}

std::optional<Length> Panel::anchor(Edge edge) const noexcept { return anchors_[index(edge)]; }

void Panel::setGrow(float grow) { setProperty(grow_, std::max(grow, 0.0f)); }

void Panel::setShrink(float shrink) { setProperty(shrink_, std::max(shrink, 0.0f)); }

void Panel::setBasis(std::optional<Length> basis) { setProperty(basis_, basis); }

void Panel::setAlignSelf(std::optional<CrossAlign> align) { setProperty(alignSelf_, align); }

void Panel::setGridCell(GridCell cell) { setProperty(gridCell_, cell); }

/* Scrolling */

void Panel::setScrollable(bool horizontal, bool vertical) {
	setProperty(scrollX_, horizontal);
	setProperty(scrollY_, vertical);
	if(!scrollX_) setProperty(scroll_.x, 0.0f);
	if(!scrollY_) setProperty(scroll_.y, 0.0f);
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
	setProperty(overlay_, true);
	setProperty(overlayAnchor_, std::move(anchorId));
	setProperty(placement_, placement);
}

void Panel::clearOverlay() {
	setProperty(overlay_, false);
	setProperty(overlayAnchor_, std::string());
}

void Panel::setTooltip(std::string text) { tooltip_ = std::move(text); }

void Panel::setAccessibleRole(std::string role) { role_ = std::move(role); }

void Panel::setAccessibleLabel(std::string label) { accessibleLabel_ = std::move(label); }

void Panel::setAccessibleDescription(std::string description) { description_ = std::move(description); }

/* Appearance */

void Panel::setColor(Color color) { setLook(color_, color); }

void Panel::setCornerRadii(CornerRadii radii) { setLook(radii_, radii); }

void Panel::setBorder(float width, Color color) {
	setLook(borderWidth_, width);
	setLook(borderColor_, color);
}

void Panel::setShadow(const Shadow& shadow) { setLook(shadow_, shadow); }

void Panel::setCursor(Cursor cursor) {
	cursor_ = cursor;
	if(window_) window_->updateCursor();
}

void Panel::setVisible(bool visible) { setProperty(visible_, visible); }

/* Layout */

// Found by id, so the other panel may be anywhere in the tree
std::optional<float> Panel::targetEdge(Edge edge) const {
	const auto& target = targets_[index(edge)];
	if(!target || !window_) return std::nullopt;
	// Found again once panels came or went, or the one found has another id now (only one that is found is kept)
	if(target->found != window_->treeGeneration_ || !target->other || target->other->id_ != target->id) {
		target->other = window_->root().findById(target->id);
		target->found = window_->treeGeneration_;
	}
	const Panel* other = target->other;
	if(!other) return std::nullopt;
	if(!other->isSettled(window_->layoutPass_)) window_->forwardAnchor_ = true; // a later panel: lay out again
	return edgeOf(other->frame_, target->edge) + target->offset;
}

// Parent anchors are measured from the parent's (scrolled) frame; bottom/right ones from its unclipped size. Every
// panel asks this for its four edges on every pass, so an anchor to another panel is told apart by a bit
std::optional<float> Panel::anchoredEdge(Edge edge, const Rect& origin, Vec2 parentSize) const {
	const auto& anchor = anchors_[index(edge)];
	if(!anchor) return std::nullopt;
	if((targetEdges_ & edgeBit(edge)) != 0) return targetEdge(edge);

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

// An edge anchored wins; without one the position places it; a container's size wins over the far anchor
std::pair<float, float> Panel::alongAxis(AnchoredEdges edges, float position, float size, bool arranged) noexcept {
	const float start = edges.start ? *edges.start : edges.end ? *edges.end - size : position;
	return {start, edges.end && (!edges.start || !arranged) ? *edges.end : start + size};
}

// An axis stretched by its anchors is set too: the other one follows its stretched size
void Panel::followAspectRatio(AnchoredEdges horizontal, AnchoredEdges vertical, Vec2 parentSize) noexcept {
	const bool stretchedX = horizontal.start && horizontal.end && !arrangedWidth_;
	const bool stretchedY = vertical.start && vertical.end && !arrangedHeight_;
	if(stretchedX) size_.x = std::max(*horizontal.end - *horizontal.start, 0.0f);
	if(stretchedY) size_.y = std::max(*vertical.end - *vertical.start, 0.0f);
	size_ = withAspectRatio(size_, arrangedWidth_ || stretchedX || width_.value != 0.0f,
	                        arrangedHeight_ || stretchedY || height_.value != 0.0f, parentSize);
}

// A panel never laid out is dirty, so laidOut_ is read only once it holds a pass's inputs
bool Panel::hasSameInputs(const Rect& origin, const Rect& clip, Vec2 parentSize, bool parentShown) const noexcept {
	if(layoutDirty_ || anchorsBelow_ || !window_ || window_->layoutAll_) return false;
	return laidOut_.origin == origin && laidOut_.clip == clip && laidOut_.parentSize == parentSize &&
	       laidOut_.parentShown == parentShown && laidOut_.arrangedWidth == arrangedWidth_ &&
	       laidOut_.arrangedHeight == arrangedHeight_;
}

// Laid out in this pass, or inside a subtree the pass skipped: its frame won't change again in the pass
bool Panel::isSettled(std::uint64_t pass) const noexcept {
	if(layoutPass_ == pass) return true;
	for(const Panel* panel = this; panel; panel = panel->parent_)
		if(panel->skippedPass_ == pass) return true;
	return false;
}

void Panel::layout(const Rect& origin, const Rect& clip, Vec2 parentSize, bool parentShown) {
	// A subtree whose inputs are the same and where nothing changed keeps the frames it has
	const bool sameInputs = hasSameInputs(origin, clip, parentSize, parentShown);
	if(sameInputs && !layoutBelow_) {
		skippedPass_ = window_->layoutPass_;
		return;
	}
	// Only panels below the children changed: the children measure as they did (a measured size is kept until the
	// panel itself changes), so they stay where arrange() put them
	const bool arranged =
	    sameInputs && std::ranges::none_of(children_, [](const auto& child) { return child->layoutDirty_; });
	laidOut_ = {origin, clip, parentSize, arrangedWidth_, arrangedHeight_, parentShown};

	prepareLayout();
	if(window_) layoutPass_ = window_->layoutPass_;
	placeFrame(origin, clip, parentSize, parentShown);
	if(!arranged) arrange();
	layoutChildren();
	layoutDirty_ = false;
}

void Panel::placeFrame(const Rect& origin, const Rect& clip, Vec2 parentSize, bool parentShown) {
	size_ = {arrangedWidth_ ? *arrangedWidth_ : bounded(width_, minWidth_, maxWidth_, parentSize.x),
	         arrangedHeight_ ? *arrangedHeight_ : bounded(height_, minHeight_, maxHeight_, parentSize.y)};

	const auto top = anchoredEdge(Edge::Top, origin, parentSize);
	const auto bottom = anchoredEdge(Edge::Bottom, origin, parentSize);
	const auto left = anchoredEdge(Edge::Left, origin, parentSize);
	const auto right = anchoredEdge(Edge::Right, origin, parentSize);
	if(aspectRatio_ > 0.0f) followAspectRatio({left, right}, {top, bottom}, parentSize);

	Rect rect;
	std::tie(rect.top, rect.bottom) =
	    alongAxis({top, bottom}, origin.top + y_.resolve(parentSize.y), size_.y, arrangedHeight_.has_value());
	std::tie(rect.left, rect.right) =
	    alongAxis({left, right}, origin.left + x_.resolve(parentSize.x), size_.x, arrangedWidth_.has_value());

	// A panel anchored on opposite edges is stretched: its real size (what children measure against) comes from them
	size_ = {std::max(rect.width(), 0.0f), std::max(rect.height(), 0.0f)};

	frame_ = rect;

	// Clip to the parent; a transformed panel to the parent's clip brought into its space (exact when the transform
	// keeps rectangles, else that area's bounding box, which the renderer narrows with a stencil)
	const Rect within = transform_.isIdentity() ? clip : clipInSpace(clip);
	rect.top = std::max(rect.top, within.top);
	rect.bottom = std::min(rect.bottom, within.bottom);
	rect.left = std::max(rect.left, within.left);
	rect.right = std::min(rect.right, within.right);

	bounds_ = rect;
	shown_ = parentShown && visible_ && rect.bottom >= rect.top && rect.right >= rect.left;
}

// What the subtree has is gathered from the children as they are laid out. A change made during the pass to a panel
// already laid out stays marked, for the next pass
void Panel::layoutChildren() {
	const auto pass = [this] {
		const Rect origin{frame_.left - scroll_.x, frame_.top - scroll_.y, frame_.right - scroll_.x,
		                  frame_.bottom - scroll_.y};
		bool below = false, anchors = targetEdges_ != 0, overlays = false;
		for(auto& child : children_) {
			if(!child->overlay_) child->layout(origin, bounds_, size_, shown_); // overlays: after the whole tree
			below = below || child->layoutDirty_ || child->layoutBelow_;
			anchors = anchors || child->anchorsBelow_;
			overlays = overlays || child->overlay_ || child->overlaysBelow_;
		}
		layoutBelow_ = below;
		anchorsBelow_ = anchors;
		overlaysBelow_ = overlays;
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

// A rotated panel's bounds are a bounding box: the parent already checked the point is inside its own bounds
Panel* Panel::hitTest(Vec2 point) noexcept {
	if(!shown_) return nullptr;
	if(!transform_.isIdentity()) {
		const std::optional<Affine2> inverse = localTransform().inverse();
		if(!inverse) return nullptr; // flattened: never hit
		point = inverse->apply(point);
	}
	if(!bounds_.contains(point)) return nullptr;

	for(const auto& child : std::views::reverse(children_)) {
		if(child->overlay_) continue; // hit-tested by the window, above everything
		if(Panel* hit = child->hitTest(point)) return hit;
	}

	return this;
}

Rect Panel::clipInSpace(const Rect& clip) const noexcept {
	const std::optional<Affine2> inverse = localTransform().inverse();
	return inverse ? inverse->boundingBox(clip) : frame_; // flattened: it draws nothing anyway
}

void Panel::setTransform(const Affine2& transform, Vec2 origin) {
	setProperty(transform_, transform);
	setProperty(transformOrigin_, origin);
}

Affine2 Panel::localTransform() const noexcept {
	if(transform_.isIdentity()) return {};
	return transform_.about({frame_.left + transformOrigin_.x * size_.x, frame_.top + transformOrigin_.y * size_.y});
}

Affine2 Panel::worldTransform() const noexcept {
	Affine2 world = localTransform();
	for(const Panel* panel = this; !panel->overlay_ && panel->parent_;) {
		panel = panel->parent_;
		if(!panel->transform_.isIdentity()) world = panel->localTransform() * world;
	}
	return world;
}

std::optional<Vec2> Panel::toLocal(Vec2 point) const noexcept {
	const Affine2 world = worldTransform();
	if(world.isIdentity()) return point;
	const std::optional<Affine2> inverse = world.inverse();
	return inverse ? std::optional<Vec2>{inverse->apply(point)} : std::nullopt;
}

Rect Panel::screenBounds() const noexcept {
	const Affine2 world = worldTransform();
	return world.isIdentity() ? bounds_ : world.boundingBox(bounds_);
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
