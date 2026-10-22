/// @file
/// @brief The accessibility tree (affresco/render/Accessibility.hpp): roles, names, values, states and actions read
/// from the panels, and the actions an assistive technology asks for.

#include "affresco/render/Accessibility.hpp"
#include "affresco/render/Controls.hpp"
#include "affresco/render/ImageView.hpp"
#include "affresco/render/Label.hpp"
#include "affresco/render/TextArea.hpp"
#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Window.hpp"

#include <algorithm>
#include <array>
#include <bit>

namespace affresco {

namespace {

/// @brief The roles a person presses, which also take the focus.
constexpr std::array<std::string_view, 11> pressableRoles{
    "button", "checkbox", "switch", "tab", "treeitem", "menuitem", "link", "option", "radio", "combobox", "listitem"};

/// @brief Whether a panel's role is one whose text is its value, not its name.
/// @param role The role.
/// @return Whether it is.
bool holdsValue(std::string_view role) { return role == "textbox" || role == "slider"; }

/// @brief Appends the texts of the shown labels under a panel, separated by spaces.
/// @param panel The panel.
/// @param out Where to append.
void appendTexts(const Panel& panel, std::string& out) {
	for(const auto& child : panel.children()) {
		if(!child->isShown()) continue;
		const auto* label = dynamic_cast<const Label*>(child.get());
		if(label && !dynamic_cast<const TextArea*>(label)) {
			const std::string text = label->text();
			if(!text.empty()) {
				if(!out.empty()) out += ' ';
				out += text;
			}
		}
		appendTexts(*child, out);
	}
}

/// @brief Cuts UTF-8 text to at most maxAccessibleText bytes, at a code point's start.
/// @param text The text.
/// @return It, or its beginning.
std::string capped(std::string text) {
	if(text.size() <= maxAccessibleText) return text;
	std::size_t end = maxAccessibleText;
	while(end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0u) == 0x80u) --end;
	text.resize(end);
	return text;
}

/// @brief The AccessibleAction flags a role and a state allow.
/// @param role The role.
/// @param node The element, with its range and state filled.
/// @return The flags.
std::uint8_t actionsOf(std::string_view role, const AccessibleNode& node) {
	if(node.state.disabled) return 0;
	unsigned flags = 0;
	if(std::ranges::find(pressableRoles, role) != pressableRoles.end())
		flags |= static_cast<unsigned>(AccessibleAction::Press) | static_cast<unsigned>(AccessibleAction::Focus);
	if(role == "textbox" || role == "slider") flags |= static_cast<unsigned>(AccessibleAction::Focus);
	if(node.range)
		flags |=
		    static_cast<unsigned>(AccessibleAction::Increment) | static_cast<unsigned>(AccessibleAction::Decrement);
	if(node.state.expanded)
		flags |= static_cast<unsigned>(*node.state.expanded ? AccessibleAction::Collapse : AccessibleAction::Expand);
	return static_cast<std::uint8_t>(flags);
}

/// @brief Fills a text field's value and selection.
/// @param panel The panel.
/// @param node Its element.
void readText(const Panel& panel, AccessibleNode& node) {
	if(const auto* area = dynamic_cast<const TextArea*>(&panel)) {
		node.value = capped(area->text());
		const auto [from, to] = area->selection();
		const auto length = static_cast<std::size_t>(
		    std::ranges::count_if(node.value, [](char c) { return (static_cast<unsigned char>(c) & 0xC0u) != 0x80u; }));
		node.selection = std::pair{std::min(from, length), std::min(to, length)};
	} else if(const auto* editor = dynamic_cast<const TextEditor*>(&panel)) {
		node.value = capped(editor->text());
	}
}

/// @brief Fills an element from its panel, its children aside.
/// @param panel The panel.
/// @param role Its role, not empty.
/// @return The element.
AccessibleNode element(const Panel& panel, std::string role) {
	AccessibleNode node;
	node.panel = const_cast<Panel*>(&panel); // the tree hands out panels to act on, as findByRole() does
	node.id = accessibleId(panel);
	node.name = accessibleName(panel);
	node.description = panel.accessibleDescription();
	node.state = panel.accessibleState();
	node.bounds = panel.bounds();
	node.focused = panel.window() && panel.window()->focusedPanel() == &panel;
	if(const auto* toggle = dynamic_cast<const Toggle*>(&panel)) node.checked = toggle->isChecked();
	if(const auto* slider = dynamic_cast<const Slider*>(&panel))
		node.range = AccessibleRange{slider->value(), slider->minimum(), slider->maximum(), slider->step()};
	if(role == "textbox") readText(panel, node);
	node.actions = actionsOf(role, node);
	node.role = std::move(role);
	return node;
}

/// @brief Adds the elements under a panel to a node, skipping panels without a role.
/// @param panel The panel.
/// @param node Where to add them.
/// @param count The elements made so far, at most maxAccessibleNodes.
void addChildren(const Panel& panel, AccessibleNode& node, std::size_t& count) {
	for(const auto& child : panel.children()) {
		if(!child->isShown()) continue;
		std::string role = accessibleRole(*child);
		if(role.empty()) {
			addChildren(*child, node, count);
			continue;
		}
		if(count >= maxAccessibleNodes) return;
		++count;
		AccessibleNode added = element(*child, std::move(role));
		addChildren(*child, added, count);
		node.children.push_back(std::move(added));
	}
}

/// @brief Appends the shown panels under a panel, itself included, that have a role and a name.
/// @param panel The panel.
/// @param role The role.
/// @param name The name, or empty for any.
/// @param out Where to append.
void find(Panel& panel, std::string_view role, std::string_view name, std::vector<Panel*>& out) {
	if(!panel.isShown()) return;
	if(accessibleRole(panel) == role && (name.empty() || accessibleName(panel) == name)) out.push_back(&panel);
	for(const auto& child : panel.children()) find(*child, role, name, out);
}

/// @brief Clicks a panel at its centre, as the pointer would: a move, a press and a release.
/// @param window Its window.
/// @param panel The panel.
void click(Window& window, const Panel& panel) {
	const Rect& area = panel.bounds();
	const Vec2 centre{(area.left + area.right) * 0.5f, (area.top + area.bottom) * 0.5f};
	window.routeMouseMove(centre);
	window.routeMouseButton({.button = MouseButton::Left, .action = Action::Press, .mods = {}, .position = centre});
	window.routeMouseButton({.button = MouseButton::Left, .action = Action::Release, .mods = {}, .position = centre});
}

/// @brief Moves a slider by a step (a twentieth of its range when it has none), telling its listeners.
/// @param slider The slider.
/// @param up true to raise it.
void step(Slider& slider, bool up) {
	const double size = slider.step() > 0.0 ? slider.step() : (slider.maximum() - slider.minimum()) / 20.0;
	const double before = slider.value();
	slider.setValue(before + (up ? size : -size));
	if(slider.value() != before) slider.onValueChanged(slider.value());
}

} // namespace

std::string accessibleRole(const Panel& panel) {
	if(!panel.accessibleRole().empty()) return panel.accessibleRole();
	if(dynamic_cast<const TextArea*>(&panel) || dynamic_cast<const TextEditor*>(&panel)) return "textbox";
	if(dynamic_cast<const Label*>(&panel)) return "text";
	if(dynamic_cast<const Toggle*>(&panel)) return "checkbox";
	if(dynamic_cast<const Slider*>(&panel)) return "slider";
	if(dynamic_cast<const ImageView*>(&panel)) return "img";
	return {};
}

std::string accessibleName(const Panel& panel) {
	if(!panel.accessibleLabel().empty()) return panel.accessibleLabel();
	if(!holdsValue(accessibleRole(panel))) {
		std::string name;
		if(const auto* label = dynamic_cast<const Label*>(&panel)) name = label->text();
		if(name.empty()) appendTexts(panel, name);
		if(!name.empty()) return name;
	}
	return panel.tooltip();
}

AccessibleNode accessibilityTree(Panel& root) {
	AccessibleNode node;
	std::size_t count = 0;
	if(root.isShown()) {
		std::string role = accessibleRole(root);
		if(!role.empty()) {
			node = element(root, std::move(role));
			++count;
		}
		addChildren(root, node, count);
	}
	return node;
}

std::uint64_t accessibleId(const Panel& panel) noexcept {
	// A panel's address: unique while it lives, and aligned, so never 1 (the window) nor odd (text runs use id + 1)
	return std::bit_cast<std::uintptr_t>(&panel);
}

Panel* findByAccessibleId(Panel& root, std::uint64_t id) noexcept {
	if(accessibleId(root) == id) return root.isShown() ? &root : nullptr;
	for(const auto& child : root.children())
		if(Panel* found = findByAccessibleId(*child, id)) return found;
	return nullptr;
}

bool performAccessibleAction(Window& window, Panel& panel, AccessibleAction action) {
	const std::string role = accessibleRole(panel);
	AccessibleNode node = element(panel, role);
	if(!(node.actions & static_cast<std::uint8_t>(action))) return false;
	auto* slider = dynamic_cast<Slider*>(&panel);
	switch(action) {
	case AccessibleAction::Focus:
		window.setFocus(&panel);
		break;
	case AccessibleAction::Increment:
	case AccessibleAction::Decrement:
		if(slider) step(*slider, action == AccessibleAction::Increment);
		break;
	default: // pressing, expanding and collapsing are what a click does
		if(auto* toggle = dynamic_cast<Toggle*>(&panel)) {
			toggle->setChecked(!toggle->isChecked());
			toggle->onToggled(toggle->isChecked());
		} else {
			click(window, panel);
		}
	}
	return true;
}

std::vector<Panel*> findByRole(Panel& root, std::string_view role, std::string_view name) {
	std::vector<Panel*> out;
	find(root, role, name, out);
	return out;
}

} // namespace affresco
