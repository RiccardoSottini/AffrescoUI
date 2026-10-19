#include "affresco/render/Window.hpp"
#include "affresco/render/Label.hpp"

#include "detail/Ppm.hpp"
#include "gl/Context.hpp"
#include "gl/QuadRenderer.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <array>
#include <stdexcept>
#include <utility>

namespace affresco {

namespace {

constexpr double tooltipDelayMs = 600.0; ///< How long the pointer rests before a tooltip shows.
constexpr float scrollSpeed = 24.0f;     ///< Pixels per wheel step.

} // namespace

void Window::HandleDeleter::operator()(GLFWwindow* handle) const noexcept { glfwDestroyWindow(handle); }

Window::Window(std::string title, int width, int height) : title_(std::move(title)), width_(width), height_(height) {
	handle_.reset(glfwCreateWindow(width, height, title_.c_str(), nullptr, nullptr));
	if(!handle_) throw std::runtime_error("AffrescoUI: could not create a window with an OpenGL 4.1 core context");

	glfwMakeContextCurrent(handle_.get());
	if(!gl::loadFunctions(reinterpret_cast<gl::ProcAddressLoader>(glfwGetProcAddress)))
		throw std::runtime_error("AffrescoUI: could not load the OpenGL functions");
	glfwSwapInterval(1); // vsync: never render faster than the display

	renderer_ = std::make_unique<gl::QuadRenderer>();

	root_.attachTo(this);
	installCallbacks();
}

Window::~Window() {
	for(GLFWcursor* cursor : cursors_)
		if(cursor) glfwDestroyCursor(cursor);

	// GL objects owned by the renderer are deleted in this window's context
	if(handle_) glfwMakeContextCurrent(handle_.get());
}

/* Properties */

void Window::setTitle(std::string title) {
	title_ = std::move(title);
	glfwSetWindowTitle(handle_.get(), title_.c_str());
}

void Window::setSize(int width, int height) {
	width_ = std::max(width, minWidth_);
	height_ = std::max(height, minHeight_);
	glfwSetWindowSize(handle_.get(), width_, height_);
	invalidate();
}

void Window::setMinSize(int minWidth, int minHeight) {
	minWidth_ = minWidth;
	minHeight_ = minHeight;
	glfwSetWindowSizeLimits(handle_.get(), minWidth_, minHeight_, GLFW_DONT_CARE, GLFW_DONT_CARE);

	if(width_ < minWidth_ || height_ < minHeight_) setSize(width_, height_);
}

bool Window::isVisible() const { return glfwGetWindowAttrib(handle_.get(), GLFW_VISIBLE) == GLFW_TRUE; }

void Window::setVisible(bool visible) {
	if(visible) glfwShowWindow(handle_.get());
	else glfwHideWindow(handle_.get());
}

bool Window::shouldClose() const { return glfwWindowShouldClose(handle_.get()) == GLFW_TRUE; }

/* Content */

void Window::setFocus(Panel* panel) {
	if(panel == focused_) return;

	Panel* previous = std::exchange(focused_, panel);
	if(previous) previous->focusEvent(false);
	if(focused_) focused_->focusEvent(true);
}

std::string Window::clipboardText() const {
	const char* text = glfwGetClipboardString(handle_.get());
	return text ? text : "";
}

void Window::setClipboardText(const std::string& utf8) { glfwSetClipboardString(handle_.get(), utf8.c_str()); }

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

	if(tooltipShown_ && tooltip_) {
		const Vec2 size{tooltip_->measuredWidth(width), tooltip_->measuredHeight(height)};
		const float left = std::clamp(cursor_.x + 12.0f, 0.0f, std::max(width - size.x - 4.0f, 0.0f));
		float top = cursor_.y + 20.0f;
		if(top + size.y > height) top = cursor_.y - size.y - 8.0f;
		tooltip_->layout(Rect{left, top, left + size.x, top + size.y}, area, Vec2{width, height}, true);
	}
	layingOut_ = false;

	layoutDirty_ = false;
}

// Pre-order, after the tree: an overlay's own overlays come after it, so they are drawn on top of it
void Window::layoutOverlays(Panel& panel) {
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

void Window::draw() {
	// On HiDPI screens the framebuffer has more pixels than the window has units
	int framebufferWidth = 0, framebufferHeight = 0;
	glfwGetFramebufferSize(handle_.get(), &framebufferWidth, &framebufferHeight);
	draw(framebufferWidth, framebufferHeight);
}

void Window::draw(int framebufferWidth, int framebufferHeight) {
	if(glfwGetCurrentContext() != handle_.get()) glfwMakeContextCurrent(handle_.get());
	gl::beginFrame(framebufferWidth, framebufferHeight);

	updateLayout();
	renderer_->draw(root_, overlays_, tooltipShown_ ? tooltip_.get() : nullptr, width_, height_);
}

void Window::defer(std::function<void()> task) {
	deferred_.push_back(std::move(task));
	if(routing_ == 0) invalidate(); // make sure a frame comes, which runs it
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
	timers_.push_back({id, glfwGetTime() + std::max(delayMs, 0.0) / 1000.0, std::move(task)});
	glfwPostEmptyEvent(); // the event loop recomputes how long it may sleep
	return id;
}

void Window::cancelTimer(std::uint64_t id) {
	std::erase_if(timers_, [id](const Timer& timer) { return timer.id == id; });
}

std::optional<double> Window::secondsToNextTimer() const {
	if(timers_.empty()) return std::nullopt;
	double due = timers_.front().due;
	for(const Timer& timer : timers_) due = std::min(due, timer.due);
	return std::max(due - glfwGetTime(), 0.0);
}

// Each due timer runs once, earliest first; tasks may set new timers, which wait for the next call. A due timer stays
// in timers_ until it runs, so a task can still cancel the ones after it.
void Window::runTimers() {
	const double now = glfwGetTime();
	const std::uint64_t setBefore = nextTimerId_; // the timers already set
	for(;;) {
		auto next = timers_.end();
		for(auto it = timers_.begin(); it != timers_.end(); ++it)
			if(it->due <= now && it->id < setBefore && (next == timers_.end() || it->due < next->due)) next = it;
		if(next == timers_.end()) return;
		Timer timer = std::move(*next); // the task stays alive while it runs, even if it cancels itself
		timers_.erase(next);
		timer.task();
	}
}

void Window::render() {
	runTimers();
	runDeferred();
	if(!redrawDirty_) return;

	draw();
	glfwSwapBuffers(handle_.get());
	redrawDirty_ = false;
}

bool Window::saveScreenshot(const std::filesystem::path& path) {
	runDeferred(); // pending UI updates belong in the picture
	draw();        // into the back buffer, without presenting it

	int width = 0, height = 0;
	glfwGetFramebufferSize(handle_.get(), &width, &height);

	return detail::writePpm(path, width, height, gl::readBackBuffer(width, height));
}

bool Window::saveScreenshot(const std::filesystem::path& path, int scale) {
	runDeferred();
	if(glfwGetCurrentContext() != handle_.get()) glfwMakeContextCurrent(handle_.get()); // the image is created in it

	const int width = width_ * scale, height = height_ * scale;
	const gl::OffscreenTarget target(width, height);
	draw(width, height);
	return detail::writePpm(path, width, height, target.pixels());
}

/* Input: GLFW callbacks are converted to typed events and routed immediately */

void Window::installCallbacks() {
	GLFWwindow* handle = handle_.get();
	glfwSetWindowUserPointer(handle, this);

	static constexpr auto self = [](GLFWwindow* window) -> Window& {
		return *static_cast<Window*>(glfwGetWindowUserPointer(window));
	};

	// Size changes only mark the window dirty; the main loop or the refresh callback draws once.
	// (During a live resize on macOS the loop is blocked and the refresh callback fires after every step.)
	glfwSetWindowSizeCallback(handle, [](GLFWwindow* window, int width, int height) {
		Window& w = self(window);
		w.width_ = width;
		w.height_ = height;
		w.invalidate();
		w.onResize(ResizeEvent{width, height});
	});

	glfwSetFramebufferSizeCallback(handle, [](GLFWwindow* window, int, int) { self(window).invalidate(); });

	glfwSetWindowRefreshCallback(handle, [](GLFWwindow* window) {
		Window& w = self(window);
		w.redrawDirty_ = true;
		w.render();
	});

	glfwSetKeyCallback(handle, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		self(window).routeKey({static_cast<Key>(key), scancode, static_cast<Action>(action), Mods{mods}});
	});

	glfwSetCharCallback(handle, [](GLFWwindow* window, unsigned int codepoint) {
		self(window).routeChar({static_cast<char32_t>(codepoint)});
	});

	glfwSetCursorPosCallback(handle, [](GLFWwindow* window, double x, double y) {
		self(window).routeMouseMove({static_cast<float>(x), static_cast<float>(y)});
	});

	glfwSetMouseButtonCallback(handle, [](GLFWwindow* window, int button, int action, int mods) {
		// Asked rather than taken from the last move: a click can come before any move event
		double x = 0.0, y = 0.0;
		glfwGetCursorPos(window, &x, &y);
		self(window).routeMouseButton({static_cast<MouseButton>(button),
		                               static_cast<Action>(action),
		                               Mods{mods},
		                               {static_cast<float>(x), static_cast<float>(y)}});
	});

	glfwSetScrollCallback(handle, [](GLFWwindow* window, double dx, double dy) {
		Window& w = self(window);
		w.routeScroll({{static_cast<float>(dx), static_cast<float>(dy)}, w.cursor_});
	});

	glfwSetCursorEnterCallback(
	    handle, [](GLFWwindow* window, int entered) { self(window).routeCursorEnter(entered == GLFW_TRUE); });
}

void Window::routeKey(const KeyEvent& event) {
	++routing_;
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
	if(Panel* target = grabbed_ ? grabbed_ : hovered_) target->mouseMoveEvent(event);
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
			invalidate();
		}
		if(event.button == MouseButton::Left) setFocus(hit);
		if(!grabbed_) grabbed_ = hit;
	}

	if(Panel* target = grabbed_ ? grabbed_ : hit) target->mouseButtonEvent(event);

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
		target->scrollEvent(event);
		for(Panel* panel = target; panel; panel = panel->parent_)
			if(panel->isScrollable() && panel->scrollBy({-event.offset.x * scrollSpeed, -event.offset.y * scrollSpeed}))
				break;
	}
	onScroll(event);

	if(--routing_ == 0) runDeferred();
}

void Window::routeCursorEnter(bool entered) {
	if(!entered) setHovered(nullptr);
	onCursorEnter(CursorEnterEvent{entered});
}

void Window::forget(const Panel& panel) noexcept {
	if(focused_ == &panel) focused_ = nullptr;
	if(hovered_ == &panel) hovered_ = nullptr;
	if(grabbed_ == &panel) grabbed_ = nullptr;
	std::erase(overlays_, &panel);
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
		invalidate();
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
		invalidate();
	});
}

void Window::updateCursor() {
	const Panel* owner = grabbed_ ? grabbed_ : hovered_;
	const Cursor shape = owner ? owner->cursor() : Cursor::Arrow;
	if(shape == currentCursor_) return;
	currentCursor_ = shape;

	if(shape == Cursor::Arrow) {
		glfwSetCursor(handle_.get(), nullptr); // the system default
		return;
	}

	static constexpr std::array<int, 5> standardShapes = {
	    GLFW_ARROW_CURSOR, GLFW_IBEAM_CURSOR, GLFW_POINTING_HAND_CURSOR, GLFW_RESIZE_EW_CURSOR, GLFW_RESIZE_NS_CURSOR};
	const auto index = static_cast<std::size_t>(shape);
	if(!cursors_[index]) cursors_[index] = glfwCreateStandardCursor(standardShapes[index]);
	glfwSetCursor(handle_.get(), cursors_[index]);
}

} // namespace affresco
