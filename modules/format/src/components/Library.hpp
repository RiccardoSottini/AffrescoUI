/// @file
/// @brief The components and design tokens a document uses without defining them: those of the installed
/// ComponentLibrary (affresco/core/ComponentLibrary.hpp; modules/themes in a stock program). The format reads them here
/// and never knows which kits or themes they are.

#pragma once

#include <affresco/core/Json.hpp>

#include <set>
#include <string>
#include <string_view>

namespace affresco::format {

/// @brief A component of the installed library, for a "use" the document's "components" doesn't define.
/// @param name Its name.
/// @return Its definition ({params, node, runsActions}), or nullptr if there is none by that name (or no library).
[[nodiscard]] const json* libraryComponent(std::string_view name);

/// @brief The installed library's tokens, in the shape of a "tokens" section: a document's defaults.
/// @param themed true for a light and a dark variant of each colour, false for the base (dark) values alone.
/// @return The tokens (empty without a library).
[[nodiscard]] const json& libraryTokens(bool themed);

/// @brief Tells whether a name is one of the installed library's tokens.
/// @param name The name, without "@".
/// @return true if it is.
[[nodiscard]] bool isLibraryToken(std::string_view name);

/// @brief Tells whether a document opts in to the library's light variants: its "tokens" or "colors" define a name in
/// the namespace of one of the library's tokens (the part up to its first dot: "kit." for "kit.surface").
/// @param document The document.
/// @return true if it does.
[[nodiscard]] bool followsLibraryThemes(const json& document);

/// @brief The library's tokens as a SCREEN_FORMAT.md table: name, dark and light values, and what each is for.
/// @return The table, its header first.
[[nodiscard]] std::string libraryTokenTable();

/// @brief The names of the library's components that run the actions their parameters name (their "runsActions").
/// @return Them.
[[nodiscard]] std::set<std::string> actionRunningLibraryComponents();

/// @brief Whether a use of a component that declares "runsActions" may run the actions it is given.
/// @param definition The component's definition.
/// @param with The use's "with" (anything but an object passes nothing).
/// @param computes Whether a value computes anything (a "{{...}}" string at any depth).
/// @return true if its "runsActions" is true, or names a parameter the use computes; false if it declares none.
[[nodiscard]] bool declaredToRunActions(const json& definition, const json& with, bool (*computes)(const json&));

} // namespace affresco::format
