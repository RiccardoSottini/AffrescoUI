/// @file
/// @brief The "gradient" property of every panel (format::Properties::gradient()): its background as a linear or
/// radial gradient of colour stops, over its "color", as CSS's linear-gradient() and radial-gradient().

#include "Properties.hpp"
#include "PropertyValues.hpp"

#include <format>

namespace affresco::format {

namespace {

/// @brief The angle when a gradient gives none: top to bottom, as in CSS.
constexpr double defaultAngle = 180.0;

/// @brief Reads a stop's position.
/// @param value A number from 0 (the start) to 1 (the end).
/// @param at Its place, for errors.
/// @return The position.
double stopPosition(const json& value, const std::string& at) {
	const float position = finiteNumber(value, at);
	if(position < 0.0f || position > 1.0f) fail(at, "expected a position from 0 (the start) to 1 (the end)");
	return position;
}

} // namespace

// A list of colours spread evenly from top to bottom, or {"kind", "angle", "stops"}, each stop a colour (spread
// evenly) or {"at", "color"}
std::shared_ptr<const Gradient> Properties::gradient(const json& value, const std::string& at) const {
	Gradient result{.angle = defaultAngle};
	const json* stops = &value;
	std::string stopsAt = at;
	if(!value.is_array() && !value.is_object()) fail(at, "expected a list of colours or {kind, angle, stops}");
	if(value.is_object()) {
		for(const auto& [key, item] : value.items()) {
			const std::string place = std::format("{}.{}", at, key);
			if(key == "kind") {
				const std::string kind = string(item, place);
				if(kind != "linear" && kind != "radial") fail(place, R"(expected "linear" or "radial")");
				result.kind = kind == "radial" ? Gradient::Kind::Radial : Gradient::Kind::Linear;
			} else if(key == "angle") result.angle = finiteNumber(item, place);
			else if(key != "stops") fail(place, "unknown property (expected kind, angle or stops)");
		}
		if(!value.contains("stops")) fail(at, R"(expected "stops": the gradient's colours)");
		stops = &value["stops"];
		stopsAt += ".stops";
	}
	if(!stops->is_array() || stops->empty()) fail(stopsAt, "expected a list of colours or {at, color} stops");
	if(stops->size() > maxGradientStops) fail(stopsAt, std::format("at most {} stops", maxGradientStops));

	const double last = static_cast<double>(stops->size() - 1);
	for(std::size_t index = 0; index < stops->size(); ++index) {
		const json& stop = (*stops)[index];
		const std::string place = std::format("{}[{}]", stopsAt, index);
		GradientStop read{.at = last > 0.0 ? static_cast<double>(index) / last : 0.0, .color = {}};
		if(stop.is_object()) {
			for(const auto& [key, item] : stop.items())
				if(key != "at" && key != "color") fail(place + "." + key, "unknown property (expected at or color)");
			if(stop.contains("at")) read.at = stopPosition(stop["at"], place + ".at");
			if(!stop.contains("color")) fail(place, R"(expected "color")");
			read.color = color(stop["color"], place + ".color");
		} else read.color = color(stop, place);
		result.stops.push_back(read);
	}
	normalizeGradient(result);
	return std::make_shared<const Gradient>(std::move(result));
}

} // namespace affresco::format
