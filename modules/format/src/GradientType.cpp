/// @file
/// @brief The "gradient" node type: a GradientEditor, its colours, and its value as
/// { "kind", "angle", "stops": [{ "at", "color" }] }.

#include "CreativeValues.hpp"
#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/ColorPicker.hpp"
#include "affresco/render/GradientEditor.hpp"

#include <utility>

namespace affresco::format {

namespace {

/// @brief Sets an editor's font and colours (its popover's picker's too): those the node gives, the defaults for the
/// others.
/// @param panel The panel, a GradientEditor.
/// @param values Its font, textColor and accentColor, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyGradient(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* editor = dynamic_cast<GradientEditor*>(&panel);
	if(!editor) return;
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	const Color accent = colorOr("accentColor", colors::accent);
	editor->setAccent(accent);
	editor->picker().setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	editor->picker().setColors(colorOr("textColor", colors::black), accent);
}

/// @brief Shows an editor's gradient: its bound value, else "value" (the default gradient when neither is one).
/// @param panel The panel, a GradientEditor.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
/// @param lastText Not used.
void showGradient(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& lastText) {
	static_cast<void>(lastText);
	auto* editor = dynamic_cast<GradientEditor*>(&panel);
	if(!editor) return;
	const json& value = bound ? *bound : properties.contains("value") ? properties["value"] : emptyObject();
	editor->setGradient(gradientFromJson(value));
}

/// @brief Reports a gradient the user changed.
/// @param panel The panel, a GradientEditor.
/// @param changed Gets the value.
/// @return The connection.
Connection listenToGradient(Panel& panel, std::function<void(const json&)> changed) {
	auto* editor = dynamic_cast<GradientEditor*>(&panel);
	if(!editor) return 0;
	return editor->onGradientChanged.connect(
	    [changed = std::move(changed)](const Gradient& gradient) { changed(gradientToJson(gradient)); });
}

/// @brief Disconnects listenToGradient().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToGradient(Panel& panel, Connection connection) {
	if(auto* editor = dynamic_cast<GradientEditor*>(&panel)) editor->onGradientChanged.disconnect(connection);
}

} // namespace

void registerGradientType(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},
	                   {"value", ValueKind::Any},
	                   {"font", ValueKind::Font},
	                   {"textColor", ValueKind::Color},
	                   {"accentColor", ValueKind::Color}};
	set->apply = applyGradient;
	set->everyUpdate = true; // colours follow the theme
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const GradientEditor*>(&panel) != nullptr; };
	set->value = {"value", showGradient, listenToGradient, stopListeningToGradient};

	registry.add({"gradient", "A linear or radial gradient: colour stops on a bar, each edited in a colour picker",
	              [] { return std::make_unique<GradientEditor>(); }, set, false});
}

} // namespace affresco::format
