#pragma once

/// @file
/// @brief Forge's file icon theme: which Lucide icon, and colour, each file and folder of the explorer shows. The theme
/// is data in the screen (`state.iconTheme`), so a person or an agent changes it like any other value; this applies
/// it to the tree items of FileIndex.hpp, which an expression can't do because the tree nests.

#include <affresco/core/Json.hpp>

namespace forge {

/// @brief Gives each tree item an `icon` and an `iconColor` from an icon theme.
/// @param items Tree items (FileIndex.hpp): a folder's id ends with "/" and it has `children`.
/// @param theme `{ "file": look, "folder": look, "extensions": { "cpp": look }, "names": { "CMakeLists.txt": look } }`,
/// each look `{ "icon": "file-code", "color": "#519aba" }`; a file's name wins over its extension, which wins over
/// `file`. Untrusted: parts that aren't of these shapes, and colours that aren't hex, are left out.
void applyIconTheme(affresco::json& items, const affresco::json& theme);

} // namespace forge
