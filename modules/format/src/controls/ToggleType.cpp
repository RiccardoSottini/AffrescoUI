/// @file
/// @brief The "toggle" and "checkbox" node types: a Toggle drawn as a switch or a checkbox, its colours, and its
/// checked value.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Controls.hpp"

#include <utility>

namespace affresco::format {

namespace {

/// @brief Sets a toggle's colours: those the node gives, the defaults for the others.
/// @param panel The panel, a Toggle.
/// @param values Its activeColor, trackColor and knobColor, if given.
/// @param reader Reads colours.
/// @param path The node's place, for errors.
void applyToggle(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* toggle = dynamic_cast<Toggle*>(&panel);
	if(!toggle) return;
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	toggle->setOnColor(colorOr("activeColor", colors::accent));
	toggle->setOffColor(colorOr("trackColor", colors::idle));
	toggle->setKnobColor(colorOr("knobColor", colors::white));
}

/// @brief Shows whether a toggle is checked: its bound value, else "checked".
/// @param panel The panel, a Toggle.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
/// @param lastText Not used: a toggle shows whether it is checked.
void showChecked(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& lastText) {
	static_cast<void>(lastText);
	if(auto* toggle = dynamic_cast<Toggle*>(&panel))
		toggle->setChecked(bound ? truthy(*bound) : truthy(properties.value("checked", json(false))));
}

/// @brief Reports a toggle the user switched.
/// @param panel The panel, a Toggle.
/// @param changed Gets whether it is checked.
/// @return The connection.
Connection listenToToggle(Panel& panel, std::function<void(const json&)> changed) {
	auto* toggle = dynamic_cast<Toggle*>(&panel);
	if(!toggle) return 0;
	return toggle->onToggled.connect([changed = std::move(changed)](bool checked) { changed(checked); });
}

/// @brief Disconnects listenToToggle().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToToggle(Panel& panel, Connection connection) {
	if(auto* toggle = dynamic_cast<Toggle*>(&panel)) toggle->onToggled.disconnect(connection);
}

} // namespace

void registerToggleTypes(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},
	                   {"checked", ValueKind::Boolean},
	                   {"activeColor", ValueKind::Color},
	                   {"trackColor", ValueKind::Color},
	                   {"knobColor", ValueKind::Color}};
	set->apply = applyToggle;
	set->everyUpdate = true; // colours follow the theme
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const Toggle*>(&panel) != nullptr; };
	set->value = {"checked", showChecked, listenToToggle, stopListeningToToggle};

	registry.add({"toggle", "A switch", [] { return std::make_unique<Toggle>(Toggle::Look::Switch); }, set, false});
	registry.add(
	    {"checkbox", "A checkbox", [] { return std::make_unique<Toggle>(Toggle::Look::Checkbox); }, set, false});
}

} // namespace affresco::format
