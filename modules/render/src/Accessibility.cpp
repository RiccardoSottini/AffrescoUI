#include "affresco/render/Accessibility.hpp"
#include "affresco/render/Controls.hpp"
#include "affresco/render/ImageView.hpp"
#include "affresco/render/Label.hpp"
#include "affresco/render/TextArea.hpp"
#include "affresco/render/TextEditor.hpp"
#include "affresco/render/Window.hpp"

namespace affresco {

namespace {

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

/// @brief Adds the elements under a panel to a node, skipping panels without a role.
/// @param panel The panel.
/// @param node Where to add them.
void addChildren(const Panel& panel, AccessibleNode& node) {
	for(const auto& child : panel.children()) {
		if(!child->isShown()) continue;
		std::string role = accessibleRole(*child);
		if(role.empty()) {
			addChildren(*child, node);
			continue;
		}
		AccessibleNode element;
		element.panel = child.get();
		element.role = std::move(role);
		element.name = accessibleName(*child);
		element.description = child->accessibleDescription();
		element.focused = child->window() && child->window()->focusedPanel() == child.get();
		addChildren(*child, element);
		node.children.push_back(std::move(element));
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
	if(root.isShown()) {
		node.role = accessibleRole(root);
		if(!node.role.empty()) {
			node.panel = &root;
			node.name = accessibleName(root);
			node.description = root.accessibleDescription();
			node.focused = root.window() && root.window()->focusedPanel() == &root;
		}
		addChildren(root, node);
	}
	return node;
}

std::vector<Panel*> findByRole(Panel& root, std::string_view role, std::string_view name) {
	std::vector<Panel*> out;
	find(root, role, name, out);
	return out;
}

} // namespace affresco
