/// @file
/// @brief The kit's design tokens ("kit.surface", "kit.text", "kit.radius", ...): the colours and sizes every built-in
/// component takes by default, which a document changes by defining tokens of the same names.

#pragma once

#include <affresco/core/Json.hpp>

#include <string>
#include <string_view>

namespace affresco::format {

/// @brief The kit's default tokens, in the shape of a "tokens" section.
/// @param themed true for a light and a dark variant of each colour ({"dark": ..., "light": ...}, dark the base),
/// false for the dark values alone (the look the built-ins had before the kit, in every theme).
/// @return The tokens.
[[nodiscard]] const json& kitTokens(bool themed);

/// @brief Tells whether a name is one of the kit's tokens.
/// @param name The name, without "@".
/// @return true for "kit.surface" and the others.
[[nodiscard]] bool isKitToken(std::string_view name) noexcept;

/// @brief Tells whether a document opts in to the kit's light variants: its "tokens" or "colors" define a "kit." name.
/// @param document The document.
/// @return true if it does.
[[nodiscard]] bool followsKitThemes(const json& document);

/// @brief The kit's tokens as a SCREEN_FORMAT.md table: name, dark and light values, and what each is for.
/// @return The table, its header first.
[[nodiscard]] std::string kitTokenTable();

} // namespace affresco::format
