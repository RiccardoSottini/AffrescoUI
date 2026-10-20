/// @file
/// @brief The "icon" property of labels: a glyph named in the label's font or its fallbacks (e.g. Lucide's "search"),
/// drawn before the text in its colour.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Label.hpp"
#include "affresco/render/TextArea.hpp"

namespace affresco::format {

namespace {

/// @brief Sets a label's icon: the one the node names, else none.
/// @param panel The panel, a Label.
/// @param values Its "icon", if given.
/// @param reader Not used: the icon is looked up in the label's own font when it is laid out.
/// @param path The node's place, for errors.
void applyIcon(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	static_cast<void>(reader);
	auto* label = dynamic_cast<Label*>(&panel);
	if(!label) return;
	label->setIcon(values.contains("icon") ? string(values["icon"], path + ".icon") : std::string());
}

} // namespace

void registerIconProperty(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"icon", ValueKind::Text}};
	set->apply = applyIcon;
	set->everyUpdate = true; // setIcon() does nothing when the name is the same; it also clears an icon removed
	// Labels and the application's subclasses of them; not text inputs, whose text is what the person types
	set->appliesTo = [](const Panel& panel) {
		return dynamic_cast<const Label*>(&panel) != nullptr && dynamic_cast<const TextArea*>(&panel) == nullptr;
	};
	registry.addProperties(std::move(set));
}

} // namespace affresco::format
