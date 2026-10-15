#include "sgl/render/Window.hpp"

#include "detail/Ppm.hpp"
#include "gl/Context.hpp"
#include "gl/QuadRenderer.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace sgl {

void Window::HandleDeleter::operator()(GLFWwindow* handle) const noexcept { glfwDestroyWindow(handle); }

Window::Window(std::string title, int width, int height) : title_(std::move(title)), width_(width), height_(height) {
	handle_.reset(glfwCreateWindow(width, height, title_.c_str(), nullptr, nullptr));
	if(!handle_) throw std::runtime_error("sGLGUI: could not create a window with an OpenGL 4.1 core context");

	glfwMakeContextCurrent(handle_.get());
	if(!gl::loadFunctions(reinterpret_cast<gl::ProcAddressLoader>(glfwGetProcAddress)))
		throw std::runtime_error("sGLGUI: could not load the OpenGL functions");
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

void Window::setDefaultFont(std::shared_ptr<const Font> font) {
	defaultFont_ = std::move(font);
	invalidate();
}

Panel* Window::panelAt(Vec2 point) {
	updateLayout();
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
	root_.layout(Rect{0.0f, 0.0f, width, height}, Vec2{width, height}, true);
	layingOut_ = false;

	layoutDirty_ = false;
}

void Window::draw() {
	if(glfwGetCurrentContext() != handle_.get()) glfwMakeContextCurrent(handle_.get());

	// On HiDPI screens the framebuffer has more pixels than the window has units
	int framebufferWidth = 0, framebufferHeight = 0;
	glfwGetFramebufferSize(handle_.get(), &framebufferWidth, &framebufferHeight);
	gl::beginFrame(framebufferWidth, framebufferHeight);

	updateLayout();
	renderer_->draw(root_, width_, height_);
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

void Window::render() {
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
	if(Panel* target = panelAt(event.position)) target->scrollEvent(event);
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
}

void Window::setHovered(Panel* panel) {
	if(panel == hovered_) return;

	Panel* previous = std::exchange(hovered_, panel);
	if(previous) previous->hoverEvent(false);
	if(hovered_) hovered_->hoverEvent(true);
	updateCursor();
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

} // namespace sgl
