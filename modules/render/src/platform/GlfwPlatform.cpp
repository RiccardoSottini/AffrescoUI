/// @file
/// @brief The GLFW platform: windows with an OpenGL 4.1 core context, their input, cursors and clipboard, and the
/// event loop's waiting. The only file of the render module that calls GLFW.

#include "platform/Platform.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/render/Window.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace affresco::platform {

namespace {

/// @brief A GLFW window and its OpenGL context.
class GlfwWindow final : public NativeWindow {
public:
	/// @brief Opens the window, makes its context current and turns vsync on.
	/// @param title The title.
	/// @param width In window units.
	/// @param height In window units.
	/// @throws std::runtime_error If the window or its context can't be created.
	GlfwWindow(const std::string& title, int width, int height)
	    : handle_(glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr)) {
		if(!handle_) throw std::runtime_error("AffrescoUI: could not create a window with an OpenGL 4.1 core context");
		glfwMakeContextCurrent(handle_);
		glfwSwapInterval(1); // vsync: never render faster than the display
	}

	/// @brief Destroys the cursors and the window.
	~GlfwWindow() override {
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

private:
	/// @brief The Window a GLFW window delivers its events to.
	/// @param window The GLFW window.
	/// @return Its Window (set by connect()).
	static Window& owner(GLFWwindow* window) { return *static_cast<Window*>(glfwGetWindowUserPointer(window)); }

	GLFWwindow* handle_;                   ///< The window and its context; destroyed with this object.
	std::array<GLFWcursor*, 5> cursors_{}; ///< Standard cursors, created on first use (indexed by Cursor).
};

// GLFW's callbacks are converted to typed events and routed immediately
void GlfwWindow::connect(Window& window) {
	glfwSetWindowUserPointer(handle_, &window);

	// Size changes only mark the window dirty; the main loop or the refresh callback draws once.
	// (During a live resize on macOS the loop is blocked and the refresh callback fires after every step.)
	glfwSetWindowSizeCallback(handle_,
	                          [](GLFWwindow* native, int width, int height) { resized(owner(native), width, height); });
	glfwSetFramebufferSizeCallback(handle_, [](GLFWwindow* native, int, int) { owner(native).invalidate(); });
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
	/// @throws std::runtime_error If GLFW can't start.
	GlfwPlatform() {
		glfwSetErrorCallback([](int code, const char* description) {
			log(LogLevel::Error, "AffrescoUI: GLFW error " + std::to_string(code) + ": " + description);
		});
		if(!glfwInit()) throw std::runtime_error("AffrescoUI: could not initialise GLFW");
	}
	/// @brief Stops GLFW; every window must be gone.
	~GlfwPlatform() override { glfwTerminate(); }

	GlfwPlatform(const GlfwPlatform&) = delete;
	GlfwPlatform& operator=(const GlfwPlatform&) = delete;
	GlfwPlatform(GlfwPlatform&&) = delete;
	GlfwPlatform& operator=(GlfwPlatform&&) = delete;

	[[nodiscard]] std::unique_ptr<NativeWindow> createWindow(const std::string& title, int width, int height) override {
		// OpenGL 4.1 core: the highest version available on macOS. Other hints (e.g. GLFW_VISIBLE, set by tests) stay.
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
		return std::make_unique<GlfwWindow>(title, width, height);
	}

	void waitEvents(std::optional<double> timeoutSeconds) override {
		if(timeoutSeconds) glfwWaitEventsTimeout(*timeoutSeconds);
		else glfwWaitEvents();
	}

	void wake() override { glfwPostEmptyEvent(); }
};

} // namespace

std::unique_ptr<Platform> createGlfwPlatform() { return std::make_unique<GlfwPlatform>(); }

} // namespace affresco::platform
