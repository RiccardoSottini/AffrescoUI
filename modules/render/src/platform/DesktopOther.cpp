/// @file
/// @brief The desktop helpers on Linux and the other systems: no native menu bar (it needs D-Bus menus, which most
/// desktops don't show), and a light colour scheme (the system's scheme needs the D-Bus settings portal). Apps draw
/// a menu bar of their own (Window::setMenuBar() returns false).

#include "platform/Desktop.hpp"

namespace affresco::platform::desktop {

ColorScheme colorScheme() { return ColorScheme::Light; }

void watchColorScheme() {}

std::unique_ptr<MenuBar> createMenuBar(GLFWwindow* /*window*/, MenuChosen /*chosen*/) { return nullptr; }

} // namespace affresco::platform::desktop
