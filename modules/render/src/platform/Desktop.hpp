#pragma once

/// @file
/// @brief What GLFW doesn't do, per OS: the system's colour scheme and motion setting, a window's native menu bar, and
/// file dialogs. Each OS has its own file (DesktopMac.mm, DesktopWin32.cpp, DesktopOther.cpp), and the file dialogs one
/// for all (FileDialogs.cpp); only the GLFW platform uses them.

#include "affresco/render/Events.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace affresco {

class Bitmap;
struct MenuItem;
struct FileDialog;

namespace platform::desktop {

/// @brief Whether the system shows light or dark windows now.
/// @return Light where it can't be read.
[[nodiscard]] ColorScheme colorScheme();

/// @brief Whether the system asks for less motion now (macOS "Reduce motion", Windows "Animation effects" off).
/// @return false where it can't be read.
[[nodiscard]] bool reducedMotion();

/// @brief What watchColorScheme() set up: the OS stops announcing the changes to it once it is destroyed.
class AppearanceWatch {
public:
	AppearanceWatch() = default;
	virtual ~AppearanceWatch() = default;
	AppearanceWatch(const AppearanceWatch&) = delete;
	AppearanceWatch& operator=(const AppearanceWatch&) = delete;
	AppearanceWatch(AppearanceWatch&&) = delete;
	AppearanceWatch& operator=(AppearanceWatch&&) = delete;
};

/// @brief Calls a function on the main thread, inside the event loop's wait, whenever the colour scheme or the motion
/// setting changes, where the OS announces it.
/// Called once, after GLFW started.
/// @param changed What to call; it must wake the event loop (glfwPostEmptyEvent()).
/// @return The watch, which must go before GLFW stops; nullptr where the OS announces no change (colorScheme() and
/// reducedMotion() must then be read after every wake). Where they can't be read they never change: a watch that
/// calls nothing.
[[nodiscard]] std::unique_ptr<AppearanceWatch> watchColorScheme(std::function<void()> changed);

/// @brief Keeps an app whose windows all stay hidden out of the Dock and the app switcher (macOS: an accessory app
/// that never activates); nothing elsewhere, where a hidden window has no taskbar entry.
/// Called once, after GLFW started.
void keepOutOfSwitcher();

/// @brief Sets the app's icon where the system shows one icon per app (the Dock on macOS); nothing elsewhere, where
/// GLFW sets each window's.
/// @param icon The image, or nullptr for the app's own (its bundle's).
void setApplicationIcon(const Bitmap* icon);

/// @brief The image on the system clipboard, as the bytes of a PNG file, where the OS hands one over (macOS: a PNG or
/// TIFF on the general pasteboard); empty elsewhere, when it holds none, or when it is larger than @p most bytes.
/// @param most The most bytes taken.
/// @return The PNG's bytes, or empty.
[[nodiscard]] std::vector<std::uint8_t> clipboardImagePng(std::size_t most);

/// @brief Puts an image on the system clipboard, where the OS takes one (macOS, as PNG and TIFF); nothing elsewhere.
/// @param image The image.
/// @return Whether it is on the clipboard now.
bool setClipboardImage(const Bitmap& image);

/// @brief A window's native menu bar.
class MenuBar {
public:
	virtual ~MenuBar() = default;

	MenuBar() = default;
	MenuBar(const MenuBar&) = delete;
	MenuBar& operator=(const MenuBar&) = delete;
	MenuBar(MenuBar&&) = delete;
	MenuBar& operator=(MenuBar&&) = delete;

	/// @brief Replaces the menus shown.
	/// @param menus The menus; empty removes the bar.
	virtual void show(const std::vector<MenuItem>& menus) = 0;
	/// @brief The window got the focus: on macOS, where the bar belongs to the app, its menus are shown from now on.
	virtual void activate() = 0;
};

/// @brief Calls what a chosen item does (a copy of MenuItem::run).
using MenuChosen = std::function<void(std::function<void()> run)>;

/// @brief Creates a window's menu bar, empty.
/// @param window The GLFW window.
/// @param chosen Called when the user chooses an item.
/// @return The bar, or nullptr where native menus aren't supported.
[[nodiscard]] std::unique_ptr<MenuBar> createMenuBar(GLFWwindow* window, MenuChosen chosen);

/// @brief Whether this build shows file dialogs (not on Linux built without D-Bus).
[[nodiscard]] bool fileDialogs() noexcept;

/// @brief Shows a file dialog over a window and waits for the user (see Window::chooseFiles()).
/// @param window The GLFW window it belongs to.
/// @param dialog What it asks for.
/// @return The paths chosen (UTF-8), or none if the user cancelled, it failed, or there are no file dialogs.
[[nodiscard]] std::vector<std::string> chooseFiles(GLFWwindow* window, const FileDialog& dialog);

} // namespace platform::desktop
} // namespace affresco
