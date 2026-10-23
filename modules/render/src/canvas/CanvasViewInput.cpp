/// @file
/// @brief CanvasView: the pointer and the keys. Panning, zooming at the pointer, selecting, the handles, snapping
/// and guides dragged out of the rulers.

#include "canvas/CanvasViewInternals.hpp"

#include "affresco/render/Window.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <ranges>

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

/// @brief Whether a press pans rather than selects: the middle button, or the left one while Space is held.
/// @param event The press.
/// @param space Whether Space is held.
/// @return true if it pans.
[[nodiscard]] bool pans(const MouseButtonEvent& event, bool space) noexcept {
	return event.button == MouseButton::Middle || (event.button == MouseButton::Left && space);
}

/// @brief Which ruler a point is on.
enum class RulerHit {
	none,   ///< Neither.
	top,    ///< The top one: it drags out a horizontal guide.
	left,   ///< The left one: a vertical guide.
	corner, ///< Where they meet: nothing.
};

/// @brief Tells which ruler a point is on; the rulers are never thicker than the view.
/// @param box The view's frame.
/// @param position The point.
/// @return The ruler.
[[nodiscard]] RulerHit rulerHit(const Rect& box, Vec2 position) noexcept {
	const float thickness = std::min({RulerStyle{}.thickness, box.width(), box.height()});
	const bool onTop = position.y < box.top + thickness, onLeft = position.x < box.left + thickness;
	if(onTop && onLeft) return RulerHit::corner;
	return onTop ? RulerHit::top : onLeft ? RulerHit::left : RulerHit::none;
}

/// @brief A selection with an id toggled: taken out if it is in, added at the end if not.
/// @param ids The selection.
/// @param id The id.
/// @return The new selection.
[[nodiscard]] std::vector<std::string> toggled(std::vector<std::string> ids, const std::string& id) {
	if(const auto found = std::ranges::find(ids, id); found != ids.end()) ids.erase(found);
	else ids.push_back(id);
	return ids;
}

/// @brief A guide found under the pointer.
struct GuideHit {
	bool vertical = false; ///< Whether it is a vertical one.
	int index = -1;        ///< Its index along its axis.
};

/// @brief Finds the first guide near a world point: the vertical ones first, each axis in order.
/// @param guides The guides.
/// @param world The point, in world units.
/// @param reach How near counts, in world units.
/// @return The guide, or nothing.
[[nodiscard]] std::optional<GuideHit> guideAt(const CanvasGuides& guides, Vec2 world, float reach) noexcept {
	for(const bool vertical : {true, false}) {
		const std::vector<float>& axis = vertical ? guides.vertical : guides.horizontal;
		const float at = vertical ? world.x : world.y;
		for(std::size_t i = 0; i < axis.size(); ++i)
			if(std::abs(axis[i] - at) <= reach) return GuideHit{vertical, static_cast<int>(i)};
	}
	return std::nullopt;
}

/// @brief Moves the guide being dragged, adding it on its first move (if its axis has room).
/// @param axis The guides along its axis.
/// @param index Its index, or -1 before it is added.
/// @param at Where it goes, in world units.
/// @return Its index, still -1 if there was no room.
[[nodiscard]] int dragGuide(std::vector<float>& axis, int index, float at) {
	if(index < 0 && axis.size() < CanvasView::maximumGuides) {
		axis.push_back(at);
		return static_cast<int>(axis.size() - 1);
	}
	if(index >= 0 && static_cast<std::size_t>(index) < axis.size())
		axis[static_cast<std::size_t>(index)] = at; // (a peer may have taken guides away meanwhile)
	return index;
}

/// @brief How far the rotation handle has turned a box: the pointer's angle around its middle, from where it was
/// pressed.
/// @param box The box, on screen.
/// @param pressed Where the handle was pressed.
/// @param pointer Where the pointer is.
/// @return The angle, in degrees clockwise.
[[nodiscard]] float rotationOf(const Rect& box, Vec2 pressed, Vec2 pointer) noexcept {
	const Vec2 middle{(box.left + box.right) / 2.0f, (box.top + box.bottom) / 2.0f};
	return angleOf(middle, pointer) - angleOf(middle, pressed);
}

/// @brief A box with some of its edges moved.
/// @param from The box, in world units.
/// @param delta How far the edges move.
/// @param edges The edges that move: all of them for a move.
/// @param resize Whether it is a resize, which never turns the box inside out: the moving edge stops a world unit
/// short of the other.
/// @return The box.
[[nodiscard]] Rect draggedRect(const Rect& from, Vec2 delta, const SnapEdges& edges, bool resize) noexcept {
	Rect rect = from;
	if(edges.left) rect.left += delta.x;
	if(edges.right) rect.right += delta.x;
	if(edges.top) rect.top += delta.y;
	if(edges.bottom) rect.bottom += delta.y;
	if(!resize) return rect;
	if(edges.left) rect.left = std::min(rect.left, rect.right - 1.0f);
	if(edges.right) rect.right = std::max(rect.right, rect.left + 1.0f);
	if(edges.top) rect.top = std::min(rect.top, rect.bottom - 1.0f);
	if(edges.bottom) rect.bottom = std::max(rect.bottom, rect.top + 1.0f);
	return rect;
}

/// @brief Whether a marquee is small enough to be a click.
/// @param marquee The marquee, on screen.
/// @return true if it is.
[[nodiscard]] bool isClick(const Rect& marquee) noexcept {
	return marquee.width() < canvas::clickPixels && marquee.height() < canvas::clickPixels;
}

/// @brief Carries the items a gesture moved from the box they started in to the one it ended in.
/// @param items The items, as they were pressed.
/// @param from The box they started in.
/// @param to The box it ended in.
/// @param rotation How far it turned them, in degrees.
/// @return false if the gesture changed nothing (and the items are left as they were).
bool carry(std::vector<CanvasTransform>& items, const Rect& from, const Rect& to, float rotation) {
	if(from == to && rotation == 0.0f) return false;
	for(CanvasTransform& item : items) {
		item.bounds = canvas::carried(item.bounds, from, to);
		item.rotate = std::fmod(item.rotate + std::round(rotation), 360.0f);
	}
	return true;
}

/// @brief Whether a key is one the zoom modifier (Control, or Command on a Mac) is held after.
/// @param event The key event.
/// @param down Whether it is a press or a repeat.
/// @return Whether the modifier is held after it.
[[nodiscard]] bool zoomKeyAfter(const KeyEvent& event, bool down) noexcept {
	switch(event.key) {
	case Key::LeftControl:
	case Key::RightControl:
	case Key::LeftSuper:
	case Key::RightSuper:
		return down;
	default:
		return event.mods.control() || event.mods.super();
	}
}

/// @brief Where a key zooms at: the pointer if it is on the view, else the view's middle.
/// @param box The view's frame.
/// @param pointer The pointer.
/// @return The point.
[[nodiscard]] Vec2 keyAnchor(const Rect& box, Vec2 pointer) noexcept {
	return box.contains(pointer) ? pointer : Vec2{(box.left + box.right) / 2.0f, (box.top + box.bottom) / 2.0f};
}

/// @brief What a zoom key multiplies the zoom by.
/// @param key The key.
/// @param zoom The zoom now.
/// @return The factor, or nothing if it isn't one: + and - step, 0 goes back to 100%.
[[nodiscard]] std::optional<float> keyZoom(Key key, float zoom) noexcept {
	switch(key) {
	case Key::Equal:
	case Key::KeypadAdd:
		return canvas::stepZoom;
	case Key::Minus:
	case Key::KeypadSubtract:
		return 1.0f / canvas::stepZoom;
	case Key::Num0:
		return 1.0f / zoom;
	default:
		return std::nullopt;
	}
}

/// @brief How far an arrow key nudges the selection.
/// @param key The key.
/// @param step How far, in world units.
/// @return The move, or nothing if it isn't an arrow.
[[nodiscard]] std::optional<Vec2> arrowDelta(Key key, float step) noexcept {
	switch(key) {
	case Key::Left:
		return Vec2{-step, 0.0f};
	case Key::Right:
		return Vec2{step, 0.0f};
	case Key::Up:
		return Vec2{0.0f, -step};
	case Key::Down:
		return Vec2{0.0f, step};
	default:
		return std::nullopt;
	}
}

/// @brief What a key press does.
enum class KeyAction {
	none,     ///< Nothing.
	zoom,     ///< Zooms by KeyCommand::factor.
	fit,      ///< Fits the content, or the selection if KeyCommand::selected.
	deselect, ///< Clears the selection.
	nudge,    ///< Moves the selection by KeyCommand::delta.
};

/// @brief A key press, decided.
struct KeyCommand {
	KeyAction action = KeyAction::none; ///< What it does.
	float factor = 1.0f;                ///< For zoom.
	bool selected = false;              ///< For fit.
	Vec2 delta{};                       ///< For nudge.
};

/// @brief Decides what a key press does: + - 0 zoom, Shift+1 and Shift+2 fit, Escape deselects, the arrows nudge
/// (ten units with Shift).
/// @param event The press.
/// @param zoom The zoom now.
/// @return The command.
[[nodiscard]] KeyCommand keyCommand(const KeyEvent& event, float zoom) noexcept {
	if(const std::optional<float> factor = keyZoom(event.key, zoom)) return {KeyAction::zoom, *factor};
	if(const std::optional<Vec2> delta = arrowDelta(event.key, event.mods.shift() ? 10.0f : 1.0f))
		return {KeyAction::nudge, 1.0f, false, *delta};
	if(event.key == Key::Escape) return {KeyAction::deselect};
	if(event.mods.shift() && (event.key == Key::Num1 || event.key == Key::Num2))
		return {KeyAction::fit, 1.0f, event.key == Key::Num2};
	return {};
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
	if(pans(event, space_)) {
		gesture_ = Gesture::pan;
		return;
	}
	if(event.button == MouseButton::Left) begin(event.position, event.mods.shift());
}

void CanvasView::begin(Vec2 position, bool shift) {
	additive_ = shift;
	if(const RulerHit ruler = rulers_ ? rulerHit(frame(), position) : RulerHit::none; ruler != RulerHit::none) {
		if(ruler == RulerHit::corner) return;
		gesture_ = Gesture::guide;
		guideVertical_ = ruler == RulerHit::left;
		guideIndex_ = -1;
		return;
	}
	handle_ = handleAt(position);
	if(handle_ >= 0) {
		grabSelection();
		return;
	}
	if(const std::string id = itemAt(position); !id.empty()) {
		if(shift || !isSelected(id)) {
			setSelection(toggled(shift ? selection_ : std::vector<std::string>{}, id));
			onViewChanged();
		}
		if(isSelected(id)) grabSelection();
		return;
	}
	// A guide under the pointer is dragged; anything else starts a marquee
	if(const std::optional<GuideHit> guide = guideAt(guides_, worldAt(position), canvas::guidePixels / camera_.zoom)) {
		gesture_ = Gesture::guide;
		guideVertical_ = guide->vertical;
		guideIndex_ = guide->index;
		return;
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
	case Gesture::rotate:
		rotation_ = rotationOf(onScreen(from_), pressedAt_, event.position);
		invalidatePaint();
		return;
	case Gesture::guide: {
		const Vec2 world = worldAt(event.position);
		guideIndex_ = dragGuide(guideVertical_ ? guides_.vertical : guides_.horizontal, guideIndex_,
		                        std::round(guideVertical_ ? world.x : world.y));
		invalidatePaint();
		if(guideIndex_ >= 0) onViewChanged();
		return;
	}
	}
}

void CanvasView::dragItems(Vec2 position) {
	const Vec2 delta{(position.x - pressedAt_.x) / camera_.zoom, (position.y - pressedAt_.y) / camera_.zoom};
	const SnapEdges edges = gesture_ == Gesture::move ? SnapEdges{} : edgesOf(handle_);
	const Rect rect = draggedRect(from_, delta, edges, gesture_ == Gesture::resize);
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
	switch(gesture) {
	case Gesture::none:
	case Gesture::pan: // reported as it moved
		return;
	case Gesture::marquee: {
		const Rect marquee = canvas::spanning(pressedAt_, pointer_);
		const bool click = isClick(marquee);
		const std::vector<std::string> before = selection_;
		if(click && !additive_) setSelection({});
		else if(!click)
			selectIn(canvas::spanning(worldAt({marquee.left, marquee.top}), worldAt({marquee.right, marquee.bottom})),
			         additive_);
		if(selection_ != before) onViewChanged();
		return;
	}
	case Gesture::guide:
		finishGuide();
		return;
	case Gesture::move:
	case Gesture::resize:
	case Gesture::rotate:
		if(carry(internals_->pressed, from_, to_, rotation_)) onTransform(internals_->pressed);
		return;
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
	if(away && static_cast<std::size_t>(guideIndex_) < axis.size()) axis.erase(axis.begin() + guideIndex_);
	guideIndex_ = -1;
	onViewChanged();
}

std::string CanvasView::itemAt(Vec2 position) const {
	const auto children = this->children();
	for(const auto& child : std::views::reverse(children)) {
		const Panel& panel = *child;
		const Rect bounds = scaleChildren_ ? onScreen(worldBoundsOf(panel)) : panel.frame();
		if(!bounds.contains(position) || panel.resolvedSize().x <= 0.0f) continue;
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
	const bool rotatable = internals_->rects.size() == 1 && (scaleChildren_ || modelItem(selection_.front()) != nullptr);
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
	zoomKey_ = zoomKeyAfter(event, down);
	if(event.key == Key::Space) space_ = down && focusWithin();
	if(!down || gesture_ != Gesture::none || !focusWithin()) return;
	const KeyCommand command = keyCommand(event, camera_.zoom);
	switch(command.action) {
	case KeyAction::none:
		return;
	case KeyAction::zoom:
		zoomBy(keyAnchor(frame(), window()->pointer()), command.factor);
		return;
	case KeyAction::fit:
		fit(command.selected);
		return;
	case KeyAction::deselect:
		if(selection_.empty()) return;
		setSelection({});
		onViewChanged();
		return;
	case KeyAction::nudge:
		nudge(command.delta.x, command.delta.y);
		return;
	}
}

} // namespace affresco
