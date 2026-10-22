/// @file
/// @brief The "colorPicker" node type: a ColorPicker, its mode, parts, swatches and colours, and its value as
/// "#rrggbbaa".

#include "CreativeValues.hpp"
#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/ColorPicker.hpp"

#include <array>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Reads the swatches: colours or design tokens, at most ColorPicker::maxSwatches.
/// @param picker The picker.
/// @param given The list, if given.
/// @param reader Reads colours.
/// @param path The node's place, for errors.
void applySwatches(ColorPicker& picker, const json* given, const PropertyReader& reader, const std::string& path) {
	std::array<Color, ColorPicker::maxSwatches> swatches{};
	std::size_t count = 0;
	if(given) {
		if(!given->is_array()) fail(path + ".swatches", "expected a list of colours");
		for(; count < given->size() && count < swatches.size(); ++count)
			swatches[count] =
			    reader.color((*given)[count], concatenate(path, ".swatches[", std::to_string(count) + "]"));
	}
	picker.setSwatches(std::span<const Color>(swatches.data(), count));
}

/// @brief Sets a picker's mode, parts, swatches, font and colours: those the node gives, the defaults for the others.
/// @param panel The panel, a ColorPicker.
/// @param values Its mode, alpha, fields, swatches, font, textColor and accentColor, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyColorPicker(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* picker = dynamic_cast<ColorPicker*>(&panel);
	if(!picker) return;
	if(panel.accessibleRole().empty()) panel.setAccessibleRole("group"); // a composite control to assistive tools
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	const auto booleanOr = [&](const char* key, bool fallback) {
		return values.contains(key) ? boolean(values[key], path + "." + key) : fallback;
	};
	const std::string mode = values.contains("mode") ? string(values["mode"], path + ".mode") : "square";
	if(mode != "square" && mode != "wheel") fail(path + ".mode", R"(expected "square" or "wheel")");
	picker->setMode(mode == "wheel" ? ColorPicker::Mode::Wheel : ColorPicker::Mode::Square);
	picker->setShowsAlpha(booleanOr("alpha", true));
	picker->setShowsFields(booleanOr("fields", true));
	applySwatches(*picker, values.contains("swatches") ? &values["swatches"] : nullptr, reader, path);
	picker->setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	picker->setColors(colorOr("textColor", colors::black), colorOr("accentColor", colors::accent));
}

/// @brief Shows a picker's colour: its bound value, else "value"; a value that isn't a hex colour is ignored.
/// Its last text is not used: a picker shows a colour.
/// @param panel The panel, a ColorPicker.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
void showColor(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& /*lastText*/) {
	auto* picker = dynamic_cast<ColorPicker*>(&panel);
	if(!picker) return;
	const json& value = bound ? *bound : properties.contains("value") ? properties["value"] : emptyObject();
	if(const auto color = colorFromJson(value)) picker->setValue(*color);
}

/// @brief Reports a colour the user picked, as "#rrggbbaa".
/// @param panel The panel, a ColorPicker.
/// @param changed Gets the value.
/// @return The connection.
Connection listenToColorPicker(Panel& panel, std::function<void(const json&)> changed) {
	auto* picker = dynamic_cast<ColorPicker*>(&panel);
	if(!picker) return 0;
	return picker->onValueChanged.connect(
	    [changed = std::move(changed)](Color value) { changed(json(colorToJson(value))); });
}

/// @brief Disconnects listenToColorPicker().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToColorPicker(Panel& panel, Connection connection) {
	if(auto* picker = dynamic_cast<ColorPicker*>(&panel)) picker->onValueChanged.disconnect(connection);
}

} // namespace

void registerColorPickerType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {
	    {"bind", ValueKind::StatePath}, {"value", ValueKind::Color},     {"mode", ValueKind::Text},
	    {"alpha", ValueKind::Boolean},  {"fields", ValueKind::Boolean},  {"swatches", ValueKind::Any},
	    {"font", ValueKind::Font},      {"textColor", ValueKind::Color}, {"accentColor", ValueKind::Color}};
	set->apply = applyColorPicker;
	set->everyUpdate = true; // colours follow the theme
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const ColorPicker*>(&panel) != nullptr; };
	set->value = {"value", showColor, listenToColorPicker, stopListeningToColorPicker};

	registry.add({"colorPicker", "A colour: a square or a wheel, alpha, typed fields and swatches",
	              [] { return std::make_unique<ColorPicker>(); }, set, false});
}

} // namespace affresco::format
