/// @file
/// @brief Window's input: routing the platform's typed events to the focused, hovered or grabbed panel, hover and
/// its tooltip, and the cursor. Tests and automation call the same functions.

#include "affresco/render/Window.hpp"
#include "affresco/render/Label.hpp"

#include "paint/GeometryCache.hpp"
#include "paint/RetainedGeometry.hpp"
#include "detail/Utf8.hpp"
#include "platform/Platform.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

namespace {

constexpr double tooltipDelayMs = 600.0;   ///< How long the pointer rests before a tooltip shows.
constexpr float scrollSpeed = 24.0f;       ///< Pixels per wheel step.
constexpr std::size_t maxDropPaths = 4096; ///< The most paths a drop delivers.
constexpr std::size_t maxDropPath = 32768; ///< The longest path a drop delivers, in bytes.

/// @brief Whether a dropped path can be delivered: the screens put paths in JSON, which needs valid UTF-8, and a NUL
/// would cut the path short in the C functions that open it.
/// @param path The path, as the platform gave it.
/// @return true if it is not empty, not too long, has no NUL and is valid UTF-8.
[[nodiscard]] bool deliverable(std::string_view path) noexcept {
	if(path.empty() || path.size() > maxDropPath) return false;
	for(std::string_view rest = path; !rest.empty();) {
		const detail::Decoded decoded = detail::decodeUtf8(rest);
		if(decoded.codepoint == 0 || (decoded.codepoint == 0xFFFD && !rest.starts_with("\xEF\xBF\xBD"))) return false;
		rest.remove_prefix(decoded.length);
	}
	return true;
}

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
	keyTaken_ = false; // a new press or a release: what a binding took before is over
	capsLock_ = event.mods.capsLock();
	if(focused_ && focused_->isShown()) focused_->keyEvent(event);
	onKey(event);

	if(--routing_ == 0) runDeferred();
}

void Window::routeChar(const CharEvent& event) {
	// A surrogate or a codepoint past U+10FFFF is no character: no keyboard types one, and UTF-8 can't hold it
	const char32_t codepoint = event.codepoint;
	if((codepoint >= 0xD800U && codepoint <= 0xDFFFU) || codepoint > 0x10FFFFU) return;
	if(std::exchange(keyTaken_, false)) return; // the character of a key a binding took
	++routing_;
	if(focused_ && focused_->isShown()) focused_->charEvent(event);
	onChar(event);

	if(--routing_ == 0) runDeferred();
}

void Window::takeKey() noexcept { keyTaken_ = true; }

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
	// A NaN passes every clamp and an infinity can turn into one in a view's arithmetic: either leaves a scroll NaN
	const Vec2 offset = event.offset, position = event.position;
	if(!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(position.x) || !std::isfinite(position.y))
		return;
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
	if(event.paths.size() > maxDropPaths || !std::ranges::all_of(event.paths, deliverable)) {
		DropEvent kept{{}, event.position};
		for(const std::string& path : event.paths)
			if(kept.paths.size() < maxDropPaths && deliverable(path)) kept.paths.push_back(path);
		if(!kept.paths.empty()) routeDrop(kept); // every path of kept is deliverable, so this goes on below
		return;
	}
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

// NOLINTNEXTLINE(performance-unnecessary-value-param): the header's signature, kept as it is
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
	std::erase(snapshots_, &panel);
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
	for(Panel* p : std::views::reverse(after)) // entered: outermost first
		if(std::ranges::find(before, p) == before.end()) p->hoverEvent(true);

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
