/// @file
/// @brief Panel's layout: measuring, anchors, the frame and clip of a pass, and the children and their content.

#include "affresco/render/Panel.hpp"
#include "affresco/render/Window.hpp"

#include "panel/PanelEdges.hpp"

#include <algorithm>
#include <span>

namespace affresco {

using panel::edgeBit;
using panel::index;

namespace {

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

/// @brief How far a scrollable panel's content reaches, as if unscrolled, relative to its frame.
struct ContentExtent {
	Vec2 min; ///< Its top-left corner: 0 or less.
	Vec2 max; ///< Its bottom-right corner: 0 or more.
};

/// @brief The extent of the shown children that are not overlays.
/// @param children The children.
/// @param frame The panel's frame.
/// @param scroll The panel's scroll offset.
/// @return The extent, which always holds the origin.
ContentExtent contentExtent(std::span<const std::unique_ptr<Panel>> children, const Rect& frame, Vec2 scroll) noexcept {
	ContentExtent extent;
	for(const auto& child : children) {
		if(child->isOverlay() || !child->isVisible()) continue;
		const Rect& f = child->frame();
		extent.min = {std::min(extent.min.x, f.left + scroll.x - frame.left),
		              std::min(extent.min.y, f.top + scroll.y - frame.top)};
		extent.max = {std::max(extent.max.x, f.right + scroll.x - frame.left),
		              std::max(extent.max.y, f.bottom + scroll.y - frame.top)};
	}
	return extent;
}

} // namespace

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

	rightToLeft_ = writingDirection_ == WritingDirection::RightToLeft ||
	               (writingDirection_ == WritingDirection::Locale && window_ && window_->localeRightToLeft()) ||
	               (writingDirection_ == WritingDirection::Inherit && parent_ && parent_->rightToLeft_);
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
	// Right to left: placed as left to right, then mirrored in the parent; content wider than it reaches left, where
	// scrolling from 0 goes. Edges anchored to another panel are already where it was placed
	if(parent_ && parent_->rightToLeft_ && !isOverlay() &&
	   (targetEdges_ & (edgeBit(Edge::Left) | edgeBit(Edge::Right))) == 0) {
		const float width = rect.width();
		rect.left = 2.0f * origin.left + parentSize.x - rect.right;
		rect.right = rect.left + width;
	}

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
			if(!child->isOverlay()) child->layout(origin, bounds_, size_, shown_); // overlays: after the whole tree
			below = below || child->layoutDirty_ || child->layoutBelow_;
			anchors = anchors || child->anchorsBelow_;
			overlays = overlays || child->isOverlay() || child->overlaysBelow_;
		}
		layoutBelow_ = below;
		anchorsBelow_ = anchors;
		overlaysBelow_ = overlays;
	};
	pass();
	if(!isScrollable()) return;

	// The content's extent, as if unscrolled; an offset the content no longer allows is pulled back
	const ContentExtent extent = contentExtent(children_, frame_, scroll_);
	contentMin_ = extent.min;
	contentMax_ = extent.max;
	if(scrollBy({0.0f, 0.0f})) pass();
}

} // namespace affresco
