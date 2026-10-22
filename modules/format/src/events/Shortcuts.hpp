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

/// @brief Writes a chord, presses separated by spaces, each the way normalizeShortcut() does: "Mod+K  Mod + S" is
/// "cmd+k cmd+s" on macOS. A single press is a chord of one.
/// @param text The chord as written.
/// @return The normalised chord, its presses joined by one space.
[[nodiscard]] std::string normalizeChord(std::string_view text);

/// @brief A normalised chord as people read it: "⌘K ⌘S" on macOS, "Ctrl+K Ctrl+S" elsewhere.
/// @param chord The chord, as normalizeChord() writes it.
/// @return Its label.
[[nodiscard]] std::string keyLabel(std::string_view chord);

} // namespace affresco::format
