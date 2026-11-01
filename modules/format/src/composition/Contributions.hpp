#pragma once

/// @file
/// @brief Contributions (rfcs/0001-contributions.md): commands, keybindings and the `context` keys they are judged by,
/// offered to expressions as the `commands`, `keybindings` and `context` variables.

#include "NodeKeys.hpp"

#include <string>
#include <string_view>

namespace affresco::format {

struct Engine;

/// The keys of a command, an action written as an object that describes itself.
inline constexpr std::array<std::string_view, 6> commandKeys = {"title",      "category",    "icon",
                                                                "enablement", "description", "do"};

/// @brief Tells whether an action is a command: an object with "do" or a key only commands have, and only command
/// keys.
/// @param action The action, as written in "actions".
/// @return true if it is one.
[[nodiscard]] bool isCommand(const json& action);

/// @brief Checks an action written as an object: it is a command or steps, never both.
/// @param action The action.
/// @param where Its place, for errors.
/// @throws DocumentError If it has a command key ("title", "enablement", ...) and a key no command has.
void checkCommand(const json& action, const std::string& where);

/// @brief Tells whether a binding is an object `{when, do, with}`, not inline steps.
/// @param binding The binding.
/// @return true if it has "do" and only those keys.
[[nodiscard]] bool isBinding(const json& binding);

/// @brief Tells whether a key's handler is a list of bindings, tried in order, not a list of steps.
/// @param handler The handler.
/// @return true if it is a non-empty list of binding objects.
[[nodiscard]] bool isBindingList(const json& handler);

/// @brief Tells whether a command can run from a key, a menu or a palette: its "enablement" is true, or it has none.
/// Steps and actions that aren't commands always can.
/// @param engine The runtime.
/// @param name The action's name.
/// @return true if it is enabled; false if its "enablement" fails (which is reported).
[[nodiscard]] bool commandEnabled(Engine& engine, std::string_view name);

/// @brief Checks the "menus" section: `"<menu>": { "<item>": { run, with, submenu, group, order, when, title, icon } }`,
/// a "submenu" naming another menu.
/// @param section The section.
/// @param name "menus".
/// @throws DocumentError If an item is malformed.
void checkMenus(const json& section, const std::string& name);

/// @brief Adds the `context`, `commands`, `keybindings` and `menus` variables to an engine's document.
/// @param engine The runtime; outlives the variables.
void addContributionVariables(Engine& engine);

/// @brief A title or description a document declares outside its nodes (a command's, a menu item's, a setting's, the
/// window's): its text, evaluated when it holds an expression, so that `t()` can translate it.
/// @param engine The runtime.
/// @param owner The object that declares it.
/// @param key Its key there.
/// @param fallback What it is if the object has none.
/// @param where Its place, for errors.
/// @return The text; as written if it can't be evaluated, which is reported.
[[nodiscard]] std::string declaredText(Engine& engine, const json& owner, const char* key, const std::string& fallback,
                                       const std::string& where);

/// @brief Shows a bound "window.title" on the window, when it differs.
/// @param engine The runtime.
void showWindowTitle(Engine& engine);

/// @brief Shows the window's native menu bar ("window.menuBar": menus of "menus", resolved like the `menus` variable),
/// if it changed since the last call. Errors are reported, and leave the bar as it was.
/// @param engine The runtime.
void showMenuBar(Engine& engine);

/// @brief Removes the window's native menu bar, whose items call into the runtime.
/// @param engine The runtime.
void hideMenuBar(Engine& engine);

} // namespace affresco::format
