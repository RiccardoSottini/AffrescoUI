#include "affresco/render/Panel.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/render/Window.hpp"

#include "paint/RetainedGeometry.hpp"
#include "panel/PanelEdges.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace affresco {

using panel::edgeBit;
using panel::index;

namespace {

constexpr float scrollBarWidth = 4.0f; ///< Thickness of the scroll bar, in pixels.

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
	// A container sized by its children changes size with them: measured again, and placed again by its own parent
	if(!window_ || !window_->layingOut_)
		for(Panel* ancestor = parent_; ancestor && ancestor->fitsChildren(); ancestor = ancestor->parent_) {
			ancestor->measured_.reset();
			ancestor->layoutDirty_ = true;
		}
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

void Panel::setJustifySelf(std::optional<CrossAlign> align) { setProperty(justifySelf_, align); }

void Panel::setGridCell(GridCell cell) { setProperty(gridCell_, cell); }

// Every panel below may move, so the whole window is laid out again
void Panel::setWritingDirection(WritingDirection direction) {
	if(direction == writingDirection_) return;
	writingDirection_ = direction;
	invalidate();
	if(window_) window_->invalidate();
}

Align Panel::readingAlign(Align align) const noexcept {
	if(!rightToLeft_ || align == Align::Center) return align;
	return align == Align::Start ? Align::End : Align::Start;
}

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

void Panel::setOverlay(std::string anchorId, Placement placement, bool flip) {
	setProperty(overlay_, true);
	setProperty(overlayAnchor_, std::move(anchorId));
	setProperty(placement_, placement);
	setProperty(flip_, flip);
}

void Panel::clearOverlay() {
	setProperty(overlay_, false);
	setProperty(overlayAnchor_, std::string());
	setProperty(flip_, false);
	flipped_ = false;
}

/* Snapshots */

void Panel::setSnapshot(float scale, SnapshotHandler onImage) {
	const bool moved = !snapshot_; // out of the flow: the parent arranges its children again
	snapshot_ = std::make_unique<Snapshot>(Snapshot{scale > 0.0f && scale <= 4.0f ? scale : 1.0f, std::move(onImage)});
	if(moved) invalidate();
	else invalidatePaint(); // drawn again, at the new scale or to the new handler
}

void Panel::clearSnapshot() {
	if(!snapshot_) return;
	snapshot_.reset();
	invalidate();
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

void Panel::setGradient(std::shared_ptr<const Gradient> gradient) {
	const bool same = gradient_ == gradient || (gradient_ && gradient && *gradient_ == *gradient);
	if(same) return;
	gradient_ = std::move(gradient);
	invalidatePaint();
}

void Panel::setCursor(Cursor cursor) {
	cursor_ = cursor;
	if(window_) window_->updateCursor();
}

void Panel::setVisible(bool visible) { setProperty(visible_, visible); }

// A rotated panel's bounds are a bounding box: the parent already checked the point is inside its own bounds
Panel* Panel::hitTest(Vec2 point) noexcept {
	if(!shown_) return nullptr;
	if(isTransformed()) {
		const std::optional<Affine2> inverse = localTransform().inverse();
		if(!inverse) return nullptr; // flattened: never hit
		point = inverse->apply(point);
	}
	const bool inside = bounds_.contains(point);
	if(!inside && !(overflows() && childClip_.contains(point))) return nullptr;

	for(const auto& child : std::views::reverse(children_)) {
		if(child->isOverlay()) continue; // hit-tested by the window, above everything
		if(Panel* hit = child->hitTest(point)) return hit;
	}

	return inside ? this : nullptr;
}

Rect Panel::clipInSpace(const Rect& clip) const noexcept {
	const std::optional<Affine2> inverse = localTransform().inverse();
	return inverse ? inverse->boundingBox(clip) : frame_; // flattened: it draws nothing anyway
}

void Panel::setTransform(const Affine2& transform, Vec2 origin) {
	setProperty(transform_, transform);
	setProperty(transformOrigin_, origin);
}

void Panel::setParentTransform(const Affine2& transform) { setProperty(parentTransform_, transform); }

Affine2 Panel::localTransform() const noexcept {
	if(transform_.isIdentity()) return parentTransform_;
	const Affine2 own =
	    transform_.about({frame_.left + transformOrigin_.x * size_.x, frame_.top + transformOrigin_.y * size_.y});
	return parentTransform_.isIdentity() ? own : parentTransform_ * own;
}

Affine2 Panel::worldTransform() const noexcept {
	Affine2 world = localTransform();
	for(const Panel* panel = this; !panel->isOverlay() && panel->parent_;) {
		panel = panel->parent_;
		if(panel->isTransformed()) world = panel->localTransform() * world;
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
	if(gradient_) painter.fillGradient(frame_, radii_, *gradient_);
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
