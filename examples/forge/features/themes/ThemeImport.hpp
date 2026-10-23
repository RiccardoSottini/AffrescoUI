#pragma once

/// @file
/// @brief Forge's colour theme importer: a theme file's `colors` become one more variant of Forge's colour
/// tokens. Plain C++ with no window, so the unit tests use it too.

#include <affresco/core/Json.hpp>

#include <string>
#include <string_view>

namespace forge {

/// @brief The theme id a name becomes: lower case letters, digits and dashes, at most 40 of them.
/// @param name A theme's name, e.g. "Solarized Dark".
/// @return Its id, e.g. "solarized-dark"; "imported" if nothing is left.
std::string themeId(std::string_view name);

/// @brief Reads a colour theme (JSON with comments and trailing commas) into Forge's colour tokens.
///
/// Each Forge token is read from the theme's `colors` by a list of theme keys, the first one set winning, else from
/// the theme's `type` ("dark", "light" or "hc"): a theme that sets only a few colours still gives a whole variant.
/// Colours are kept as `#rrggbb` or `#rrggbbaa`; anything else is skipped. `include` and `tokenColors` are ignored.
/// @param text The theme file's text.
/// @param fallbackName The name to use if the theme has none, e.g. its file name.
/// @return `{ "id", "name", "type", "patch": [JSON Patch ops] }`: the ops add the id as a variant of each
/// `color.*` token (`/tokens/color.editor/<id>`), for a `patch` step.
/// @throws std::runtime_error If the text isn't a JSON object, even with its comments and trailing commas removed.
affresco::json importColorTheme(std::string_view text, std::string_view fallbackName);

} // namespace forge
