/// @file
/// @brief Window's input: routing the platform's typed events to the focused, hovered or grabbed panel, hover and
/// its tooltip, and the cursor. Tests and automation call the same functions.

#include "affresco/render/Window.hpp"
#include "affresco/render/Label.hpp"

#include "GeometryCache.hpp"
#include "RetainedGeometry.hpp"
#include "platform/Platform.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace affresco {

namespace {

constexpr double tooltipDelayMs = 600.0; ///< How long the pointer rests before a tooltip shows.
constexpr float scrollSpeed = 24.0f;     ///< Pixels per wheel step.

/// @brief A pointer event as a panel hears it: its position in the panel's layout space (through its transforms).
/// @tparam Event An event with a position.
/// @param panel The panel.
/// @param event The event, in window units.
/// @return The event, moved; unchanged for a panel without transforms or one they flatten.
template <typename Event> Event inSpaceOf(const Panel& panel, Event event) noexcept {
	event.position = panel.toLocal(event.position).value_or(event.position);
	return event;
}

} // namespace

/* Input: the platform converts the OS's events to typed events and routes them immediately; tests and automation
   call the same functions */

void Window::routeKey(const KeyEvent& event) {
	++routing_;
	keyInput_ = true;
	if(focused_ && focused_->isShown()) focused_->keyEvent(event);
	onKey(event);

	if(--routing_ == 0) runDeferred();
}

void Window::routeChar(const CharEvent& event) {
	++routing_;
	if(focused_ && focused_->isShown()) focused_->charEvent(event);
	onChar(event);

	if(--routing_ == 0) runDeferred();
}

void Window::routeMouseMove(Vec2 position) {
	++routing_;
	cursor_ = position;
	setHovered(panelAt(position));

	const MouseMoveEvent event{position};
	if(Panel* target = grabbed_ ? grabbed_ : hovered_) target->mouseMoveEvent(inSpaceOf(*target, event));
	onMouseMove(event);

	if(--routing_ == 0) runDeferred();
}

void Window::routeMouseButton(const MouseButtonEvent& event) {
	++routing_;
	cursor_ = event.position;
	Panel* hit = panelAt(event.position);
	setHovered(hit);

	if(event.action == Action::Press) {
		if(tooltipShown_ || tooltipTimer_) { // a press dismisses the tooltip until the pointer moves to another panel
			cancelTimer(std::exchange(tooltipTimer_, 0));
			tooltipShown_ = false;
			requestLayout();
		}
		keyInput_ = false;
		if(event.button == MouseButton::Left) setFocus(hit);
		if(!grabbed_) grabbed_ = hit;
	}

	if(Panel* target = grabbed_ ? grabbed_ : hit) target->mouseButtonEvent(inSpaceOf(*target, event));

	if(event.action == Action::Release) {
		grabbed_ = nullptr;
		updateCursor(); // the drag may have ended over a panel with another cursor
	}

	onMouseButton(event);

	if(--routing_ == 0) runDeferred();
}

void Window::routeScroll(const ScrollEvent& event) {
	++routing_;
	// The panel under the pointer hears it; the nearest scrollable one (it or an ancestor) scrolls
	if(Panel* target = panelAt(event.position)) {
		target->scrollEvent(inSpaceOf(*target, event));
		for(Panel* panel = target; panel; panel = panel->parent_)
			if(panel->scrollContent({-event.offset.x * scrollSpeed, -event.offset.y * scrollSpeed})) break;
	}
	onScroll(event);

	if(--routing_ == 0) runDeferred();
}

void Window::routeCursorEnter(bool entered) {
	if(!entered) setHovered(nullptr);
	onCursorEnter(CursorEnterEvent{entered});
}

void Window::routeDrop(const DropEvent& event) {
	++routing_;
	cursor_ = event.position;
	setHovered(panelAt(event.position));
	onDrop(event);

	if(--routing_ == 0) runDeferred();
}

void Window::routeDisplay(const DisplayEvent& event) {
	if(event.scale == scale_ && event.colorScheme == colorScheme_ && event.reducedMotion == reducedMotion_) return;
	if(event.scale != scale_) {
		scale_ = event.scale;
		damageAll_ = true; // the glyphs and edges land on other pixels
		requestFrame();
	}
	colorScheme_ = event.colorScheme;
	reducedMotion_ = event.reducedMotion;
	onDisplay(event);
	runDeferred();
}

void Window::runChosen(std::function<void()> run) {
	if(!run) return;
	++routing_;
	run();

	if(--routing_ == 0) runDeferred();
}

void Window::forget(const Panel& panel) noexcept {
	++treeGeneration_;                                 // an anchor may have found it
	if(panel.geometry_ && panel.geometry_->recorded) { // where it drew is damage
		detail::addTo(damage_, detail::screenReachOf(*panel.geometry_));
		requestFrame();
	}
	if(focused_ == &panel) {
		focused_ = nullptr;
		if(focusRing_) requestLayout();
	}
	if(hovered_ == &panel) hovered_ = nullptr;
	if(grabbed_ == &panel) grabbed_ = nullptr;
	std::erase(overlays_, &panel);
	std::erase(drawn_, &panel);
}

// Like CSS :hover, a panel counts as hovered while the pointer is over it or over any of its descendants
void Window::setHovered(Panel* panel) {
	if(panel == hovered_) return;

	std::vector<Panel*> before, after;
	for(Panel* p = hovered_; p; p = p->parent_) before.push_back(p);
	for(Panel* p = panel; p; p = p->parent_) after.push_back(p);
	hovered_ = panel;

	for(Panel* p : before) // left: innermost first
		if(std::ranges::find(after, p) == after.end()) p->hoverEvent(false);
	for(auto it = after.rbegin(); it != after.rend(); ++it) // entered: outermost first
		if(std::ranges::find(before, *it) == before.end()) (*it)->hoverEvent(true);

	updateCursor();
	restartTooltip();
}

void Window::restartTooltip() {
	cancelTimer(std::exchange(tooltipTimer_, 0));
	if(tooltipShown_) {
		tooltipShown_ = false;
		requestLayout();
	}

	const Panel* owner = hovered_;
	while(owner && owner->tooltip().empty()) owner = owner->parent_;
	if(!owner || grabbed_) return;

	tooltipTimer_ = setTimer(tooltipDelayMs, [this, text = owner->tooltip()] {
		tooltipTimer_ = 0;
		if(!tooltip_) {
			tooltip_ = std::make_unique<Label>();
			tooltip_->attachTo(this);
			tooltip_->setColor(Color::hex(0x111827));
			tooltip_->setTextColor(colors::white);
			tooltip_->setPadding(6.0f);
			tooltip_->setFitToText(true, 320.0f);
			tooltip_->setWrap(true);
		}
		tooltip_->setText(text);
		tooltipShown_ = true;
		requestLayout(); // placed at the pointer by the next pass
	});
}

void Window::updateCursor() {
	const Panel* owner = grabbed_ ? grabbed_ : hovered_;
	const Cursor shape = owner ? owner->cursor() : Cursor::Arrow;
	if(shape == currentCursor_) return;
	currentCursor_ = shape;
	if(native_) native_->setCursor(shape);
}

} // namespace affresco
