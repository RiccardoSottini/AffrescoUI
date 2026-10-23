#pragma once

/// @file
/// @brief The accessibility tree: the panels that mean something to a person (a role and a name, values, states and
/// actions), the way assistive tools see them, and finding them by role and name (like Testing Library's getByRole).
/// The window hands this tree to the platform's API (modules/render/src/platform/Accessibility.hpp).

#include "affresco/render/Panel.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

/// @brief What an assistive technology can do to an element, as bit flags (AccessibleNode::actions).
enum class AccessibleAction : std::uint8_t {
	Press = 1,     ///< Click it (a button, a tab, a checkbox).
	Focus = 2,     ///< Give it the keyboard focus.
	Increment = 4, ///< Raise its value by a step (a slider).
	Decrement = 8, ///< Lower its value by a step.
	Expand = 16,   ///< Expand it (a collapsed tree item).
	Collapse = 32, ///< Collapse it.
};

/// @brief A slider's value and range.
struct AccessibleRange {
	double value = 0.0;   ///< The value.
	double minimum = 0.0; ///< The lowest value.
	double maximum = 0.0; ///< The highest value.
	double step = 0.0;    ///< The step; 0 for none.

	/// @brief Equal when every field is.
	bool operator==(const AccessibleRange&) const = default;
};

/// @brief One element of the accessibility tree.
struct AccessibleNode {
	Panel* panel = nullptr;               ///< The panel; nullptr for the root of a tree whose root has no role.
	std::uint64_t id = 0;                 ///< Stays the same while the panel lives (accessibleId()).
	std::string role;                     ///< Its role (accessibleRole()).
	std::string name;                     ///< Its name (accessibleName()).
	std::string description;              ///< Its description (Panel::accessibleDescription()).
	std::string value;                    ///< A text field's text, at most maxAccessibleText bytes.
	std::optional<AccessibleRange> range; ///< A slider's value and range.
	std::optional<bool> checked;          ///< A checkbox's or a switch's state.
	AccessibleState state;                ///< Expanded, selected, disabled, live (Panel::accessibleState()).
	/// A text field's selection, from and to, in code points of its value; the caret when they are equal.
	std::optional<std::pair<std::size_t, std::size_t>> selection;
	Rect bounds;                          ///< Where it is, in window units (the visible part).
	std::uint8_t actions = 0;             ///< The AccessibleAction flags it supports.
	bool focused = false;                 ///< It has the window's focus.
	std::vector<AccessibleNode> children; ///< The elements inside it.
};

/// @brief The longest text field value the tree carries, in bytes: longer text is cut (an editor's whole file isn't
/// sent on each keystroke).
inline constexpr std::size_t maxAccessibleText = 16 * 1024;

/// @brief The most elements accessibilityTree() returns: past it, the rest are left out.
inline constexpr std::size_t maxAccessibleNodes = 20'000;

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

/// @brief A panel's id in the tree: the same while it lives, never 0 or 1 (the window's own element).
/// @param panel The panel.
/// @return The id.
[[nodiscard]] std::uint64_t accessibleId(const Panel& panel) noexcept;

/// @brief The panel shown under a root that has an id.
/// @param root Where to look.
/// @param id Its accessibleId().
/// @return The panel, or nullptr if none has it (it was destroyed since the tree was read).
[[nodiscard]] Panel* findByAccessibleId(Panel& root, std::uint64_t id) noexcept;

/// @brief Does what an assistive technology asked: presses a button (a click at its centre, or flipping a toggle),
/// focuses, steps a slider (telling its listeners), expands or collapses (a click).
/// @param window The panel's window, which routes the click.
/// @param panel The panel.
/// @param action The action.
/// @return Whether the panel supports it.
bool performAccessibleAction(Window& window, Panel& panel, AccessibleAction action);

/// @brief The shown panels with a role, and a name if given, depth first.
/// @param root Where to look.
/// @param role The role.
/// @param name The exact name, or empty for any.
/// @return The matching panels, in tree order.
[[nodiscard]] std::vector<Panel*> findByRole(Panel& root, std::string_view role, std::string_view name = {});

} // namespace affresco
