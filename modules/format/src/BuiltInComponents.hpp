/// @file
/// @brief The built-in components (menu, menuBar, contextMenu, dialog, toast, hoverCard), used like a document's own.

#pragma once

#include <affresco/core/Json.hpp>

#include <string_view>

namespace affresco::format {

/// @brief A built-in component, for a "use" the document's "components" doesn't define.
/// @param name Its name.
/// @return Its definition ({params, node}), or nullptr if there is none by that name.
[[nodiscard]] const json* builtInComponent(std::string_view name);

} // namespace affresco::format
