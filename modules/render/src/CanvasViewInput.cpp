/// @file
/// @brief CanvasView: the pointer and the keys. Panning, zooming at the pointer, selecting, the handles, snapping
/// and guides dragged out of the rulers.

#include "CanvasViewInternals.hpp"

#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace affresco {
namespace {

constexpr std::size_t maximumTargets = 1024; ///< Items a drag snaps to, at most (the nearest ones are queried).
constexpr float targetPixels = 400.0f;       ///< How far around a dragged rectangle, in screen units, it looks.

/// @brief The angle of a point around a centre, in degrees clockwise from the right.
/// @param centre The centre.
/// @param point The point.
[[nodiscard]] float angleOf(Vec2 centre, Vec2 point) noexcept {
	return std::atan2(point.y - centre.y, point.x - centre.x) * 180.0f / std::numbers::pi_v<float>;
}

/// @brief The edges a resize handle moves.
/// @param handle 0 to 7 clockwise from the top-left.
[[nodiscard]] constexpr SnapEdges edgesOf(int handle) noexcept {
	return {handle == 0 || handle == 6 || handle == 7, handle <= 2, handle >= 2 && handle <= 4,
	        handle >= 4 && handle <= 6};
}

} // namespace

void CanvasView::listen() {
	if(window() == listening_) return;
	stopListening();
	listening_ = window();
	if(!listening_) return;
	connections_ = {listening_->onMouseButton.connect([this](const MouseButtonEvent& event) { pressed(event); }),
	                listening_->onMouseMove.connect([this](const MouseMoveEvent& event) { moved(event); }),
	                listening_->onKey.connect([this](const KeyEvent& event) { keyed(event); })};
}

void CanvasView::stopListening() noexcept {
	if(!listening_) return;
	listening_->onMouseButton.disconnect(connections_[0]);
	listening_->onMouseMove.disconnect(connections_[1]);
	listening_->onKey.disconnect(connections_[2]);
	listening_ = nullptr;
}

bool CanvasView::focusWithin() const noexcept {
	const Panel* focused = window() ? window()->focusedPanel() : nullptr;
	return focused && (focused == this || focused->parent() == this);
}

void CanvasView::pressed(const MouseButtonEvent& event) {
	if(event.action == Action::Release) {
		if(gesture_ != Gesture::none) finish();
		return;
	}
	if(gesture_ != Gesture::none || !window() || !isShown()) return;
	const Panel* hit = window()->panelAt(event.position);
	while(hit && hit != this) hit = hit->parent();
	if(!hit) return;
	pressedAt_ = pointer_ = event.position;
	pressedCamera_ = camera_;
	if(event.button == MouseButton::Middle || (event.button == MouseButton::Left && space_)) {
		gesture_ = Gesture::pan;
		return;
	}
	if(event.button == MouseButton::Left) begin(event.position, event.mods.shift());
}

void CanvasView::begin(Vec2 position, bool shift) {
	additive_ = shift;
	const Rect& box = frame();
	const float thickness = std::min({RulerStyle{}.thickness, box.width(), box.height()});
	const bool onTop = position.y < box.top + thickness, onLeft = position.x < box.left + thickness;
	if(rulers_ && (onTop || onLeft)) {
		if(onTop && onLeft) return;
		gesture_ = Gesture::guide;
		guideVertical_ = onLeft;
		guideIndex_ = -1;
		return;
	}
	handle_ = handleAt(position);
	if(handle_ >= 0) {
		grabSelection();
		return;
	}
	if(std::string id = itemAt(position); !id.empty()) {
		if(shift || !isSelected(id)) {
			std::vector<std::string> ids = shift ? selection_ : std::vector<std::string>{};
			if(const auto found = std::ranges::find(ids, id); found != ids.end()) ids.erase(found);
			else ids.push_back(id);
			setSelection(std::move(ids));
			onViewChanged();
		}
		if(isSelected(id)) grabSelection();
		return;
	}
	// A guide under the pointer is dragged; anything else starts a marquee
	const float reach = canvas::guidePixels / camera_.zoom;
	const Vec2 world = worldAt(position);
	for(const bool vertical : {true, false}) {
		const std::vector<float>& axis = vertical ? guides_.vertical : guides_.horizontal;
		for(std::size_t i = 0; i < axis.size(); ++i)
			if(std::abs(axis[i] - (vertical ? world.x : world.y)) <= reach) {
				gesture_ = Gesture::guide;
				guideVertical_ = vertical;
				guideIndex_ = static_cast<int>(i);
				return;
			}
	}
	gesture_ = Gesture::marquee;
}

void CanvasView::grabSelection() {
	selectedItems(internals_->pressed);
	const std::optional<Rect> group = boundsOf(internals_->pressed);
	if(!group) return;
	from_ = to_ = *group;
	rotation_ = 0.0f;
	gesture_ = handle_ == 8 ? Gesture::rotate : handle_ >= 0 ? Gesture::resize : Gesture::move;
}

void CanvasView::moved(const MouseMoveEvent& event) {
	pointer_ = event.position;
	switch(gesture_) {
	case Gesture::none:
		return;
	case Gesture::pan: { // reported as it goes, so that a state bound to the camera follows the drag
		const Camera2D panned =
		    pressedCamera_.pannedBy({event.position.x - pressedAt_.x, event.position.y - pressedAt_.y});
		if(panned == camera_) return;
		camera_ = panned;
		invalidate();
		onViewChanged();
		return;
	}
	case Gesture::marquee:
		invalidatePaint();
		return;
	case Gesture::move:
	case Gesture::resize:
		dragItems(event.position);
		invalidate();
		return;
	case Gesture::rotate: {
		const Rect box = onScreen(from_);
		const Vec2 middle{(box.left + box.right) / 2.0f, (box.top + box.bottom) / 2.0f};
		rotation_ = angleOf(middle, event.position) - angleOf(middle, pressedAt_);
		invalidatePaint();
		return;
	}
	case Gesture::guide: {
		std::vector<float>& axis = guideVertical_ ? guides_.vertical : guides_.horizontal;
		const Vec2 world = worldAt(event.position);
		const float at = std::round(guideVertical_ ? world.x : world.y);
		if(guideIndex_ < 0 && axis.size() < maximumGuides) {
			guideIndex_ = static_cast<int>(axis.size());
			axis.push_back(at);
		} else if(guideIndex_ >= 0) {
			axis[static_cast<std::size_t>(guideIndex_)] = at;
		}
		invalidatePaint();
		if(guideIndex_ >= 0) onViewChanged();
		return;
	}
	}
}

void CanvasView::dragItems(Vec2 position) {
	const Vec2 delta{(position.x - pressedAt_.x) / camera_.zoom, (position.y - pressedAt_.y) / camera_.zoom};
	const SnapEdges edges = gesture_ == Gesture::move ? SnapEdges{} : edgesOf(handle_);
	Rect rect = from_;
	if(edges.left) rect.left += delta.x;
	if(edges.right) rect.right += delta.x;
	if(edges.top) rect.top += delta.y;
	if(edges.bottom) rect.bottom += delta.y;
	// A resize never turns the box inside out: the moving edge stops a world unit short of the other
	if(gesture_ == Gesture::resize) {
		if(edges.left) rect.left = std::min(rect.left, rect.right - 1.0f);
		if(edges.right) rect.right = std::max(rect.right, rect.left + 1.0f);
		if(edges.top) rect.top = std::min(rect.top, rect.bottom - 1.0f);
		if(edges.bottom) rect.bottom = std::max(rect.bottom, rect.top + 1.0f);
	}
	internals_->lines = {};
	to_ = rect;
	if(!snap_) return;
	// Targets: the unselected items near the box (the nearest maximumTargets), and the unselected children
	auto& targets = internals_->rects;
	targets.clear();
	const float margin = targetPixels / camera_.zoom;
	internals_->index.query({rect.left - margin, rect.top - margin, rect.right + margin, rect.bottom + margin},
	                        internals_->found);
	for(const std::uint32_t index : internals_->found) {
		if(targets.size() >= maximumTargets) break;
		if(!isSelected(model_->item(index).id)) targets.push_back(internals_->bounds[index]);
	}
	for(const auto& child : children())
		if(targets.size() < maximumTargets && !isSelected(child->id()) && child->resolvedSize().x > 0.0f)
			targets.push_back(worldBoundsOf(*child));
	const SnapTargets snapTargets{grid_, guides_.vertical, guides_.horizontal, targets};
	const float tolerance = canvas::snapPixels / camera_.zoom;
	const SnapResult result = gesture_ == Gesture::move ? snapMove(rect, snapTargets, tolerance)
	                                                    : snapResize(rect, edges, snapTargets, tolerance);
	to_ = result.rect;
	internals_->lines = result.lines;
}

void CanvasView::finish() {
	const Gesture gesture = std::exchange(gesture_, Gesture::none);
	internals_->lines = {};
	invalidate();
	if(gesture == Gesture::pan) {
		// reported as it moved
	} else if(gesture == Gesture::marquee) {
		const Rect marquee = canvas::spanning(pressedAt_, pointer_);
		const bool click = marquee.width() < canvas::clickPixels && marquee.height() < canvas::clickPixels;
		const Rect area =
		    canvas::spanning(worldAt({marquee.left, marquee.top}), worldAt({marquee.right, marquee.bottom}));
		const std::vector<std::string> before = selection_;
		if(click && !additive_) setSelection({});
		else if(!click) selectIn(area, additive_);
		if(selection_ != before) onViewChanged();
	} else if(gesture == Gesture::guide) {
		finishGuide();
	} else if(gesture == Gesture::move || gesture == Gesture::resize || gesture == Gesture::rotate) {
		if(from_ == to_ && rotation_ == 0.0f) return;
		auto& items = internals_->pressed;
		for(CanvasTransform& item : items) {
			item.bounds = canvas::carried(item.bounds, from_, to_);
			item.rotate = std::fmod(item.rotate + std::round(rotation_), 360.0f);
		}
		onTransform(items);
	}
}

void CanvasView::finishGuide() {
	if(guideIndex_ < 0) return;
	std::vector<float>& axis = guideVertical_ ? guides_.vertical : guides_.horizontal;
	// Dropped back on its ruler (or outside the view): taken away
	const Rect& box = frame();
	const float thickness = std::min({RulerStyle{}.thickness, box.width(), box.height()});
	const bool away = guideVertical_ ? pointer_.x < box.left + thickness || pointer_.x > box.right
	                                 : pointer_.y < box.top + thickness || pointer_.y > box.bottom;
	if(away) axis.erase(axis.begin() + guideIndex_);
	guideIndex_ = -1;
	onViewChanged();
}

std::string CanvasView::itemAt(Vec2 position) const {
	const auto children = this->children();
	for(auto child = children.rbegin(); child != children.rend(); ++child) {
		const Panel& panel = **child;
		if(!panel.frame().contains(position) || panel.resolvedSize().x <= 0.0f) continue;
		const auto& standIns = internals_->standIns;
		const auto standing = std::ranges::find(standIns, &panel, &std::pair<const Panel*, const Panel*>::first);
		const std::string& id = standing != standIns.end() ? standing->second->id() : panel.id();
		if(!id.empty()) return id;
	}
	const Vec2 world = worldAt(position);
	internals_->index.query({world.x, world.y, world.x, world.y}, internals_->found);
	if(internals_->found.empty() || !model_) return {};
	return model_->item(internals_->found.back()).id;
}

int CanvasView::handleAt(Vec2 position) const noexcept {
	if(selection_.empty()) return -1;
	selectedRects(internals_->rects);
	if(internals_->rects.empty()) return -1;
	Rect group = internals_->rects.front();
	for(const Rect& rect : internals_->rects) group = canvas::united(group, rect);
	const Rect box = onScreen(group);
	const bool rotatable = internals_->rects.size() == 1 && modelItem(selection_.front()) != nullptr;
	const float reach = handleSize / 2.0f + 2.0f;
	for(int handle = rotatable ? 8 : 7; handle >= 0; --handle) {
		const Vec2 at = canvas::handlePoint(box, handle);
		if(std::abs(at.x - position.x) <= reach && std::abs(at.y - position.y) <= reach) return handle;
	}
	return -1;
}

void CanvasView::selectIn(const Rect& area, bool add) {
	std::vector<std::string> ids = add ? selection_ : std::vector<std::string>{};
	for(const auto& child : children())
		if(!child->id().empty() && child->resolvedSize().x > 0.0f && canvas::meet(worldBoundsOf(*child), area))
			ids.push_back(child->id());
	internals_->index.query(area, internals_->found);
	for(const std::uint32_t index : internals_->found) {
		if(ids.size() >= maximumSelection) break;
		ids.push_back(model_->item(index).id);
	}
	setSelection(std::move(ids));
}

void CanvasView::zoomBy(Vec2 position, float factor) {
	const Camera2D zoomed =
	    camera_.zoomedAt({position.x - frame().left, position.y - frame().top}, camera_.zoom * factor);
	if(zoomed == camera_) return;
	setCamera(zoomed);
	onViewChanged();
}

void CanvasView::nudge(float dx, float dy) {
	auto& items = internals_->pressed;
	selectedItems(items);
	if(items.empty()) return;
	for(CanvasTransform& item : items)
		item.bounds = {item.bounds.left + dx, item.bounds.top + dy, item.bounds.right + dx, item.bounds.bottom + dy};
	onTransform(items);
}

bool CanvasView::scrollContent(Vec2 delta) {
	if(zoomKey_ && window()) {
		zoomBy(window()->pointer(), std::pow(1.0025f, -delta.y));
		return true;
	}
	setCamera(camera_.pannedBy({-delta.x, -delta.y}));
	onViewChanged();
	return true;
}

void CanvasView::keyed(const KeyEvent& event) {
	const bool down = event.action != Action::Release;
	if(event.key == Key::LeftControl || event.key == Key::RightControl || event.key == Key::LeftSuper ||
	   event.key == Key::RightSuper)
		zoomKey_ = down;
	else zoomKey_ = event.mods.control() || event.mods.super();
	if(event.key == Key::Space) space_ = down && focusWithin();
	if(!down || gesture_ != Gesture::none || !focusWithin()) return;
	const Vec2 pointer = window()->pointer();
	const Vec2 at = frame().contains(pointer)
	                    ? pointer
	                    : Vec2{(frame().left + frame().right) / 2.0f, (frame().top + frame().bottom) / 2.0f};
	const float step = event.mods.shift() ? 10.0f : 1.0f;
	switch(event.key) {
	case Key::Equal:
	case Key::KeypadAdd:
		zoomBy(at, canvas::stepZoom);
		break;
	case Key::Minus:
	case Key::KeypadSubtract:
		zoomBy(at, 1.0f / canvas::stepZoom);
		break;
	case Key::Num0:
		zoomBy(at, 1.0f / camera_.zoom);
		break;
	case Key::Num1:
		if(event.mods.shift()) fit(false);
		break;
	case Key::Num2:
		if(event.mods.shift()) fit(true);
		break;
	case Key::Escape:
		if(selection_.empty()) break;
		setSelection({});
		onViewChanged();
		break;
	case Key::Left:
		nudge(-step, 0.0f);
		break;
	case Key::Right:
		nudge(step, 0.0f);
		break;
	case Key::Up:
		nudge(0.0f, -step);
		break;
	case Key::Down:
		nudge(0.0f, step);
		break;
	default:
		break;
	}
}

} // namespace affresco
