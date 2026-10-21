#pragma once

/// @file
/// @brief Window: a root panel with its input routing, focus, timers and layout, shown in an OS window with its own
/// OpenGL context, or headless (no display, no OpenGL).

#include "affresco/render/Clock.hpp"
#include "affresco/render/Events.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"
#include "affresco/core/Signal.hpp"

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

namespace affresco {

class Label;

namespace platform {
class NativeWindow;
}
namespace detail {
class Renderer;
struct Frame;
struct WindowFactory;
} // namespace detail

/// @brief An entry of a window's menu bar (Window::setMenuBar()): a menu, a command, or a separator.
struct MenuItem {
	std::string title;             ///< What it shows.
	std::string keyLabel;          ///< Its shortcut as shown (e.g. "Ctrl+S"), or empty. Only shown: the window's own
	                               ///< key handling runs shortcuts, so they never run twice.
	bool enabled = true;           ///< false greys it out.
	bool separator = false;        ///< A line between groups; the other fields are ignored.
	std::vector<MenuItem> submenu; ///< Its items, for a menu (the top level holds menus).
	std::function<void()> run;     ///< What choosing it does, for a command.
};

/// @brief The ring a window draws around the focused panel when the keyboard moved the focus there (Window::setFocusRing()).
struct FocusRing {
	Color color = Color::hex(0x2563EB); ///< Its colour.
	float width = 2.0f;                 ///< How thick it is, in pixels.
	float offset = 2.0f;                ///< The gap between it and the panel, in pixels.

	/// @brief Whether two rings look the same.
	bool operator==(const FocusRing&) const = default;
};

/// @brief What a file dialog asks for (FileDialog::kind).
enum class FileDialogKind {
	Open,  ///< Files to open, which exist.
	Save,  ///< A file to save to, which may exist.
	Folder ///< A folder.
};

/// @brief A kind of file a file dialog lists (FileDialog::filters).
struct FileFilter {
	std::string name;                    ///< What it shows (e.g. "Images").
	std::vector<std::string> extensions; ///< The extensions, without the dot (e.g. "png", "jpg").
};

/// @brief A file dialog the OS shows (Window::chooseFiles()).
struct FileDialog {
	FileDialogKind kind = FileDialogKind::Open; ///< Open, save or choose a folder.
	std::string title;                          ///< Its title; empty for the OS's own.
	std::vector<FileFilter> filters{};          ///< The kinds of file listed (open and save); none lists all.
	std::string folder{};                       ///< The folder it starts in; empty for the OS's choice.
	std::string name{};                         ///< The file name it suggests (save).
	bool multiple = false;                      ///< Several files may be chosen (open).
};

/// @brief Answers file dialogs instead of the OS (Window::setFileDialogHandler()): tests, automation, or an app's own
/// dialog. Returns the paths chosen, or none for a cancel.
using FileDialogHandler = std::function<std::vector<std::string>(const FileDialog& dialog)>;

/// @brief A root Panel filling a window: routes input to its panels, keeps the focus, hover and mouse capture, runs
/// timers and deferred tasks, lays the tree out, and draws only when something changed. Created by Application.
///
/// Its logic needs no display. An Application's window is an OS window with an OpenGL context, behind the render
/// module's platform layer and renderer; a headless window (for tests and automation) has neither, and does
/// everything but draw. Sizes are in window units; on HiDPI screens the framebuffer has more pixels. Its timers,
/// tooltip delay and double clicks read its clock(). Main thread only.
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

	/// @brief The clock its timers, tooltip delay and double clicks read, and the animations of what is built into it.
	[[nodiscard]] const Clock& clock() const noexcept { return *clock_; }

	/// @brief Whether the window is shown.
	[[nodiscard]] bool isVisible() const;
	/// @brief Shows or hides the window.
	/// @param visible true to show it.
	void setVisible(bool visible);

	/// @brief Whether the user asked to close the window.
	[[nodiscard]] bool shouldClose() const;

	/* Desktop */

	/// @brief Pixels per window unit on the window's display (2 on most HiDPI screens; 1 when headless).
	[[nodiscard]] float contentScale() const noexcept { return scale_; }
	/// @brief Whether the system shows light or dark windows (Light when headless, or where it can't be read).
	[[nodiscard]] ColorScheme colorScheme() const noexcept { return colorScheme_; }
	/// @brief Whether the system asks for less motion (false when headless, or where it can't be read): animations
	/// should then end at once.
	[[nodiscard]] bool reducedMotion() const noexcept { return reducedMotion_; }

	/// @brief Sets the window's native menu bar (the screen's top on macOS, the window's top on Windows).
	/// @param menus The menus, each with its items; empty removes the bar.
	/// @return Whether the OS shows it: false when headless or where native menus aren't supported, and an app then
	/// draws a menu bar of its own.
	bool setMenuBar(std::vector<MenuItem> menus);
	/// @brief The menus last given to setMenuBar(), shown natively or not.
	[[nodiscard]] const std::vector<MenuItem>& menuBar() const noexcept { return menus_; }
	/// @brief Whether the OS shows the menu bar (see setMenuBar()).
	[[nodiscard]] bool nativeMenuBar() const noexcept { return nativeMenuBar_; }

	/// @brief Shows a file dialog over the window and waits until the user chooses or cancels (the frames stop
	/// meanwhile; the OS keeps the dialog responsive). Call it from input handling, never from a frame or a timer that
	/// repeats: the OS may refuse dialogs no user asked for.
	/// @param dialog What it asks for.
	/// @return The paths chosen (UTF-8, absolute), or none if the user cancelled, it failed, or there are no file
	/// dialogs (headless, Linux built without D-Bus). The handler of setFileDialogHandler() answers first, if set.
	std::vector<std::string> chooseFiles(const FileDialog& dialog);
	/// @brief Answers file dialogs instead of the OS, headless too (tests, automation, an app's own dialog).
	/// @param handler The handler; an empty one gives them back to the OS.
	void setFileDialogHandler(FileDialogHandler handler);
	/// @brief Whether chooseFiles() can show anything: a handler is set, or the OS shows file dialogs.
	[[nodiscard]] bool fileDialogs() const;

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
	/// @brief Whether the focus is visible: the keyboard moved it to the focused panel, rather than a click or the
	/// app (like CSS :focus-visible).
	[[nodiscard]] bool focusVisible() const noexcept { return focused_ && focusVisible_; }
	/// @brief Draws a ring around the focused panel, above everything, while the focus is visible (focusVisible()).
	/// It follows the panel's corners and isn't hit: clicks reach what is under it.
	/// @param ring The ring, or std::nullopt (the default) for none.
	void setFocusRing(std::optional<FocusRing> ring);
	/// @brief The ring setFocusRing() gave, or std::nullopt.
	[[nodiscard]] const std::optional<FocusRing>& focusRing() const noexcept { return focusRing_; }

	/// @brief Finds the panel at a point, laying out first if needed.
	/// @param point In window units.
	/// @return The deepest shown panel there, or nullptr outside the window.
	[[nodiscard]] Panel* panelAt(Vec2 point);

	/// @brief Sets the font of labels that don't have their own.
	/// @param font The font.
	void setDefaultFont(std::shared_ptr<const Font> font);
	/// @brief The font of labels that don't have their own, or nullptr.
	[[nodiscard]] const Font* defaultFont() const noexcept { return defaultFont_.get(); }

	/// @brief The system clipboard's text, as UTF-8 (a headless window keeps a clipboard of its own).
	[[nodiscard]] std::string clipboardText() const;
	/// @brief Sets the system clipboard's text (a headless window keeps a clipboard of its own).
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
	Signal<const DropEvent&> onDrop;               ///< Files were dropped from the OS.
	Signal<const DisplayEvent&> onDisplay;         ///< The display's scale, the system's colour scheme or its motion setting changed.

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
	/// @brief Delivers files dropped from the OS: the pointer moves where they were dropped, then onDrop.
	/// @param event The event.
	void routeDrop(const DropEvent& event);
	/// @brief Takes a new display scale, colour scheme or motion setting: a new scale draws everything again. Then
	/// onDisplay, if one changed.
	/// @param event The scale, scheme and motion setting now.
	void routeDisplay(const DisplayEvent& event);
	/// @brief Whether an input event is being delivered: what runs now runs because of the user's input, not a timer
	/// or a deferred task.
	/// @return true while one of the route methods runs.
	[[nodiscard]] bool routingInput() const noexcept { return routing_ > 0; }

	/* Rendering */

	/// @brief Marks the whole layout and frame as out of date: the next pass lays out every panel again and measures
	/// every size again, and the next frame records and draws every panel again, as for something all panels read (the
	/// default font). A change to one panel needs only that panel's own invalidate(). Changes made by the layout pass
	/// itself don't count.
	void invalidate() noexcept {
		if(layingOut_) return;
		layoutDirty_ = redrawDirty_ = layoutAll_ = damageAll_ = true;
		++measureGeneration_;
		++paintGeneration_;
	}

	/// @brief Runs a task once the current event has been fully delivered (or before the next frame).
	///
	/// Lets event handlers rebuild parts of the UI, even the panel whose handler is running.
	/// @param task The work to run; it may defer more.
	void defer(std::function<void()> task);
	/// @brief Runs the deferred tasks now, and those they defer, as the end of an event does: e.g. before something
	/// replaces what the window shows, so that the tasks run on what deferred them.
	void runDeferredTasks() { runDeferred(); }

	/// @brief Runs a task later, on the main thread (inside Application::run).
	/// @param delayMs Milliseconds from now.
	/// @param task The work to run; it may set more timers.
	/// @return An id for cancelTimer().
	std::uint64_t setTimer(double delayMs, std::function<void()> task);
	/// @brief Cancels a timer that hasn't run yet, even one due in the frame whose timers are running.
	/// @param id From setTimer(); unknown ids are ignored.
	void cancelTimer(std::uint64_t id);
	/// @brief How long until the next timer is due.
	/// @return Seconds (0 if one is due now), or std::nullopt if none is set.
	[[nodiscard]] std::optional<double> secondsToNextTimer() const;

	/// @brief Runs due timers and deferred tasks, then lays out, draws and presents a frame if something changed. A
	/// headless window lays out instead of drawing.
	void render();

	/// @brief Draws the current frame and saves it without presenting it.
	/// @param path Binary PPM file to write, at framebuffer resolution.
	/// @return false if it couldn't be written, or the window is headless.
	bool saveScreenshot(const std::filesystem::path& path);
	/// @brief Draws the current frame offscreen, at a fixed scale whatever the display, and saves it (for tests).
	/// @param path Binary PPM file to write.
	/// @param scale Pixels per window unit (2 looks like a Retina display).
	/// @return false if it couldn't be written, or the window is headless.
	bool saveScreenshot(const std::filesystem::path& path, int scale);

	/// @brief The GLFW window, or nullptr for a headless window.
	[[nodiscard]] GLFWwindow* nativeHandle() const noexcept;

private:
	friend class Application;
	friend class Panel;
	friend class platform::NativeWindow;
	friend struct detail::WindowFactory;

	/// @brief Sets a window up around its parts.
	/// @param native The OS window, or nullptr for a headless one; owned from now on, and connected to this window.
	/// @param renderer Draws the frames in @p native's context, or nullptr for a headless window.
	/// @param clock The clock; must outlive the window.
	/// @param title The title.
	/// @param width Width in window units.
	/// @param height Height in window units.
	Window(std::unique_ptr<platform::NativeWindow> native, std::unique_ptr<detail::Renderer> renderer,
	       const Clock& clock, std::string title, int width, int height);

	/// @brief Lays the tree out if anything changed; the root fills the window.
	void updateLayout();
	/// @brief Schedules a layout pass and a frame, for a panel that changed. Changes made by the layout pass itself
	/// don't schedule another one.
	void requestLayout() noexcept {
		if(!layingOut_) layoutDirty_ = redrawDirty_ = true;
	}
	/// @brief Schedules a frame without a layout pass.
	void requestFrame() noexcept { redrawDirty_ = true; }
	/// @brief What the renderer draws: the tree, the overlays and the tooltip, and the damage the window gathered.
	/// @return The frame.
	[[nodiscard]] detail::Frame frame() noexcept;
	/// @brief Forgets the damage gathered, once a frame has taken it.
	void clearDamage() noexcept {
		damage_.reset();
		damageAll_ = false;
	}
	/// @brief Lays out and draws the tree into the back buffer, in this window's context.
	/// @pre The window has a renderer.
	void draw();
	/// @brief Takes a new size from the OS: lays out again and fires onResize.
	/// @param width In window units.
	/// @param height In window units.
	void resized(int width, int height);
	/// @brief Draws a frame now, whether or not something changed (the OS asks during a live resize).
	void refresh();

	/// @brief Changes the hovered panel, sending hover events to it and to its ancestors that the pointer entered or
	/// left, and updating the cursor and the tooltip.
	/// @param panel The panel under the pointer, or nullptr.
	void setHovered(Panel* panel);
	/// @brief Lays out the overlays inside a subtree (after the tree), collecting them in drawing order.
	/// @param panel The subtree.
	void layoutOverlays(Panel& panel);
	/// @brief Lays out the focus ring around the focused panel, and lists what is drawn above the tree (drawn_).
	void layoutFocusRing();
	/// @brief Lays out one overlay next to its anchor.
	/// @param overlay The overlay.
	/// @param parent Its parent: the anchor when it names none.
	void layoutOverlay(Panel& overlay, const Panel& parent);
	/// @brief Starts the tooltip delay for the hovered panel or its nearest ancestor with a tooltip; hides any shown.
	void restartTooltip();
	/// @brief Runs the timers that are due.
	void runTimers();
	/// @brief Runs a menu item the user chose in the native menu bar, as input.
	/// @param run What it does (a copy is run: it may replace the menu bar).
	void runChosen(std::function<void()> run);
	/// @brief Drops the focus, hover and capture pointers to a panel being destroyed.
	/// @param panel The panel.
	void forget(const Panel& panel) noexcept;
	/// @brief Runs deferred tasks until none are left.
	void runDeferred();
	/// @brief Shows the cursor of the grabbing or hovered panel.
	void updateCursor();

	// Declaration order matters: the renderer must be destroyed before the OS window (and its GL context)
	std::unique_ptr<platform::NativeWindow> native_; ///< OS window and OpenGL context; nullptr when headless.
	std::unique_ptr<detail::Renderer> renderer_;     ///< Draws the tree; nullptr when headless.
	const Clock* clock_;                             ///< See clock(); never null.
	std::string clipboard_;                          ///< A headless window's clipboard.
	bool visible_ = true;                            ///< Whether a headless window counts as shown.
	float scale_ = 1.0f;                             ///< See contentScale().
	ColorScheme colorScheme_ = ColorScheme::Light;   ///< See colorScheme().
	bool reducedMotion_ = false;                     ///< See reducedMotion().
	std::vector<MenuItem> menus_;                    ///< See menuBar().
	bool nativeMenuBar_ = false;                     ///< See nativeMenuBar().
	FileDialogHandler fileDialogHandler_;            ///< See setFileDialogHandler().

	std::string title_;                ///< See title().
	int width_ = 0, height_ = 0;       ///< Size, in window units.
	int minWidth_ = 0, minHeight_ = 0; ///< Minimum size.
	Vec2 cursor_;                      ///< Last pointer position.
	bool layoutDirty_ = true;          ///< Layout needed before the next frame.
	bool redrawDirty_ = true;          ///< render() must draw.
	bool layingOut_ = false;           ///< A layout pass is running.
	bool layoutAll_ = true;            ///< The next pass lays out every panel (see invalidate()).
	std::uint64_t measureGeneration_ = 0;
	std::uint64_t treeGeneration_ = 1;  ///< Moves on when panels come, go, move or change id (Panel::treeChanged()).
	std::uint64_t paintGeneration_ = 0; ///< See detail::Frame::paintGeneration.
	std::optional<Rect> damage_;        ///< Where panels were that are gone, since the last frame.
	bool damageAll_ = true; ///< The next frame draws everything (see detail::Frame::damageAll). ///< Panels' measured
	                        ///< sizes from an older generation are measured again.
	std::uint64_t layoutPass_ = 0; ///< Counts layout passes (Panel::layoutPass_ says which one it was in).
	bool forwardAnchor_ = false;   ///< A panel anchored to one not laid out yet in this pass.
	int routing_ = 0;              ///< Depth of event delivery in progress.
	std::vector<std::function<void()>> deferred_; ///< See defer().

	std::shared_ptr<const Font> defaultFont_; ///< See defaultFont().
	Panel* focused_ = nullptr;                ///< See focusedPanel().
	Panel* hovered_ = nullptr;                ///< See hoveredPanel().
	Panel* grabbed_ = nullptr;                ///< See mouseGrabber().

	/// @brief A task waiting for its time.
	struct Timer {
		std::uint64_t id;           ///< See setTimer().
		double due;                 ///< clock().seconds() when it runs.
		std::function<void()> task; ///< The work.
	};
	std::vector<Timer> timers_;               ///< See setTimer().
	std::vector<Timer>* dueTimers_ = nullptr; ///< The due timers runTimers() is running, for cancelTimer().
	std::uint64_t nextTimerId_ = 1;           ///< The next timer's id.

	std::vector<Panel*> overlays_;   ///< Overlays shown in the last layout, in drawing order.
	std::vector<Panel*> drawn_;      ///< What is drawn above the tree: the overlays, then the focus ring if shown.
	std::optional<FocusRing> focusRing_; ///< See focusRing().
	std::unique_ptr<Panel> ring_;        ///< The focus ring's panel, created on first use.
	bool keyInput_ = false;              ///< The latest input was a key, not a mouse press.
	bool focusVisible_ = false;          ///< See focusVisible().
	std::unique_ptr<Label> tooltip_; ///< The tooltip, created on first use.
	bool tooltipShown_ = false;      ///< The tooltip is on screen.
	std::uint64_t tooltipTimer_ = 0; ///< The delay before showing it, if running.

	Cursor currentCursor_ = Cursor::Arrow; ///< Shape shown now.
	Panel root_;                           ///< Fills the window.
};

} // namespace affresco
