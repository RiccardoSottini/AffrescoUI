/// @file
/// @brief The built-in components (menu, menuBar, contextMenu, dialog, toast, hoverCard, settingsPage, quickPick, and
/// the desktop kit's bars, overlays and controls), used like a document's own.

#pragma once

#include <affresco/core/Json.hpp>

#include <string_view>

namespace affresco::format {

/// @brief A built-in component, for a "use" the document's "components" doesn't define.
/// @param name Its name.
/// @return Its definition ({params, node}), or nullptr if there is none by that name.
[[nodiscard]] const json* builtInComponent(std::string_view name);

/// @brief The form components (BuiltInFormComponents.cpp): comboBox and passwordMeter, as one JSON object by name.
/// @return Its text.
[[nodiscard]] std::string_view builtInFormComponents() noexcept;

/// @brief The desktop kit's bars (KitBars.cpp): toolbar and statusBar, as one JSON object by name.
/// @return Its text.
[[nodiscard]] std::string_view builtInKitBars() noexcept;

/// @brief The desktop kit's overlays (KitOverlays.cpp): popover and toasts, as one JSON object by name.
/// @return Its text.
[[nodiscard]] std::string_view builtInKitOverlays() noexcept;

/// @brief The desktop kit's controls (KitControls.cpp): segmentedControl, searchField, breadcrumbs and tabStrip, as one
/// JSON object by name.
/// @return Its text.
[[nodiscard]] std::string_view builtInKitControls() noexcept;

/// @brief The desktop kit's dock (KitDock.cpp): panels in tabbed areas, as one JSON object by name.
/// @return Its text.
[[nodiscard]] std::string_view builtInKitDock() noexcept;

} // namespace affresco::format
