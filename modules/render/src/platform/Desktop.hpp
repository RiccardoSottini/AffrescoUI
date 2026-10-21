#pragma once

/// @file
/// @brief What GLFW doesn't do, per OS: the system's colour scheme and motion setting, a window's native menu bar, and
/// file dialogs. Each OS has its own file (DesktopMac.mm, DesktopWin32.cpp, DesktopOther.cpp), and the file dialogs one
/// for all (FileDialogs.cpp); only the GLFW platform uses them.

#include "affresco/render/Events.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace affresco {

struct MenuItem;
struct FileDialog;

namespace platform::desktop {

/// @brief Whether the system shows light or dark windows now.
/// @return Light where it can't be read.
[[nodiscard]] ColorScheme colorScheme();

/// @brief Whether the system asks for less motion now (macOS "Reduce motion", Windows "Animation effects" off).
/// @return false where it can't be read.
[[nodiscard]] bool reducedMotion();

/// @brief Makes a change of the colour scheme or of the motion setting wake the event loop (glfwPostEmptyEvent()),
/// where the OS announces it.
/// Called once, after GLFW started.
void watchColorScheme();

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
