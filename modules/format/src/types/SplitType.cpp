/// @file
/// @brief The "split" node type: a Split, its axis, minimum sizes and sashes, and its sizes as its value.

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Split.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

constexpr std::size_t maxPanes = 1000;    ///< The most sizes a split reads (untrusted input).
constexpr double maxPixels = 1'000'000.0; ///< The largest size or sash a split takes.

/// @brief Reads a list of pixel sizes, or one number for every pane.
/// @param value An array of numbers, or a number.
/// @param path Its place, for errors.
/// @return The sizes, each within 0 to maxPixels (at most maxPanes).
std::vector<float> pixels(const json& value, const std::string& path) {
	std::vector<float> sizes;
	if(value.is_number()) {
		sizes.push_back(static_cast<float>(std::clamp(static_cast<double>(number(value, path)), 0.0, maxPixels)));
		return sizes;
	}
	if(!value.is_array()) fail(path, "expected a list of numbers");
	for(std::size_t i = 0; i < value.size() && i < maxPanes; ++i) {
		const double size = number(value[i], concatenate(path, "[", std::to_string(i) + "]"));
		sizes.push_back(static_cast<float>(std::clamp(size, 0.0, maxPixels)));
	}
	return sizes;
}

/// @brief Sets a split's axis, default and minimum sizes and its sashes: those the node gives, the defaults for the
/// others.
/// @param panel The panel, a Split.
/// @param values Its direction, defaultSizes, minSize, sashSize, sashColor and sashActiveColor, if given.
/// @param reader Reads colours.
/// @param path The node's place, for errors.
void applySplit(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* split = dynamic_cast<Split*>(&panel);
	if(!split) return;
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	const auto listOr = [&](const char* key) {
		return values.contains(key) ? pixels(values[key], path + "." + key) : std::vector<float>{};
	};
	const std::string direction =
	    values.contains("direction") ? string(values["direction"], path + ".direction") : "row";
	if(direction != "row" && direction != "column") fail(path + ".direction", R"(expected "row" or "column")");
	split->setDirection(direction == "row" ? Split::Direction::Row : Split::Direction::Column);
	split->setDefaultSizes(listOr("defaultSizes"));
	split->setMinimumSizes(listOr("minSize"));
	const double sash = values.contains("sashSize") ? number(values["sashSize"], path + ".sashSize") : 4.0;
	split->setSash(static_cast<float>(std::clamp(sash, 0.0, 64.0)), colorOr("sashColor", colors::transparent),
	               colorOr("sashActiveColor", colors::accent));
}

/// @brief Shows a split's sizes: its bound value, else "value"; nothing (or not a list) keeps the defaults.
/// @param panel The panel, a Split.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
/// @param lastText Not used: a split shows sizes.
void showSizes(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& lastText) {
	static_cast<void>(lastText);
	auto* split = dynamic_cast<Split*>(&panel);
	if(!split) return;
	const json& value = bound ? *bound : properties.contains("value") ? properties["value"] : emptyObject();
	std::vector<float> sizes;
	if(value.is_array())
		for(std::size_t i = 0; i < value.size() && i < maxPanes; ++i)
			sizes.push_back(
			    value[i].is_number() ? static_cast<float>(std::clamp(value[i].get<double>(), 0.0, maxPixels)) : 0.0f);
	split->setSizes(std::move(sizes));
}

/// @brief Reports sizes the user picked, as whole pixels.
/// @param panel The panel, a Split.
/// @param changed Gets the sizes.
/// @return The connection.
Connection listenToSplit(Panel& panel, std::function<void(const json&)> changed) {
	auto* split = dynamic_cast<Split*>(&panel);
	if(!split) return 0;
	return split->onSizesChanged.connect([changed = std::move(changed)](const std::vector<float>& sizes) {
		json value = json::array();
		for(const float size : sizes) value.push_back(static_cast<std::int64_t>(std::lround(size)));
		changed(value);
	});
}

/// @brief Disconnects listenToSplit().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToSplit(Panel& panel, Connection connection) {
	if(auto* split = dynamic_cast<Split*>(&panel)) split->onSizesChanged.disconnect(connection);
}

} // namespace

void registerSplitType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},  {"value", ValueKind::Any},
	                   {"direction", ValueKind::Text},  {"defaultSizes", ValueKind::Any},
	                   {"minSize", ValueKind::Any},     {"sashSize", ValueKind::Number},
	                   {"sashColor", ValueKind::Color}, {"sashActiveColor", ValueKind::Color}};
	set->apply = applySplit;
	set->everyUpdate = true; // colours follow the theme
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const Split*>(&panel) != nullptr; };
	set->value = {"value", showSizes, listenToSplit, stopListeningToSplit};

	registry.add({"split", "Panes side by side or stacked, resized by dragging the sashes between them",
	              [] { return std::make_unique<Split>(); }, set, false});
}

} // namespace affresco::format
