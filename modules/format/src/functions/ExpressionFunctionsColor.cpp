/// @file
/// @brief Expression functions of colours and the creative controls' values: curve, gradientColor, hexColor,
/// colorList.

#include "ExpressionFunctions.hpp"

#include "CreativeValues.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format::detail {
namespace {

/// @brief A curve's value at an input: curve(points, x, interpolation?).
/// @param call The points (a list of [x, y], or a curves value with "points"), the input and the interpolation
/// ("monotone", the default, "linear" or "bezier").
/// @return The output, 0 to 1.
Value curve(const Call& call) {
	if(!call[0].isList() && !call[0].isObject()) call.fail("expected a list of points or a curves value");
	const json given = call[0].toJson();
	std::vector<CurvePoint> points = curvePointsFromJson(given.is_object() ? given.value("points", json()) : given);
	normalizeCurve(points);
	CurveInterpolation interpolation = CurveInterpolation::Monotone;
	if(call.size() > 2) {
		if(!call[2].isText()) call.fail(R"(expected "monotone", "linear" or "bezier")");
		const auto named = curveInterpolationFromName(call[2].text());
		if(!named) call.fail(R"(expected "monotone", "linear" or "bezier")");
		interpolation = *named;
	}
	return Value::fromNumber(evaluateCurve(points, interpolation, call.number(1)));
}

/// @brief A gradient's colour at a position: gradientColor(gradient, at).
/// @param call The gradient (a gradient value) and the position, 0 to 1.
/// @return The colour, "#rrggbbaa".
Value gradientColor(const Call& call) {
	if(!call[0].isObject()) call.fail("expected a gradient: { kind, angle, stops }");
	const Gradient gradient = gradientFromJson(call[0].toJson());
	return Value::fromText(colorToJson(sampleGradient(gradient.stops, call.number(1))));
}

/// @brief A colour written in hex, in one form.
/// @param value The text.
/// @return "#rrggbb" or "#rrggbbaa" in lower case ("#rgb" and "#rgba" spelt out), or empty if the text isn't in one
/// of those four forms.
std::string hexForm(std::string_view value) {
	std::string text(value);
	if(text.size() != 4 && text.size() != 5 && text.size() != 7 && text.size() != 9) return {};
	if(text.front() != '#') return {};
	for(std::size_t i = 1; i < text.size(); ++i) {
		const auto c = static_cast<unsigned char>(text[i]);
		if(std::isxdigit(c) == 0) return {};
		text[i] = static_cast<char>(std::tolower(c));
	}
	if(text.size() > 5) return text;
	std::string full = "#";
	for(std::size_t i = 1; i < text.size(); ++i) full.append(2, text[i]);
	return full;
}

/// @brief A colour as a list of channels, written in hex.
/// @param list 3 or 4 numbers from 0 to 1 (red, green, blue and alpha), kept in that range.
/// @return "#rrggbb" or "#rrggbbaa", or empty if the list isn't 3 or 4 finite numbers.
std::string hexOfChannels(const Value& list) {
	if(list.size() != 3 && list.size() != 4) return {};
	std::string text = "#";
	for(std::size_t i = 0; i < list.size(); ++i) {
		const Value channel = list.item(i);
		if(!channel.isNumber() || !std::isfinite(channel.number())) return {};
		const auto byte = static_cast<unsigned>(std::lround(std::clamp(channel.number(), 0.0, 1.0) * 255.0));
		std::array<char, 3> digits{};
		std::snprintf(digits.data(), digits.size(), "%02x", byte);
		text += digits.data();
	}
	return text;
}

/// @brief A colour given as hex text or as a list of channels, in one hex form.
/// @param value The text or the list.
/// @return "#rrggbb" or "#rrggbbaa", or empty if the value is neither.
std::string hexOf(const Value& value) {
	if(value.isText()) return hexForm(value.text());
	if(value.isList()) return hexOfChannels(value);
	return {};
}

/// @brief A colour as one hex form: hexColor(value).
/// @param call [value]: hex text, or a list of 3 or 4 numbers from 0 to 1 (a scene material's baseColor).
/// @return "#rrggbb" or "#rrggbbaa" in lower case ("#rgb" and "#rgba" spelt out), or null if the value isn't one.
Value hexColor(const Call& call) {
	std::string text = hexOf(call[0]);
	if(text.empty()) return {};
	return Value::fromText(std::move(text));
}

/// @brief A colour as a list of channels: colorList(value).
/// @param call [value]: a colour as hexColor takes one.
/// @return [red, green, blue, alpha], each from 0 to 1 to four decimals (alpha 1 when the colour has none), or null if
/// the value isn't a colour.
Value colorList(const Call& call) {
	const std::string text = hexOf(call[0]);
	if(text.empty()) return {};
	std::vector<Value> channels;
	for(std::size_t i = 1; i + 1 < text.size(); i += 2) {
		const int byte = std::stoi(text.substr(i, 2), nullptr, 16);
		channels.push_back(Value::fromNumber(std::round(byte / 255.0 * 10000.0) / 10000.0));
	}
	if(channels.size() == 3) channels.push_back(Value::fromInteger(1));
	return Value::fromList(std::move(channels));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "curve",
	 .arguments = "points, x, interpolation?",
	 .about = "A curve's output at x: points is a list of [x, y] or a curves value; interpolation is 'monotone' "
	          "(default), 'linear' or 'bezier'",
	 .fewest = 2,
	 .most = 3,
	 .apply = curve},
    {.name = "gradientColor",
	 .arguments = "gradient, at",
	 .about = "A gradient value's colour at a position from 0 to 1, as '#rrggbbaa'",
	 .fewest = 2,
	 .most = 2,
	 .apply = gradientColor},
    {.name = "hexColor",
	 .arguments = "value",
	 .about = "A colour written '#rgb', '#rgba', '#rrggbb' or '#rrggbbaa', or as a list of 3 or 4 numbers from 0 to 1, "
	          "as '#rrggbb' or '#rrggbbaa' in lower case, or null if the value isn't one",
	 .takes = Takes::One,
	 .apply = hexColor},
    {.name = "colorList",
	 .arguments = "value",
	 .about =
	     "A colour as hexColor takes one, as [red, green, blue, alpha] from 0 to 1 (a scene material's baseColor), "
	     "or null if the value isn't one",
	 .takes = Takes::One,
	 .apply = colorList},
});

} // namespace

std::span<const Function> colorFunctions() { return table; }

} // namespace affresco::format::detail
