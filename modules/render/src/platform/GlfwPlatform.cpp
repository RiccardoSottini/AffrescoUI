/// @file
/// @brief The GLFW platform: windows with an OpenGL 4.1 core or OpenGL ES 3.0 context (AFFRESCOUI_GL_BACKEND), their
/// input, cursors, clipboard, dropped files and display scale, and the event loop's waiting. What GLFW doesn't do (the
/// colour scheme, native menu bars, file dialogs) comes from platform/Desktop.hpp; besides those files, the only one of
/// the render module that calls GLFW.

#include "platform/Desktop.hpp"
#include "platform/Platform.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Window.hpp"

#include <GLFW/glfw3.h>
#ifdef __EMSCRIPTEN__
#include <GLFW/emscripten_glfw3.h>
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace affresco::platform {

namespace {

#ifdef AFFRESCOUI_GLES3
constexpr const char* contextName = "OpenGL ES 3.0 context"; ///< What windows ask for, for errors.
#else
constexpr const char* contextName = "OpenGL 4.1 core context"; ///< What windows ask for, for errors.
#endif

/// @brief Asks GLFW for the context of this build's backend in the windows created next.
void hintContext() {
#ifdef AFFRESCOUI_GLES3
	// OpenGL ES 3.0 through EGL: ANGLE (macOS, Windows), Mesa (Linux)
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
	// OpenGL 4.1 core: the highest version available on macOS
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
}

/// @brief Tells GLFW, before it starts, which of the system's graphics APIs ANGLE draws with: Metal on macOS,
/// Direct3D 11 on Windows. Other EGLs (Mesa's) ignore it, and the gl41 backend doesn't use EGL.
void hintAngle() {
#if defined(AFFRESCOUI_GLES3) && defined(__APPLE__)
	glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_METAL);
#elif defined(AFFRESCOUI_GLES3) && defined(_WIN32)
	glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_D3D11);
#endif
}

#ifndef __EMSCRIPTEN__
/// @brief A window icon's pixels as GLFW takes them: RGBA, not premultiplied, at most 256 pixels a side (each pixel
/// the average of a square of the image's), so the X11 property stays small whatever the image.
/// @param icon The image.
/// @param pixels Receives the pixels; the result points into them.
/// @return The image for glfwSetWindowIcon().
GLFWimage iconImage(const Bitmap& icon, std::vector<std::uint8_t>& pixels) {
	constexpr int largest = 256;
	const int step = (std::max(icon.width(), icon.height()) + largest - 1) / largest; // the square's side
	const int width = std::max(1, icon.width() / step), height = std::max(1, icon.height() / step);
	const std::span<const std::uint8_t> source = icon.pixels();
	const auto offset = [](int x, int y, int rowWidth) {
		return (static_cast<std::size_t>(y) * static_cast<std::size_t>(rowWidth) + static_cast<std::size_t>(x)) * 4;
	};
	pixels.assign(offset(0, height, width), 0);
	for(int y = 0; y < height; ++y)
		for(int x = 0; x < width; ++x) {
			std::array<unsigned, 4> sum{};
			unsigned count = 0;
			for(int sy = y * step; sy < std::min((y + 1) * step, icon.height()); ++sy)
				for(int sx = x * step; sx < std::min((x + 1) * step, icon.width()); ++sx, ++count)
					for(std::size_t c = 0; c < 4; ++c) sum[c] += source[offset(sx, sy, icon.width()) + c];
			std::uint8_t* out = &pixels[offset(x, y, width)];
			const unsigned alpha = sum[3] / count;
			out[3] = static_cast<std::uint8_t>(alpha);
			for(std::size_t c = 0; c < 3 && alpha > 0; ++c) // the Bitmap's colours are premultiplied, GLFW's aren't
				out[c] = static_cast<std::uint8_t>(std::min(255u, (sum[c] / count * 255 + alpha / 2) / alpha));
		}
	return {width, height, pixels.data()};
}
#endif

/// @brief A GLFW window and its OpenGL context.
class GlfwWindow final : public NativeWindow {
public:
	/// @brief Opens the window, makes its context current and turns vsync on.
	/// @param title The title.
	/// @param width In window units.
	/// @param height In window units.
	/// @param sharing A window whose context shares its objects with this one's, or nullptr.
	/// @throws std::runtime_error If the window or its context can't be created.
	GlfwWindow(const std::string& title, int width, int height, GLFWwindow* sharing = nullptr)
	    : handle_(glfwCreateWindow(width, height, title.c_str(), nullptr, sharing)) {
		if(!handle_)
			throw std::runtime_error(std::string("AffrescoUI: could not create a window with an ") + contextName);
		glfwMakeContextCurrent(handle_);
		glfwSwapInterval(1); // vsync: never render faster than the display
		glfwSetInputMode(handle_, GLFW_LOCK_KEY_MODS, GLFW_TRUE); // key events tell whether caps lock is on
#ifdef __EMSCRIPTEN__
		// The canvas follows its container on the page (web/), at the page's devicePixelRatio (GLFW's port scales the
		// framebuffer by default); a page without one keeps the window's size
		if(emscripten::glfw3::MakeCanvasResizable(handle_, "#canvas-container") != EMSCRIPTEN_RESULT_SUCCESS)
			log(LogLevel::Info, "AffrescoUI: the page has no #canvas-container: the canvas keeps its size");
#endif
	}

	/// @brief Destroys the menu bar, the cursors and the window.
	~GlfwWindow() override {
		menuBar_.reset();
		for(GLFWcursor* cursor : cursors_)
			if(cursor) glfwDestroyCursor(cursor);
		glfwDestroyWindow(handle_);
	}

	GlfwWindow(const GlfwWindow&) = delete;
	GlfwWindow& operator=(const GlfwWindow&) = delete;
	GlfwWindow(GlfwWindow&&) = delete;
	GlfwWindow& operator=(GlfwWindow&&) = delete;

	void connect(Window& window) override;

	void setTitle(const std::string& title) override { glfwSetWindowTitle(handle_, title.c_str()); }
	void setIcon(const Bitmap* icon) override;
	void setSize(int width, int height) override { glfwSetWindowSize(handle_, width, height); }
	void setMinSize(int width, int height) override {
		glfwSetWindowSizeLimits(handle_, width, height, GLFW_DONT_CARE, GLFW_DONT_CARE);
	}
	[[nodiscard]] bool isVisible() const override { return glfwGetWindowAttrib(handle_, GLFW_VISIBLE) == GLFW_TRUE; }
	void setVisible(bool visible) override {
		if(visible) glfwShowWindow(handle_);
		else glfwHideWindow(handle_);
	}
	[[nodiscard]] bool shouldClose() const override { return glfwWindowShouldClose(handle_) == GLFW_TRUE; }
	void setShouldClose(bool close) override { glfwSetWindowShouldClose(handle_, close ? GLFW_TRUE : GLFW_FALSE); }
	[[nodiscard]] bool isFullscreen() const override { return glfwGetWindowMonitor(handle_) != nullptr; }
	void setFullscreen(bool fullscreen) override;

	[[nodiscard]] std::string clipboardText() const override {
		const char* text = glfwGetClipboardString(handle_);
		return text ? text : "";
	}
	void setClipboardText(const std::string& utf8) override { glfwSetClipboardString(handle_, utf8.c_str()); }
	void setCursor(Cursor cursor) override;

	[[nodiscard]] PixelSize framebufferSize() const override {
		PixelSize size;
		glfwGetFramebufferSize(handle_, &size.width, &size.height);
		return size;
	}
	void makeCurrent() override {
		if(glfwGetCurrentContext() != handle_) glfwMakeContextCurrent(handle_);
	}
	void present() override { glfwSwapBuffers(handle_); }
	[[nodiscard]] ProcAddressLoader procAddressLoader() const override {
		return reinterpret_cast<ProcAddressLoader>(glfwGetProcAddress);
	}
	void wake() override { glfwPostEmptyEvent(); }
	[[nodiscard]] void* handle() const noexcept override { return handle_; }
	[[nodiscard]] float contentScale() const override {
		float x = 1.0f, y = 1.0f;
		glfwGetWindowContentScale(handle_, &x, &y);
		return x;
	}
	bool setMenuBar(const std::vector<MenuItem>& menus) override;
	[[nodiscard]] bool fileDialogs() const override { return desktop::fileDialogs(); }
	[[nodiscard]] std::vector<std::string> chooseFiles(const FileDialog& dialog) override {
		return desktop::chooseFiles(handle_, dialog);
	}

private:
	/// @brief The Window a GLFW window delivers its events to.
	/// @param window The GLFW window.
	/// @return Its Window (set by connect()).
	static Window& owner(GLFWwindow* window) { return *static_cast<Window*>(glfwGetWindowUserPointer(window)); }

	GLFWwindow* handle_;                        ///< The window and its context; destroyed with this object.
	Window* window_ = nullptr;                  ///< The Window it was connected to.
	std::unique_ptr<desktop::MenuBar> menuBar_; ///< Its native menu bar, created on first use.
	bool noMenuBar_ = false;                    ///< Native menus aren't supported here.
	std::array<GLFWcursor*, 5> cursors_{};      ///< Standard cursors, created on first use (indexed by Cursor).
	std::array<int, 4> windowed_{};             ///< Where it was before it filled a display: x, y, width, height.
};

/// @brief The display that holds the most of a window, or the primary one.
/// @param window The window.
/// @return The display, or nullptr if there is none.
GLFWmonitor* displayOf(GLFWwindow* window) {
	int x = 0, y = 0, width = 0, height = 0;
	glfwGetWindowPos(window, &x, &y);
	glfwGetWindowSize(window, &width, &height);
	int count = 0;
	GLFWmonitor** monitors = glfwGetMonitors(&count);
	GLFWmonitor* best = glfwGetPrimaryMonitor();
	long long most = 0;
	for(int i = 0; i < count; ++i) {
		int mx = 0, my = 0;
		glfwGetMonitorPos(monitors[i], &mx, &my);
		const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
		if(!mode) continue;
		const long long across = std::max(0, std::min(x + width, mx + mode->width) - std::max(x, mx));
		const long long down = std::max(0, std::min(y + height, my + mode->height) - std::max(y, my));
		if(across * down > most) {
			most = across * down;
			best = monitors[i];
		}
	}
	return best;
}

// The display's own mode, so nothing changes its resolution; back in a window, it returns where it was
void GlfwWindow::setFullscreen(bool fullscreen) {
	if(fullscreen == isFullscreen()) return;
	if(!fullscreen) {
		glfwSetWindowMonitor(handle_, nullptr, windowed_[0], windowed_[1], windowed_[2], windowed_[3], GLFW_DONT_CARE);
		return;
	}
	GLFWmonitor* display = displayOf(handle_);
	const GLFWvidmode* mode = display ? glfwGetVideoMode(display) : nullptr;
	if(!mode) return; // no display to fill
	glfwGetWindowPos(handle_, &windowed_[0], &windowed_[1]);
	glfwGetWindowSize(handle_, &windowed_[2], &windowed_[3]);
	glfwSetWindowMonitor(handle_, display, 0, 0, mode->width, mode->height, mode->refreshRate);
}

// GLFW's callbacks are converted to typed events and routed immediately
void GlfwWindow::connect(Window& window) {
	window_ = &window;
	glfwSetWindowUserPointer(handle_, &window);

	// Size changes only mark the window dirty; the main loop or the refresh callback draws once.
	// (During a live resize on macOS the loop is blocked and the refresh callback fires after every step.)
	glfwSetWindowSizeCallback(handle_,
	                          [](GLFWwindow* native, int width, int height) { resized(owner(native), width, height); });
	glfwSetFramebufferSizeCallback(handle_, [](GLFWwindow* native, int, int) { framebufferResized(owner(native)); });
	glfwSetWindowRefreshCallback(handle_, [](GLFWwindow* native) { refresh(owner(native)); });

	glfwSetKeyCallback(handle_, [](GLFWwindow* native, int key, int scancode, int action, int mods) {
		owner(native).routeKey({static_cast<Key>(key), scancode, static_cast<Action>(action), Mods{mods}});
	});
	glfwSetCharCallback(handle_, [](GLFWwindow* native, unsigned int codepoint) {
		owner(native).routeChar({static_cast<char32_t>(codepoint)});
	});
	glfwSetCursorPosCallback(handle_, [](GLFWwindow* native, double x, double y) {
		owner(native).routeMouseMove({static_cast<float>(x), static_cast<float>(y)});
	});
	glfwSetMouseButtonCallback(handle_, [](GLFWwindow* native, int button, int action, int mods) {
		// Asked rather than taken from the last move: a click can come before any move event
		double x = 0.0, y = 0.0;
		glfwGetCursorPos(native, &x, &y);
		owner(native).routeMouseButton({static_cast<MouseButton>(button),
		                                static_cast<Action>(action),
		                                Mods{mods},
		                                {static_cast<float>(x), static_cast<float>(y)}});
	});
	glfwSetScrollCallback(handle_, [](GLFWwindow* native, double dx, double dy) {
		Window& w = owner(native);
		w.routeScroll({{static_cast<float>(dx), static_cast<float>(dy)}, w.pointer()});
	});
	glfwSetCursorEnterCallback(
	    handle_, [](GLFWwindow* native, int entered) { owner(native).routeCursorEnter(entered == GLFW_TRUE); });

	glfwSetDropCallback(handle_, [](GLFWwindow* native, int count, const char** paths) {
		double x = 0.0, y = 0.0;
		glfwGetCursorPos(native, &x, &y);
		const DropEvent event{std::vector<std::string>(paths, paths + count),
		                      {static_cast<float>(x), static_cast<float>(y)}};
		dropped(owner(native), event);
	});
	glfwSetWindowContentScaleCallback(handle_,
	                                  [](GLFWwindow* native, float x, float) { displayChanged(owner(native), x); });
	glfwSetWindowFocusCallback(handle_, [](GLFWwindow* native, int focused) {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast): GlfwPlatform makes only GlfwWindows
		auto* self = static_cast<GlfwWindow*>(&of(owner(native)));
		if(focused == GLFW_TRUE && self->menuBar_) self->menuBar_->activate(); // macOS: the bar is the app's
	});
}

bool GlfwWindow::setMenuBar(const std::vector<MenuItem>& menus) {
	if(noMenuBar_ || !window_) return false;
	if(!menuBar_) {
		Window* window = window_;
		menuBar_ =
		    desktop::createMenuBar(handle_, [window](std::function<void()> run) { chosen(*window, std::move(run)); });
		if(!menuBar_) {
			noMenuBar_ = true;
			return false;
		}
	}
	menuBar_->show(menus);
	return true;
}

void GlfwWindow::setIcon(const Bitmap* icon) {
	desktop::setApplicationIcon(icon); // macOS: the Dock's, as its windows have none
#ifndef __EMSCRIPTEN__
	// Only Win32 and X11 windows have icons: GLFW reports an error elsewhere (Cocoa, Wayland)
	if(glfwGetPlatform() != GLFW_PLATFORM_WIN32 && glfwGetPlatform() != GLFW_PLATFORM_X11) return;
	if(!icon) {
		glfwSetWindowIcon(handle_, 0, nullptr); // the system's default
		return;
	}
	std::vector<std::uint8_t> pixels;
	const GLFWimage image = iconImage(*icon, pixels);
	glfwSetWindowIcon(handle_, 1, &image);
#endif
}

void GlfwWindow::setCursor(Cursor cursor) {
	if(cursor == Cursor::Arrow) {
		glfwSetCursor(handle_, nullptr); // the system default
		return;
	}

	static constexpr std::array<int, 5> standardShapes = {
	    GLFW_ARROW_CURSOR, GLFW_IBEAM_CURSOR, GLFW_POINTING_HAND_CURSOR, GLFW_RESIZE_EW_CURSOR, GLFW_RESIZE_NS_CURSOR};
	const auto index = static_cast<std::size_t>(cursor);
	if(!cursors_[index]) cursors_[index] = glfwCreateStandardCursor(standardShapes[index]);
	glfwSetCursor(handle_, cursors_[index]);
}

/// @brief GLFW, started for as long as it lives.
class GlfwPlatform final : public Platform {
public:
	/// @brief Starts GLFW, sending its errors to the log.
	/// @param hiddenWindows Whether windows open hidden.
	/// @throws std::runtime_error If GLFW can't start.
	explicit GlfwPlatform(bool hiddenWindows) : hiddenWindows_(hiddenWindows) {
		glfwSetErrorCallback([](int code, const char* description) {
			log(LogLevel::Error, "AffrescoUI: GLFW error " + std::to_string(code) + ": " + description);
		});
		hintAngle();
		if(!glfwInit()) throw std::runtime_error("AffrescoUI: could not initialise GLFW");
		desktop::watchColorScheme();
	}
	/// @brief Stops GLFW; every window must be gone.
	~GlfwPlatform() override { glfwTerminate(); }

	GlfwPlatform(const GlfwPlatform&) = delete;
	GlfwPlatform& operator=(const GlfwPlatform&) = delete;
	GlfwPlatform(GlfwPlatform&&) = delete;
	GlfwPlatform& operator=(GlfwPlatform&&) = delete;

	[[nodiscard]] std::unique_ptr<NativeWindow> createWindow(const std::string& title, int width, int height) override {
		hintContext(); // other hints (e.g. GLFW_VISIBLE, set by tests) stay
		if(hiddenWindows_) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		return std::make_unique<GlfwWindow>(title, width, height);
	}

	[[nodiscard]] std::unique_ptr<NativeWindow> createSharedWindow(const std::string& title, int width, int height,
	                                                               void* sharing) override {
		hintContext();
		if(hiddenWindows_) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		return std::make_unique<GlfwWindow>(title, width, height, static_cast<GLFWwindow*>(sharing));
	}

	void waitEvents(std::optional<double> timeoutSeconds) override {
		if(timeoutSeconds) glfwWaitEventsTimeout(*timeoutSeconds);
		else glfwWaitEvents();
	}

	void wake() override { glfwPostEmptyEvent(); }

	[[nodiscard]] ColorScheme colorScheme() const override { return desktop::colorScheme(); }
	[[nodiscard]] bool reducedMotion() const override { return desktop::reducedMotion(); }

private:
	bool hiddenWindows_; ///< Windows open hidden (ApplicationOptions::hiddenWindows).
};

} // namespace

std::unique_ptr<Platform> createGlfwPlatform(bool hiddenWindows) {
	return std::make_unique<GlfwPlatform>(hiddenWindows);
}

} // namespace affresco::platform
