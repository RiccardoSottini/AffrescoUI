#pragma once

/// @file
/// @brief The kits and themes AffrescoUI ships (modules/themes): its built-in components and its design tokens, as
/// JSON files built into the library, offered to documents through core's ComponentLibrary.

#include "affresco/core/ComponentLibrary.hpp"

namespace affresco::themes {

/// @brief The shipped kits and themes, read from their JSON files on first use (any thread) and kept until the
/// program ends.
/// @return The library.
/// @throws DocumentError If a file is malformed or two files define one name (a bug of this module, which its tests
/// catch).
[[nodiscard]] const ComponentLibrary& library();

/// @brief Makes them the library every document falls back on (installComponentLibrary()). Installing again changes
/// nothing.
void install();

} // namespace affresco::themes
