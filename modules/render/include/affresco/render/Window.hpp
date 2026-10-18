#pragma once

/// @file
/// @brief Window: an OS window with its own OpenGL context and a root panel.

#include "affresco/render/Events.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/core/Signal.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct GLFWwindow;
struct GLFWcursor;

namespace affresco {

class Label;

namespace gl {
class QuadRenderer;
}

/// @brief An OS window with its own OpenGL context and a root Panel filling it. Created by Application.
///
/// Routes input to its panels and draws only when something changed. Sizes are in window units; on HiDPI screens
/// the framebuffer has more pixels.
class Window {
public:
	~Window();

	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(Window&&) = delete;

	/* Properties */

	/// @brief The title.
	[[nodiscard]] const std::string& title() const noexcept { return title_; }
	/// @brief Sets the title.
	/// @param title The title.
	void setTitle(std::string title);

	/// @brief The width, in window units.
	[[nodiscard]] int width() const noexcept { return width_; }
	/// @brief The height, in window units.
	[[nodiscard]] int height() const noexcept { return height_; }
	/// @brief Resizes the window, clamped to the minimum size.
	/// @param width The width.
	/// @param height The height.
	void setSize(int width, int height);

	/// @brief The minimum width.
	[[nodiscard]] int minWidth() const noexcept { return minWidth_; }
	/// @brief The minimum height.
	[[nodiscard]] int minHeight() const noexcept { return minHeight_; }
	/// @brief Sets the minimum size, growing the window if needed.
	/// @param minWidth The minimum width.
	/// @param minHeight The minimum height.
	void setMinSize(int minWidth, int minHeight);

	/// @brief Whether the window is shown.
	[[nodiscard]] bool isVisible() const;
	/// @brief Shows or hides the window.
	/// @param visible true to show it.
	void setVisible(bool visible);

	/// @brief Whether the user asked to close the window.
	[[nodiscard]] bool shouldClose() const;

	/* Content */

	/// @brief The root panel, which always fills the window.
	[[nodiscard]] Panel& root() noexcept { return root_; }

	/// @brief Creates a child of the root panel.
	/// @tparam T Panel or a subclass.
	/// @param args Forwarded to T's constructor.
	/// @return The child.
	template <std::derived_from<Panel> T = Panel, typename... Args> T& add(Args&&... args) {
		return root_.add<T>(std::forward<Args>(args)...);
	}

	/// @brief The panel receiving the keyboard, or nullptr.
	[[nodiscard]] Panel* focusedPanel() const noexcept { return focused_; }
	/// @brief Moves the keyboard focus (a left click also does).
	/// @param panel The panel, or nullptr.
	void setFocus(Panel* panel);

	/// @brief Finds the panel at a point, laying out first if needed.
	/// @param point In window units.
	/// @return The deepest shown panel there, or nullptr outside the window.
	[[nodiscard]] Panel* panelAt(Vec2 point);

	/// @brief Sets the font of labels that don't have their own.
	/// @param font The font.
	void setDefaultFont(std::shared_ptr<const Font> font);
	/// @brief The font of labels that don't have their own, or nullptr.
	[[nodiscard]] const Font* defaultFont() const noexcept { return defaultFont_.get(); }

	/// @brief The system clipboard's text, as UTF-8.
	[[nodiscard]] std::string clipboardText() const;
	/// @brief Sets the system clipboard's text.
	/// @param utf8 UTF-8 text.
	void setClipboardText(const std::string& utf8);

	/// @brief The panel under the pointer, or nullptr. It and its ancestors count as hovered.
	[[nodiscard]] Panel* hoveredPanel() const noexcept { return hovered_; }
	/// @brief The last pointer position, in window units.
	[[nodiscard]] Vec2 pointer() const noexcept { return cursor_; }
	/// @brief The panel holding the mouse during a drag (from button press to release), or nullptr.
	[[nodiscard]] Panel* mouseGrabber() const noexcept { return grabbed_; }

	/* Signals, fired after the focused or hovered panel handled the event */

	Signal<const KeyEvent&> onKey;                 ///< Every key event.
	Signal<const CharEvent&> onChar;               ///< Every typed character.
	Signal<const MouseMoveEvent&> onMouseMove;     ///< Every pointer move.
	Signal<const MouseButtonEvent&> onMouseButton; ///< Every button press or release.
	Signal<const ScrollEvent&> onScroll;           ///< Every scroll.
	Signal<const CursorEnterEvent&> onCursorEnter; ///< The pointer entered or left.
	Signal<const ResizeEvent&> onResize;           ///< The window was resized.

	/* Input routing: keyboard to the focused panel; mouse to the panel under the pointer, or to the panel that
	   received the button press until it is released (mouse capture, so drags work outside the panel). Deferred
	   tasks run once the outermost event is delivered. The OS callbacks call these; tests and automation can too. */

	/// @brief Delivers a key event to the focused panel, then onKey.
	/// @param event The event.
	void routeKey(const KeyEvent& event);
	/// @brief Delivers a typed character to the focused panel, then onChar.
	/// @param event The event.
	void routeChar(const CharEvent& event);
	/// @brief Updates the hovered panel and delivers the move to the grabbing or hovered panel, then onMouseMove.
	/// @param position The pointer position.
	void routeMouseMove(Vec2 position);
	/// @brief Delivers a button event; a press focuses (left button) and captures, a release ends the capture.
	/// @param event The event.
	void routeMouseButton(const MouseButtonEvent& event);
	/// @brief Delivers a scroll to the panel under the pointer, then onScroll.
	/// @param event The event.
	void routeScroll(const ScrollEvent& event);
	/// @brief Clears the hovered panel when the pointer leaves, then onCursorEnter.
	/// @param entered true if the pointer entered.
	void routeCursorEnter(bool entered);

	/* Rendering */

	/// @brief Marks layout and frame as out of date. Changes made by the layout pass itself don't count.
	void invalidate() noexcept {
		if(!layingOut_) layoutDirty_ = redrawDirty_ = true; // changes made by the layout pass itself don't count
	}

	/// @brief Runs a task once the current event has been fully delivered (or before the next frame).
	///
	/// Lets event handlers rebuild parts of the UI, even the panel whose handler is running.
	/// @param task The work to run; it may defer more.
	void defer(std::function<void()> task);

	/// @brief Runs a task later, on the main thread (inside Application::run).
	/// @param delayMs Milliseconds from now.
	/// @param task The work to run; it may set more timers.
	/// @return An id for cancelTimer().
	std::uint64_t setTimer(double delayMs, std::function<void()> task);
	/// @brief Cancels a timer that hasn't run yet.
	/// @param id From setTimer(); unknown ids are ignored.
	void cancelTimer(std::uint64_t id);
	/// @brief How long until the next timer is due.
	/// @return Seconds (0 if one is due now), or std::nullopt if none is set.
	[[nodiscard]] std::optional<double> secondsToNextTimer() const;

	/// @brief Runs due timers and deferred tasks, then lays out, draws and presents a frame if something changed.
	void render();

	/// @brief Draws the current frame and saves it without presenting it.
	/// @param path Binary PPM file to write, at framebuffer resolution.
	/// @return false if it couldn't be written.
	bool saveScreenshot(const std::filesystem::path& path);
	/// @brief Draws the current frame offscreen, at a fixed scale whatever the display, and saves it (for tests).
	/// @param path Binary PPM file to write.
	/// @param scale Pixels per window unit (2 looks like a Retina display).
	/// @return false if it couldn't be written.
	bool saveScreenshot(const std::filesystem::path& path, int scale);

	/// @brief The GLFW window.
	[[nodiscard]] GLFWwindow* nativeHandle() const noexcept { return handle_.get(); }

private:
	friend class Application;
	friend class Panel;

	/// @brief Opens the OS window, loads OpenGL and turns vsync on.
	/// @param title The title.
	/// @param width Width in window units.
	/// @param height Height in window units.
	/// @throws std::runtime_error If the window, its context or OpenGL can't be set up.
	Window(std::string title, int width, int height);

	/// @brief Lays the tree out if anything changed; the root fills the window.
	void updateLayout();
	/// @brief Draws the tree into the back buffer, in this window's context.
	void draw();
	/// @brief Draws the tree into the current framebuffer (the back buffer, or an offscreen image).
	/// @param framebufferWidth In pixels.
	/// @param framebufferHeight In pixels.
	void draw(int framebufferWidth, int framebufferHeight);
	/// @brief Connects the GLFW callbacks.
	void installCallbacks();

	/// @brief Changes the hovered panel, sending hover events to it and to its ancestors that the pointer entered or
	/// left, and updating the cursor and the tooltip.
	/// @param panel The panel under the pointer, or nullptr.
	void setHovered(Panel* panel);
	/// @brief Lays out the overlays inside a subtree (after the tree), collecting them in drawing order.
	/// @param panel The subtree.
	void layoutOverlays(Panel& panel);
	/// @brief Lays out one overlay next to its anchor.
	/// @param overlay The overlay.
	/// @param parent Its parent: the anchor when it names none.
	void layoutOverlay(Panel& overlay, const Panel& parent);
	/// @brief Starts the tooltip delay for the hovered panel or its nearest ancestor with a tooltip; hides any shown.
	void restartTooltip();
	/// @brief Runs the timers that are due.
	void runTimers();
	/// @brief Drops the focus, hover and capture pointers to a panel being destroyed.
	/// @param panel The panel.
	void forget(const Panel& panel) noexcept;
	/// @brief Runs deferred tasks until none are left.
	void runDeferred();
	/// @brief Shows the cursor of the grabbing or hovered panel.
	void updateCursor();

	/// @brief Destroys a GLFW window, for std::unique_ptr.
	struct HandleDeleter {
		/// @brief Destroys the window.
		/// @param handle The window.
		void operator()(GLFWwindow* handle) const noexcept;
	};

	// Declaration order matters: the renderer must be destroyed before the window (and its GL context)
	std::unique_ptr<GLFWwindow, HandleDeleter> handle_; ///< OS window and OpenGL context.
	std::unique_ptr<gl::QuadRenderer> renderer_;        ///< Draws the tree.

	std::string title_;                           ///< See title().
	int width_ = 0, height_ = 0;                  ///< Size, in window units.
	int minWidth_ = 0, minHeight_ = 0;            ///< Minimum size.
	Vec2 cursor_;                                 ///< Last pointer position.
	bool layoutDirty_ = true;                     ///< Layout needed before the next frame.
	bool redrawDirty_ = true;                     ///< render() must draw.
	bool layingOut_ = false;                      ///< A layout pass is running.
	std::uint64_t layoutPass_ = 0;                ///< Counts layout passes (Panel::layoutPass_ says which one it was in).
	bool forwardAnchor_ = false;                  ///< A panel anchored to one not laid out yet in this pass.
	int routing_ = 0;                             ///< Depth of event delivery in progress.
	std::vector<std::function<void()>> deferred_; ///< See defer().

	std::shared_ptr<const Font> defaultFont_; ///< See defaultFont().
	Panel* focused_ = nullptr;                ///< See focusedPanel().
	Panel* hovered_ = nullptr;                ///< See hoveredPanel().
	Panel* grabbed_ = nullptr;                ///< See mouseGrabber().

	/// @brief A task waiting for its time.
	struct Timer {
		std::uint64_t id;           ///< See setTimer().
		double due;                 ///< glfwGetTime() when it runs.
		std::function<void()> task; ///< The work.
	};
	std::vector<Timer> timers_;    ///< See setTimer().
	std::uint64_t nextTimerId_ = 1; ///< The next timer's id.

	std::vector<Panel*> overlays_;    ///< Overlays shown in the last layout, in drawing order.
	std::unique_ptr<Label> tooltip_;  ///< The tooltip, created on first use.
	bool tooltipShown_ = false;       ///< The tooltip is on screen.
	std::uint64_t tooltipTimer_ = 0;  ///< The delay before showing it, if running.

	std::array<GLFWcursor*, 5> cursors_{}; ///< Standard cursors, created on first use (indexed by Cursor).
	Cursor currentCursor_ = Cursor::Arrow; ///< Shape shown now.
	Panel root_;                           ///< Fills the window.
};

} // namespace affresco
