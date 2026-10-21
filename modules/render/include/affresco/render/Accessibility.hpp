#pragma once

/// @file
/// @brief The accessibility tree: the panels that mean something to a person (a role and a name), the way assistive
/// tools see them, and finding them by role and name (like Testing Library's getByRole).

#include "affresco/render/Panel.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief One element of the accessibility tree.
struct AccessibleNode {
	Panel* panel = nullptr;               ///< The panel; nullptr for the root of a tree whose root has no role.
	std::string role;                     ///< Its role (accessibleRole()).
	std::string name;                     ///< Its name (accessibleName()).
	std::string description;              ///< Its description (Panel::accessibleDescription()).
	bool focused = false;                 ///< It has the window's focus.
	std::vector<AccessibleNode> children; ///< The elements inside it.
};

/// @brief A panel's role: the one it was given (Panel::setAccessibleRole()), or the one its widget implies: "textbox"
/// for a TextArea or a TextEditor, "text" for a Label, "checkbox" for a Toggle, "slider" for a Slider, "img" for an
/// ImageView; empty for anything else.
/// @param panel The panel.
/// @return The role, or empty if it has none.
[[nodiscard]] std::string accessibleRole(const Panel& panel);

/// @brief A panel's name: its label (Panel::setAccessibleLabel()), else its own text (a Label's), else the texts of
/// the labels inside it, joined by spaces, else its tooltip. A textbox or a slider takes no name from its text, which
/// is its value: its label, else its tooltip.
/// @param panel The panel.
/// @return The name, or empty.
[[nodiscard]] std::string accessibleName(const Panel& panel);

/// @brief The accessibility tree of the panels shown under a root: panels with a role are elements, others are
/// skipped (their children take their place); hidden panels and what is inside them are left out.
/// @param root The root, usually Window's.
/// @return The root's element, or an element without a panel holding the top elements if the root has no role.
[[nodiscard]] AccessibleNode accessibilityTree(Panel& root);

/// @brief The shown panels with a role, and a name if given, depth first.
/// @param root Where to look.
/// @param role The role.
/// @param name The exact name, or empty for any.
/// @return The matching panels, in tree order.
[[nodiscard]] std::vector<Panel*> findByRole(Panel& root, std::string_view role, std::string_view name = {});

} // namespace affresco
