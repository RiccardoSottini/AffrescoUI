/// @file
/// @brief The "slider" node type: a Slider, its range and colours, and its value.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Controls.hpp"

#include <cmath>
#include <cstdint>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Sets a slider's range and colours: those the node gives, the defaults for the others.
/// @param panel The panel, a Slider.
/// @param values Its min, max, step, trackColor, activeColor and knobColor, if given.
/// @param reader Reads colours.
/// @param path The node's place, for errors.
void applySlider(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* slider = dynamic_cast<Slider*>(&panel);
	if(!slider) return;
	const auto numberOr = [&](const char* key, double fallback) {
		return values.contains(key) ? static_cast<double>(number(values[key], path + "." + key)) : fallback;
	};
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	slider->setRange(numberOr("min", 0.0), numberOr("max", 100.0), numberOr("step", 0.0));
	slider->setColors(colorOr("trackColor", colors::track), colorOr("activeColor", colors::accent),
	                  colorOr("knobColor", colors::white));
}

/// @brief Shows a slider's value: its bound value, else "value".
/// Its last text is not used: a slider shows a number.
/// @param panel The panel, a Slider.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
void showValue(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& /*lastText*/) {
	auto* slider = dynamic_cast<Slider*>(&panel);
	if(!slider) return;
	const json value = bound ? *bound : properties.value("value", json(0));
	slider->setValue(value.is_number() ? value.get<double>() : 0.0);
}

/// @brief Reports a value the user picked: whole numbers as integers.
/// @param panel The panel, a Slider.
/// @param changed Gets the value.
/// @return The connection.
Connection listenToSlider(Panel& panel, std::function<void(const json&)> changed) {
	auto* slider = dynamic_cast<Slider*>(&panel);
	if(!slider) return 0;
	return slider->onValueChanged.connect([changed = std::move(changed)](double value) {
		changed(std::trunc(value) == value ? json(static_cast<std::int64_t>(value)) : json(value));
	});
}

/// @brief Disconnects listenToSlider().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToSlider(Panel& panel, Connection connection) {
	if(auto* slider = dynamic_cast<Slider*>(&panel)) slider->onValueChanged.disconnect(connection);
}

} // namespace

void registerSliderType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},    {"value", ValueKind::Number},
	                   {"min", ValueKind::Number},        {"max", ValueKind::Number},
	                   {"step", ValueKind::Number},       {"trackColor", ValueKind::Color},
	                   {"activeColor", ValueKind::Color}, {"knobColor", ValueKind::Color}};
	set->apply = applySlider;
	set->everyUpdate = true; // colours follow the theme
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const Slider*>(&panel) != nullptr; };
	set->value = {"value", showValue, listenToSlider, stopListeningToSlider};

	registry.add({"slider", "A value in a range", [] { return std::make_unique<Slider>(); }, set, false});
}

} // namespace affresco::format
