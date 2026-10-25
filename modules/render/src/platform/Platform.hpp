#pragma once

/// @file
/// @brief platform::Platform and platform::NativeWindow: what the render module asks of the windowing system (GLFW
/// today), so that Window and Application never call it themselves and a Window can run without one (headless).

#include "affresco/render/Events.hpp"
#include "affresco/render/Input.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco {

class Bitmap;
class Window;
struct MenuItem;
struct FileDialog;

namespace platform {

using ProcAddressLoader = void* (*)(const char* name); ///< Looks an OpenGL function up by name.

/// @brief A size in pixels of the framebuffer (on HiDPI screens, more than the window has units).
struct PixelSize {
	int width = 0;  ///< In pixels.
	int height = 0; ///< In pixels.
};

/// @brief An OS window with an OpenGL 4.1 core context, as one Window sees it.
///
/// It delivers the OS's input and size changes to the Window it was connected to, converted to the typed events of
/// affresco/render/Events.hpp, through the protected functions below (the Window's routing methods). Main thread only.
/// It is owned by its Window, which outlives every callback.
class NativeWindow {
public:
	virtual ~NativeWindow() = default;

	NativeWindow() = default;
	NativeWindow(const NativeWindow&) = delete;
	NativeWindow& operator=(const NativeWindow&) = delete;
	NativeWindow(NativeWindow&&) = delete;
	NativeWindow& operator=(NativeWindow&&) = delete;

	/// @brief Sends the OS's events to a window from now on.
	/// @param window The window that owns this one.
	virtual void connect(Window& window) = 0;

	/// @brief Sets the title.
	/// @param title The title.
	virtual void setTitle(const std::string& title) = 0;
	/// @brief Sets the icon (see Window::setIcon()); the pixels are copied.
	/// @param icon The image, or nullptr for the system's default.
	virtual void setIcon(const Bitmap* icon) = 0;
	/// @brief Resizes the OS window.
	/// @param width In window units.
	/// @param height In window units.
	virtual void setSize(int width, int height) = 0;
	/// @brief Sets how small the user can make the window.
	/// @param width In window units.
	/// @param height In window units.
	virtual void setMinSize(int width, int height) = 0;
	/// @brief Whether the window is shown.
	[[nodiscard]] virtual bool isVisible() const = 0;
	/// @brief Shows or hides it.
	/// @param visible true to show it.
	virtual void setVisible(bool visible) = 0;
	/// @brief Whether the user asked to close it.
	[[nodiscard]] virtual bool shouldClose() const = 0;

	/// @brief The system clipboard's text, as UTF-8.
	[[nodiscard]] virtual std::string clipboardText() const = 0;
	/// @brief Sets the system clipboard's text.
	/// @param utf8 UTF-8 text.
	virtual void setClipboardText(const std::string& utf8) = 0;
	/// @brief Shows a cursor shape over the window.
	/// @param cursor The shape (Arrow is the system default).
	virtual void setCursor(Cursor cursor) = 0;

	/// @brief The framebuffer's size.
	[[nodiscard]] virtual PixelSize framebufferSize() const = 0;
	/// @brief Makes the window's OpenGL context current, if it isn't.
	virtual void makeCurrent() = 0;
	/// @brief Shows what was drawn into the back buffer.
	virtual void present() = 0;
	/// @brief Looks the context's OpenGL functions up.
	[[nodiscard]] virtual ProcAddressLoader procAddressLoader() const = 0;
	/// @brief Wakes the event loop, so it works out again how long it may sleep.
	virtual void wake() = 0;
	/// @brief The windowing system's own handle (a GLFWwindow*), for Window::nativeHandle().
	[[nodiscard]] virtual void* handle() const noexcept = 0;
	/// @brief Pixels per window unit on the window's display now.
	[[nodiscard]] virtual float contentScale() const = 0;
	/// @brief Shows a native menu bar for this window, replacing the one shown; choosing an item calls chosen().
	/// @param menus The menus; empty removes the bar.
	/// @return false where native menus aren't supported.
	virtual bool setMenuBar(const std::vector<MenuItem>& menus) = 0;
	/// @brief Whether it can show file dialogs.
	[[nodiscard]] virtual bool fileDialogs() const = 0;
	/// @brief Shows a file dialog over the window and waits for the user.
	/// @param dialog What it asks for.
	/// @return The paths chosen; none if cancelled, or where there are no file dialogs.
	[[nodiscard]] virtual std::vector<std::string> chooseFiles(const FileDialog& dialog) = 0;

protected:
	/// @brief The native window a window owns.
	/// @param window A window with a native one.
	/// @return It.
	static NativeWindow& of(Window& window);
	/// @brief Tells a window it was resized.
	/// @param window The window.
	/// @param width In window units.
	/// @param height In window units.
	static void resized(Window& window, int width, int height);
	/// @brief Asks a window to draw again now (during a live resize, its event loop is blocked).
	/// @param window The window.
	static void refresh(Window& window);
	/// @brief Tells a window its framebuffer has another size (a resize, or a move to a screen of another scale): the
	/// next frame draws everything, but the layout, in window units, doesn't change for it.
	/// @param window The window.
	static void framebufferResized(Window& window);
	/// @brief Tells a window it moved to a display of another scale.
	/// @param window The window.
	/// @param scale Pixels per window unit.
	static void displayChanged(Window& window, float scale);
	/// @brief Delivers files dropped from the OS.
	/// @param window The window.
	/// @param event The files and where.
	static void dropped(Window& window, const DropEvent& event);
	/// @brief Runs a native menu item the user chose.
	/// @param window The window.
	/// @param run What the item does.
	static void chosen(Window& window, std::function<void()> run);
};

/// @brief The windowing system: starts it, opens windows, and waits for their events. One at a time, owned by the
/// Application; main thread only, except wake().
class Platform {
public:
	virtual ~Platform() = default;

	Platform() = default;
	Platform(const Platform&) = delete;
	Platform& operator=(const Platform&) = delete;
	Platform(Platform&&) = delete;
	Platform& operator=(Platform&&) = delete;

	/// @brief Opens a window with an OpenGL 4.1 core context, made current, with vsync.
	/// @param title The title.
	/// @param width In window units.
	/// @param height In window units.
	/// @return The window.
	/// @throws std::runtime_error If the window or its context can't be created.
	[[nodiscard]] virtual std::unique_ptr<NativeWindow> createWindow(const std::string& title, int width,
	                                                                 int height) = 0;
	/// @brief Opens a window whose OpenGL context shares its objects (textures, buffers, shaders) with another's.
	/// The default opens one that shares nothing, for windowing systems with a single window.
	/// @param title The title.
	/// @param width In window units.
	/// @param height In window units.
	/// @param sharing The other window's handle() (a GLFWwindow*).
	/// @return The window, its context current.
	/// @throws std::runtime_error If the window or its context can't be created.
	[[nodiscard]] virtual std::unique_ptr<NativeWindow> createSharedWindow(const std::string& title, int width,
	                                                                       int height, void* sharing) {
		static_cast<void>(sharing);
		return createWindow(title, width, height);
	}

	/// @brief Delivers the events that came, waiting for one first if there is none.
	/// @param timeoutSeconds The longest wait, or std::nullopt to wait until an event comes.
	virtual void waitEvents(std::optional<double> timeoutSeconds) = 0;

	/// @brief Wakes waitEvents() up. Safe to call from any thread.
	virtual void wake() = 0;

	/// @brief Whether the system shows light or dark windows now (Light where it can't be read).
	[[nodiscard]] virtual ColorScheme colorScheme() const = 0;
	/// @brief Whether the system asks for less motion now (false where it can't be read).
	[[nodiscard]] virtual bool reducedMotion() const = 0;
};

/// @brief Starts GLFW.
/// @param hiddenWindows Whether its windows open hidden (never on screen, drawn offscreen only).
/// @return The platform.
/// @throws std::runtime_error If GLFW can't start.
[[nodiscard]] std::unique_ptr<Platform> createGlfwPlatform(bool hiddenWindows = false);

} // namespace platform
} // namespace affresco
