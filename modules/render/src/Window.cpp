/// @file
/// @brief Window: input routing, focus, hover, capture, timers, deferred tasks, layout and tooltips. Nothing here
/// calls the windowing system or OpenGL: the OS window is a platform::NativeWindow and drawing a detail::Renderer,
/// and a headless window has neither.

#include "affresco/render/Window.hpp"
#include "affresco/render/Label.hpp"

#include "paint/GeometryCache.hpp"
#include "paint/Renderer.hpp"
#include "paint/RetainedGeometry.hpp"
#include "detail/Ppm.hpp"
#include "platform/Accessibility.hpp"
#include "platform/Platform.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
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

platform::NativeWindow& platform::NativeWindow::of(Window& window) { return *window.native_; }

void platform::NativeWindow::resized(Window& window, int width, int height) { window.resized(width, height); }

void platform::NativeWindow::refresh(Window& window) { window.refresh(); }

void platform::NativeWindow::framebufferResized(Window& window) { window.requestFrame(); }

void platform::NativeWindow::displayChanged(Window& window, float scale) {
	window.routeDisplay({scale, window.colorScheme(), window.reducedMotion()});
}

void platform::NativeWindow::dropped(Window& window, const DropEvent& event) { window.routeDrop(event); }

void platform::NativeWindow::chosen(Window& window, std::function<void()> run) { window.runChosen(std::move(run)); }

Window::Window(std::unique_ptr<platform::NativeWindow> native, std::unique_ptr<detail::Renderer> renderer,
               const Clock& clock, std::string title, int width, int height)
    : native_(std::move(native)), renderer_(std::move(renderer)), clock_(&clock), title_(std::move(title)),
      width_(width), height_(height) {
	root_.attachTo(this);
	if(native_) {
		scale_ = native_->contentScale();
		native_->connect(*this);
		accessibility_ = platform::createAccessibilityAdapter(*this);
	}
}

Window::~Window() {
	accessibility_.reset(); // before the panels it reads go
	// GL objects owned by the renderer are deleted in this window's context
	if(native_) native_->makeCurrent();
	renderer_.reset();
}

GLFWwindow* Window::nativeHandle() const noexcept {
	return native_ ? static_cast<GLFWwindow*>(native_->handle()) : nullptr;
}

/* Properties */

void Window::setTitle(std::string title) {
	title_ = std::move(title);
	if(native_) native_->setTitle(title_);
}

void Window::setSize(int width, int height) {
	width_ = std::max(width, minWidth_);
	height_ = std::max(height, minHeight_);
	if(native_) native_->setSize(width_, height_); // the OS then reports the size: resized()
	else resized(width_, height_);                 // headless: as the OS would
	requestLayout();
}

void Window::setMinSize(int minWidth, int minHeight) {
	minWidth_ = minWidth;
	minHeight_ = minHeight;
	if(native_) native_->setMinSize(minWidth_, minHeight_);

	if(width_ < minWidth_ || height_ < minHeight_) setSize(width_, height_);
}

bool Window::isVisible() const { return native_ ? native_->isVisible() : visible_; }

void Window::setVisible(bool visible) {
	if(native_) native_->setVisible(visible);
	else visible_ = visible;
}

bool Window::shouldClose() const { return native_ && native_->shouldClose(); }

/* Desktop */

bool Window::setMenuBar(std::vector<MenuItem> menus) {
	menus_ = std::move(menus);
	nativeMenuBar_ = native_ && native_->setMenuBar(menus_);
	return nativeMenuBar_;
}

std::vector<std::string> Window::chooseFiles(const FileDialog& dialog) {
	if(fileDialogHandler_) return fileDialogHandler_(dialog);
	return native_ ? native_->chooseFiles(dialog) : std::vector<std::string>{};
}

void Window::setFileDialogHandler(FileDialogHandler handler) { fileDialogHandler_ = std::move(handler); }

bool Window::fileDialogs() const { return fileDialogHandler_ || (native_ && native_->fileDialogs()); }

void Window::resized(int width, int height) {
	width_ = width;
	height_ = height;
	requestLayout(); // the root's size changes, and with it what depends on it
	damageAll_ = true;
	onResize(ResizeEvent{width, height});
}

void Window::refresh() {
	redrawDirty_ = true;
	render();
}

/* Content */

void Window::setFocus(Panel* panel) {
	if(panel == focused_) return;

	Panel* previous = std::exchange(focused_, panel);
	focusVisible_ = keyInput_;
	if(focusRing_) requestLayout();
	if(previous) previous->focusEvent(false);
	if(focused_) focused_->focusEvent(true);
	if(accessibility_) accessibility_->focusChanged();
}

void Window::setFocusRing(std::optional<FocusRing> ring) {
	focusRing_ = ring;
	requestLayout();
}

std::string Window::clipboardText() const { return native_ ? native_->clipboardText() : clipboard_; }

void Window::setClipboardText(const std::string& utf8) {
	if(native_) native_->setClipboardText(utf8);
	else clipboard_ = utf8;
}

void Window::setDefaultFont(std::shared_ptr<const Font> font) {
	defaultFont_ = std::move(font);
	invalidate();
}

Panel* Window::panelAt(Vec2 point) {
	updateLayout();
	for(auto it = overlays_.rbegin(); it != overlays_.rend(); ++it) // above everything, the latest on top
		if(Panel* hit = (*it)->hitTest(point)) return hit;
	return root_.hitTest(point);
}

/* Rendering */

void Window::updateLayout() {
	if(!layoutDirty_) return;

	const auto width = static_cast<float>(width_);
	const auto height = static_cast<float>(height_);

	// The root always fills the window
	root_.width_ = px(width);
	root_.height_ = px(height);
	layingOut_ = true;
	const Rect area{0.0f, 0.0f, width, height};

	// A panel anchored to a later one (not placed yet) is placed again in a second pass, from the first pass's frames
	forwardAnchor_ = false;
	for(int pass = 0; pass < 2 && (pass == 0 || forwardAnchor_); ++pass) {
		++layoutPass_;
		forwardAnchor_ = false;
		root_.layout(area, area, Vec2{width, height}, true);
		overlays_.clear();
		layoutOverlays(root_);
	}
	layoutAll_ = false;

	if(tooltipShown_ && tooltip_) {
		const Vec2 size{tooltip_->measuredWidth(width), tooltip_->measuredHeight(height)};
		const float left = std::clamp(cursor_.x + 12.0f, 0.0f, std::max(width - size.x - 4.0f, 0.0f));
		float top = cursor_.y + 20.0f;
		if(top + size.y > height) top = cursor_.y - size.y - 8.0f;
		tooltip_->layout(Rect{left, top, left + size.x, top + size.y}, area, Vec2{width, height}, true);
	}
	layoutFocusRing();
	layoutInspection();
	layingOut_ = false;

	layoutDirty_ = false;
}

void Window::layoutFocusRing() {
	drawn_ = overlays_;
	if(!focusRing_ || !focusVisible() || !focused_->isShown()) return;
	if(!ring_) {
		ring_ = std::make_unique<Panel>();
		ring_->attachTo(this);
		ring_->setColor(colors::transparent);
	}
	const FocusRing& ring = *focusRing_;
	const float out = ring.offset + ring.width;
	const auto around = [out](float radius) { return radius > 0.0f ? radius + out : 0.0f; };
	const CornerRadii& radii = focused_->cornerRadii();
	ring_->setBorder(ring.width, ring.color);
	ring_->setCornerRadii(
	    {around(radii.topLeft), around(radii.topRight), around(radii.bottomRight), around(radii.bottomLeft)});

	const Rect& f = focused_->frame_;
	const Rect box{f.left - out, f.top - out, f.right + out, f.bottom + out};
	ring_->width_ = px(box.right - box.left);
	ring_->height_ = px(box.bottom - box.top);
	const auto width = static_cast<float>(width_), height = static_cast<float>(height_);
	ring_->layout(box, Rect{0.0f, 0.0f, width, height}, Vec2{width, height}, true);
	drawn_.push_back(ring_.get());
}

void Window::layoutInspection() {
	if(!inspection_ || !inspection_->isVisible()) return;
	const auto width = static_cast<float>(width_), height = static_cast<float>(height_);
	const Rect area{0.0f, 0.0f, width, height};
	inspection_->width_ = px(width);
	inspection_->height_ = px(height);
	inspection_->layout(area, area, Vec2{width, height}, true);
	drawn_.push_back(inspection_.get());
}

void Window::setInspection(std::unique_ptr<Panel> layer) {
	inspection_ = std::move(layer); // the one before forgets itself
	if(inspection_) inspection_->attachTo(this);
	requestLayout();
}

// Pre-order, after the tree: an overlay's own overlays come after it, so they are drawn on top of it
void Window::layoutOverlays(Panel& panel) {
	if(!panel.overlaysBelow_) return; // known from the panel's last layout, which a skipped subtree keeps
	for(auto& child : panel.children_) {
		if(child->overlay_) {
			layoutOverlay(*child, panel);
			overlays_.push_back(child.get());
		}
		layoutOverlays(*child);
	}
}

void Window::layoutOverlay(Panel& overlay, const Panel& parent) {
	const auto width = static_cast<float>(width_), height = static_cast<float>(height_);
	const Rect area{0.0f, 0.0f, width, height};
	const Panel* anchor = overlay.overlayAnchor_.empty() ? &parent : root_.findById(overlay.overlayAnchor_);
	if(!anchor) anchor = &parent;
	const Rect& a = anchor->frame_;
	const Vec2 size{overlay.measuredWidth(width), overlay.measuredHeight(height)};

	Vec2 at;
	switch(overlay.placement_) {
	case Placement::Below:
		at = {a.left, a.bottom};
		break;
	case Placement::BelowEnd:
		at = {a.right - size.x, a.bottom};
		break;
	case Placement::Above:
		at = {a.left, a.top - size.y};
		break;
	case Placement::AboveEnd:
		at = {a.right - size.x, a.top - size.y};
		break;
	case Placement::Right:
		at = {a.right, a.top};
		break;
	case Placement::Left:
		at = {a.left - size.x, a.top};
		break;
	case Placement::Center:
		at = {(width - size.x) / 2.0f, (height - size.y) / 2.0f};
		break;
	case Placement::Window:
		break;
	}

	// Kept inside the window; Window placement measures everything from the window itself
	Rect origin = area;
	if(overlay.placement_ != Placement::Window) {
		at.x = std::clamp(at.x, 0.0f, std::max(width - size.x, 0.0f));
		at.y = std::clamp(at.y, 0.0f, std::max(height - size.y, 0.0f));
		origin = {std::round(at.x), std::round(at.y), std::round(at.x) + width, std::round(at.y) + height};
	}
	overlay.layout(origin, area, Vec2{width, height}, parent.shown_);
}

detail::Frame Window::frame() noexcept {
	return {
	    root_,   drawn_,    tooltipShown_ ? tooltip_.get() : nullptr, width_, height_, layoutPass_, paintGeneration_,
	    damage_, damageAll_};
}

void Window::draw() {
	native_->makeCurrent();
	updateLayout();
	// On HiDPI screens the framebuffer has more pixels than the window has units
	renderer_->draw(frame(), native_->framebufferSize());
	clearDamage();
}

void Window::defer(std::function<void()> task) {
	deferred_.push_back(std::move(task));
	if(routing_ == 0) requestFrame(); // make sure a frame comes, which runs it
}

void Window::runDeferred() {
	// Tasks may defer more tasks: keep going until there are none
	while(!deferred_.empty()) {
		std::vector<std::function<void()>> tasks;
		tasks.swap(deferred_);
		for(auto& task : tasks) task();
	}
}

std::uint64_t Window::setTimer(double delayMs, std::function<void()> task) {
	const std::uint64_t id = nextTimerId_++;
	timers_.push_back({id, clock_->seconds() + std::max(delayMs, 0.0) / 1000.0, std::move(task)});
	if(native_) native_->wake(); // the event loop recomputes how long it may sleep
	return id;
}

void Window::cancelTimer(std::uint64_t id) {
	std::erase_if(timers_, [id](const Timer& timer) { return timer.id == id; });
	if(dueTimers_)
		for(Timer& timer : *dueTimers_)
			if(timer.id == id) timer.task = nullptr;
}

std::optional<double> Window::secondsToNextTimer() const {
	if(timers_.empty()) return std::nullopt;
	double due = timers_.front().due;
	for(const Timer& timer : timers_) due = std::min(due, timer.due);
	return std::max(due - clock_->seconds(), 0.0);
}

// Each due timer runs once, earliest first; tasks may set new timers, which wait for the next call. The due timers are
// taken out and sorted once (a document can set thousands), and a task can still cancel the ones after it:
// cancelTimer() reaches them through dueTimers_.
void Window::runTimers() {
	const double now = clock_->seconds();
	const auto firstDue = std::ranges::partition(timers_, [now](const Timer& t) { return t.due > now; }).begin();
	if(firstDue == timers_.end()) return;
	std::vector<Timer> due(std::make_move_iterator(firstDue), std::make_move_iterator(timers_.end()));
	timers_.erase(firstDue, timers_.end());
	// Timers due at the same time run in the order they were set
	std::ranges::sort(due, [](const Timer& a, const Timer& b) { return a.due != b.due ? a.due < b.due : a.id < b.id; });

	// Points at this call's list while it runs, even if a task throws (a task may also render() again)
	class Running {
	public:
		/// @brief Points @p current at @p list.
		/// @param current dueTimers_.
		/// @param list This call's due timers.
		Running(std::vector<Timer>*& current, std::vector<Timer>* list)
		    : current_(current), outer_(std::exchange(current, list)) {}
		/// @brief Restores dueTimers_.
		~Running() { current_ = outer_; }
		Running(const Running&) = delete;
		Running& operator=(const Running&) = delete;
		Running(Running&&) = delete;
		Running& operator=(Running&&) = delete;

	private:
		std::vector<Timer>*& current_; ///< dueTimers_.
		std::vector<Timer>* outer_;    ///< What it was before.
	};
	const Running running(dueTimers_, &due);
	for(Timer& timer : due)
		if(const auto task = std::exchange(timer.task, nullptr)) task(); // empty: cancelled by an earlier task
}

void Window::render() {
	runTimers();
	runDeferred();
	if(accessibility_) accessibility_->poll();
	if(!redrawDirty_) return;

	drawAndPresent();
	redrawDirty_ = false;
	if(accessibility_) accessibility_->frameDrawn();
}

void Window::drawAndPresent() {
	const auto start = std::chrono::steady_clock::now(); // real time, whatever clock() says
	if(renderer_) {
		draw();
		native_->present();
	} else {
		updateLayout(); // headless: the frame is laid out, never drawn
	}
	++statistics_.frames;
	statistics_.frameMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	statistics_.drawCalls = renderer_ ? renderer_->drawCalls() : 0;
}

bool Window::saveScreenshot(const std::filesystem::path& path) {
	if(!renderer_) return false;
	runDeferred(); // pending UI updates belong in the picture
	draw();        // into the back buffer, without presenting it

	const platform::PixelSize size = native_->framebufferSize();
	return detail::writePpm(path, size.width, size.height, renderer_->readBack(size));
}

bool Window::saveScreenshot(const std::filesystem::path& path, int scale) {
	if(!renderer_) return false;
	runDeferred();
	native_->makeCurrent(); // the image is created in it
	updateLayout();

	const platform::PixelSize size{width_ * scale, height_ * scale};
	const std::vector<std::uint8_t> pixels = renderer_->drawOffscreen(frame(), size);
	clearDamage();
	return detail::writePpm(path, size.width, size.height, pixels);
}

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
			if(panel->scrollContent({-event.offset.x * scrollSpeed, -event.offset.y * scrollSpeed}))
				break;
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
	++treeGeneration_; // an anchor may have found it
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
