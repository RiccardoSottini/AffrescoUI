#pragma once

#include "sgl/render/Events.hpp"
#include "sgl/render/Font.hpp"
#include "sgl/render/Panel.hpp"
#include "sgl/core/Signal.hpp"

#include <array>
#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;
struct GLFWcursor;

namespace sgl {

namespace gl {
class QuadRenderer;
}

// An OS window with its own OpenGL context and a root Panel filling it. Created by Application.
class Window {
public:
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	/* Properties */
	[[nodiscard]] const std::string& title() const noexcept { return title_; }
	void setTitle(std::string title);

	[[nodiscard]] int width() const noexcept { return width_; }
	[[nodiscard]] int height() const noexcept { return height_; }
	void setSize(int width, int height); // clamped to the minimum size

	[[nodiscard]] int minWidth() const noexcept { return minWidth_; }
	[[nodiscard]] int minHeight() const noexcept { return minHeight_; }
	void setMinSize(int minWidth, int minHeight);

	[[nodiscard]] bool isVisible() const;
	void setVisible(bool visible);

	[[nodiscard]] bool shouldClose() const;

	/* Content */
	[[nodiscard]] Panel& root() noexcept { return root_; }

	template <std::derived_from<Panel> T = Panel, typename... Args> T& add(Args&&... args) {
		return root_.add<T>(std::forward<Args>(args)...);
	}

	[[nodiscard]] Panel* focusedPanel() const noexcept { return focused_; }
	void setFocus(Panel* panel);

	[[nodiscard]] Panel* panelAt(Vec2 point); // nullptr outside the window

	// Font used by labels that don't have their own
	void setDefaultFont(std::shared_ptr<const Font> font);
	[[nodiscard]] const Font* defaultFont() const noexcept { return defaultFont_.get(); }

	[[nodiscard]] Panel* hoveredPanel() const noexcept { return hovered_; }
	[[nodiscard]] Panel* mouseGrabber() const noexcept { return grabbed_; } // panel receiving the mouse during a drag

	/* Signals (fired after the focused/hovered panel handled the event) */
	Signal<const KeyEvent&> onKey;
	Signal<const CharEvent&> onChar;
	Signal<const MouseMoveEvent&> onMouseMove;
	Signal<const MouseButtonEvent&> onMouseButton;
	Signal<const ScrollEvent&> onScroll;
	Signal<const CursorEnterEvent&> onCursorEnter;
	Signal<const ResizeEvent&> onResize;

	/* Rendering */
	void invalidate() noexcept {
		if(!layingOut_) layoutDirty_ = redrawDirty_ = true; // changes made by the layout pass itself don't count
	}
	// Runs `task` once the event being handled has been fully delivered (or before the next frame).
	// Lets event handlers rebuild parts of the UI, even the panel whose handler is running.
	void defer(std::function<void()> task);

	void render();                                          // lays out and draws only if something changed
	bool saveScreenshot(const std::filesystem::path& path); // draws the current frame to a binary PPM image

	[[nodiscard]] GLFWwindow* nativeHandle() const noexcept { return handle_.get(); }

private:
	friend class Application;
	friend class Panel;

	Window(std::string title, int width, int height);

	void updateLayout();
	void draw();
	void installCallbacks();

	// Input routing: keyboard to the focused panel; mouse to the panel under the pointer, or to the panel that
	// received the button press until it is released (mouse capture, so drags keep working outside the panel)
	void routeKey(const KeyEvent& event);
	void routeChar(const CharEvent& event);
	void routeMouseMove(Vec2 position);
	void routeMouseButton(const MouseButtonEvent& event);
	void routeScroll(const ScrollEvent& event);
	void routeCursorEnter(bool entered);

	void setHovered(Panel* panel);
	void forget(const Panel& panel) noexcept; // called by a panel being destroyed
	void runDeferred();
	void updateCursor();

	struct HandleDeleter {
		void operator()(GLFWwindow* handle) const noexcept;
	};

	// Declaration order matters: the renderer must be destroyed before the window (and its GL context)
	std::unique_ptr<GLFWwindow, HandleDeleter> handle_;
	std::unique_ptr<gl::QuadRenderer> renderer_;

	std::string title_;
	int width_ = 0, height_ = 0;
	int minWidth_ = 0, minHeight_ = 0;
	Vec2 cursor_;
	bool layoutDirty_ = true;
	bool redrawDirty_ = true;
	bool layingOut_ = false;
	int routing_ = 0; // depth of event delivery in progress
	std::vector<std::function<void()>> deferred_;

	std::shared_ptr<const Font> defaultFont_;
	Panel* focused_ = nullptr;
	Panel* hovered_ = nullptr;
	Panel* grabbed_ = nullptr;

	std::array<GLFWcursor*, 5> cursors_{}; // standard cursors, created on first use (indexed by Cursor)
	Cursor currentCursor_ = Cursor::Arrow;
	Panel root_;
};

} // namespace sgl
