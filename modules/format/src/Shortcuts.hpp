#pragma once

/// @file
/// @brief Key names and shortcuts, as documents write them ("cmd+shift+k") and as key events give them.

#include "affresco/render/Events.hpp"

#include <string>
#include <string_view>

namespace affresco::format {

/// @brief A key's name for shortcuts: "a", "7", "enter", "escape", "up", "f5", ...
/// @param key The key.
/// @return Its name, or "" for keys without one.
[[nodiscard]] std::string keyName(Key key);

/// @brief A key press as a shortcut, modifiers in a fixed order: "cmd+shift+k".
/// @param event The key event.
/// @return The shortcut, or "" for keys without a name (and lone modifiers).
[[nodiscard]] std::string shortcut(const KeyEvent& event);

/// @brief Writes a shortcut from the document the way shortcut() does: "Shift+Cmd+K" is "cmd+shift+k". "mod" is cmd
/// on macOS and ctrl elsewhere.
/// @param text The shortcut as written.
/// @return The normalised shortcut.
[[nodiscard]] std::string normalizeShortcut(std::string_view text);

} // namespace affresco::format
