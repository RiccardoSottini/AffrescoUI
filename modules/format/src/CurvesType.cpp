/// @file
/// @brief The "curves" node type: a CurvesEditor, its interpolation, channels, histogram and colours, and its value
/// as { "points": [[x, y], ...] } (with "red", "green" and "blue" lists when the channels show).

#include "CreativeValues.hpp"
#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/CurvesEditor.hpp"

#include <array>
#include <utility>

namespace affresco::format {

namespace {

/// The channels a curves value holds, in order.
constexpr std::array<CurveChannel, curveChannelCount> channels = {CurveChannel::Rgb, CurveChannel::Red,
                                                                  CurveChannel::Green, CurveChannel::Blue};

/// @brief Reads the histogram: a list of counts, at most maxHistogramBins (others are 0).
/// @param editor The editor.
/// @param given The list, if given.
/// @param path The node's place, for errors.
void applyHistogram(CurvesEditor& editor, const json* given, const std::string& path) {
	std::array<double, maxHistogramBins> bins{};
	std::size_t count = 0;
	if(given && !given->is_null()) {
		if(!given->is_array()) fail(path + ".histogram", "expected a list of numbers");
		for(; count < given->size() && count < bins.size(); ++count) {
			const json& bin = (*given)[count];
			bins[count] = bin.is_number() ? bin.get<double>() : 0.0;
		}
	}
	editor.setHistogram(std::span<const double>(bins.data(), count));
}

/// @brief Sets an editor's interpolation, channels, histogram, font and colours: those the node gives, the defaults
/// for the others.
/// @param panel The panel, a CurvesEditor.
/// @param values Its interpolation, channels, histogram, font, textColor and accentColor, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyCurves(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* editor = dynamic_cast<CurvesEditor*>(&panel);
	if(!editor) return;
	if(panel.accessibleRole().empty()) panel.setAccessibleRole("group"); // a composite control to assistive tools
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	const std::string name =
	    values.contains("interpolation") ? string(values["interpolation"], path + ".interpolation") : "monotone";
	const auto interpolation = curveInterpolationFromName(name);
	if(!interpolation) fail(path + ".interpolation", R"(expected "monotone", "linear" or "bezier")");
	editor->setInterpolation(*interpolation);
	editor->setShowsChannels(values.contains("channels") && boolean(values["channels"], path + ".channels"));
	applyHistogram(*editor, values.contains("histogram") ? &values["histogram"] : nullptr, path);
	editor->setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	editor->setColors(colorOr("textColor", colors::black), colorOr("accentColor", colors::accent));
}

/// @brief Shows an editor's curves: its bound value, else "value"; a missing channel is the diagonal.
/// @param panel The panel, a CurvesEditor.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
/// @param lastText Not used.
void showCurves(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& lastText) {
	static_cast<void>(lastText);
	auto* editor = dynamic_cast<CurvesEditor*>(&panel);
	if(!editor) return;
	const json& value = bound ? *bound : properties.contains("value") ? properties["value"] : emptyObject();
	for(const CurveChannel channel : channels) {
		const std::string key(curveChannelKey(channel));
		const bool given = value.is_object() && value.contains(key);
		editor->setCurve(channel, given ? curvePointsFromJson(value[key]) : std::vector<CurvePoint>{});
	}
}

/// @brief The value of an editor's curves: "points", and each colour channel's when the channels show.
/// @param editor The editor.
/// @return The value.
[[nodiscard]] json curvesValue(const CurvesEditor& editor) {
	json value = json::object();
	for(const CurveChannel channel : channels) {
		if(channel != CurveChannel::Rgb && !editor.showsChannels()) break;
		value[std::string(curveChannelKey(channel))] = curvePointsToJson(editor.curve(channel));
	}
	return value;
}

/// @brief Reports curves the user changed.
/// @param panel The panel, a CurvesEditor.
/// @param changed Gets the value.
/// @return The connection.
Connection listenToCurves(Panel& panel, std::function<void(const json&)> changed) {
	auto* editor = dynamic_cast<CurvesEditor*>(&panel);
	if(!editor) return 0;
	return editor->onCurveChanged.connect(
	    [editor, changed = std::move(changed)](CurveChannel) { changed(curvesValue(*editor)); });
}

/// @brief Disconnects listenToCurves().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToCurves(Panel& panel, Connection connection) {
	if(auto* editor = dynamic_cast<CurvesEditor*>(&panel)) editor->onCurveChanged.disconnect(connection);
}

} // namespace

void registerCurvesType(TypeRegistry& registry) {
	auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},     {"value", ValueKind::Any},
	                   {"interpolation", ValueKind::Text}, {"channels", ValueKind::Boolean},
	                   {"histogram", ValueKind::Any},      {"font", ValueKind::Font},
	                   {"textColor", ValueKind::Color},    {"accentColor", ValueKind::Color}};
	set->apply = applyCurves;
	set->everyUpdate = true; // colours follow the theme
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const CurvesEditor*>(&panel) != nullptr; };
	set->value = {"value", showCurves, listenToCurves, stopListeningToCurves};

	registry.add({"curves", "Tone curves: control points on a 0 to 1 square, per channel, over a histogram",
	              [] { return std::make_unique<CurvesEditor>(); }, set, false});
}

} // namespace affresco::format
