/// @file
/// @brief The JSON values of the creative controls: colours as "#rrggbbaa", gradients and curve points.

#include "CreativeValues.hpp"

#include <algorithm>
#include <cmath>

namespace affresco::format {

namespace {

/// @brief A finite number of a JSON value.
/// @param value The value.
/// @return The number, or std::nullopt when it isn't a finite number.
[[nodiscard]] std::optional<double> finiteNumber(const json& value) {
	if(!value.is_number()) return std::nullopt;
	const double number = value.get<double>();
	return std::isfinite(number) ? std::optional(number) : std::nullopt;
}

/// @brief Reads one stop of a gradient.
/// @param value { "at": 0..1, "color": hex }.
/// @return The stop, or std::nullopt when it isn't one.
[[nodiscard]] std::optional<GradientStop> stopFromJson(const json& value) {
	if(!value.is_object()) return std::nullopt;
	const auto at = value.contains("at") ? finiteNumber(value["at"]) : std::nullopt;
	const auto color = value.contains("color") ? colorFromJson(value["color"]) : std::nullopt;
	if(!at || !color) return std::nullopt;
	return GradientStop{*at, *color};
}

} // namespace

std::string colorToJson(Color color) {
	static constexpr std::string_view digits = "0123456789abcdef";
	std::string out = "#";
	for(const std::uint8_t byte : color.toBytes()) {
		out += digits[byte >> 4];
		out += digits[byte & 0x0f];
	}
	return out;
}

std::optional<Color> colorFromJson(const json& value) {
	if(!value.is_string()) return std::nullopt;
	return Color::parseHex(value.get_ref<const std::string&>());
}

Gradient gradientFromJson(const json& value) {
	Gradient gradient;
	if(value.is_object()) {
		if(value.contains("kind") && value["kind"] == "radial") gradient.kind = Gradient::Kind::Radial;
		if(const auto angle = value.contains("angle") ? finiteNumber(value["angle"]) : std::nullopt)
			gradient.angle = *angle;
		if(value.contains("stops") && value["stops"].is_array()) {
			for(const json& stop : value["stops"]) {
				if(gradient.stops.size() == maxGradientStops) break;
				if(const auto read = stopFromJson(stop)) gradient.stops.push_back(*read);
			}
		}
	}
	normalizeGradient(gradient);
	return gradient;
}

json gradientToJson(const Gradient& gradient) {
	json stops = json::array();
	for(const GradientStop& stop : gradient.stops)
		stops.push_back({{"at", stop.at}, {"color", colorToJson(stop.color)}});
	return {{"kind", gradient.kind == Gradient::Kind::Radial ? "radial" : "linear"},
	        {"angle", gradient.angle},
	        {"stops", std::move(stops)}};
}

std::vector<CurvePoint> curvePointsFromJson(const json& value) {
	std::vector<CurvePoint> points;
	if(!value.is_array()) return points;
	for(const json& pair : value) {
		if(points.size() == maxCurvePoints) break;
		if(!pair.is_array() || pair.size() != 2) continue;
		const auto x = finiteNumber(pair[0]), y = finiteNumber(pair[1]);
		if(x && y) points.push_back({*x, *y});
	}
	return points;
}

json curvePointsToJson(std::span<const CurvePoint> points) {
	json list = json::array();
	for(const CurvePoint& point : points) list.push_back(json::array({point.x, point.y}));
	return list;
}

std::optional<CurveInterpolation> curveInterpolationFromName(std::string_view name) noexcept {
	if(name == "monotone") return CurveInterpolation::Monotone;
	if(name == "linear") return CurveInterpolation::Linear;
	if(name == "bezier") return CurveInterpolation::Bezier;
	return std::nullopt;
}

std::string_view curveChannelKey(CurveChannel channel) noexcept {
	switch(channel) {
	case CurveChannel::Red:
		return "red";
	case CurveChannel::Green:
		return "green";
	case CurveChannel::Blue:
		return "blue";
	case CurveChannel::Rgb:
		break;
	}
	return "points";
}

} // namespace affresco::format
