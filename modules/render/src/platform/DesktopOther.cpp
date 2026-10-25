/// @file
/// @brief The desktop helpers on Linux and the other systems: no native menu bar (it needs D-Bus menus, which most
/// desktops don't show), and a light colour scheme with full motion (the system's settings need the D-Bus settings
/// portal). Apps draw a menu bar of their own (Window::setMenuBar() returns false).

#include "platform/Desktop.hpp"

namespace affresco::platform::desktop {

ColorScheme colorScheme() { return ColorScheme::Light; }

bool reducedMotion() { return false; }

void watchColorScheme() {}

void keepOutOfSwitcher() {} // a hidden window has no taskbar entry

void setApplicationIcon(const Bitmap* /*icon*/) {} // each window has its own (glfwSetWindowIcon), or none (Wayland)

std::unique_ptr<MenuBar> createMenuBar(GLFWwindow* /*window*/, MenuChosen /*chosen*/) { return nullptr; }

} // namespace affresco::platform::desktop
