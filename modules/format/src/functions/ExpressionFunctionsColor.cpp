/// @file
/// @brief Expression functions of colours and the creative controls' values: curve, gradientColor, hexColor.

#include "ExpressionFunctions.hpp"

#include "CreativeValues.hpp"

#include <array>
#include <cctype>
#include <string>

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

/// @brief A colour written in hex, as one form: hexColor(value).
/// @param call [value].
/// @return "#rrggbb" or "#rrggbbaa" in lower case ("#rgb" and "#rgba" spelt out), or null if the value isn't text in
/// one of those four forms.
Value hexColor(const Call& call) {
	if(!call[0].isText()) return {};
	std::string text(call[0].text());
	if(text.size() != 4 && text.size() != 5 && text.size() != 7 && text.size() != 9) return {};
	if(text.front() != '#') return {};
	for(std::size_t i = 1; i < text.size(); ++i) {
		const auto c = static_cast<unsigned char>(text[i]);
		if(std::isxdigit(c) == 0) return {};
		text[i] = static_cast<char>(std::tolower(c));
	}
	if(text.size() > 5) return Value::fromText(std::move(text));
	std::string full = "#";
	for(std::size_t i = 1; i < text.size(); ++i) full.append(2, text[i]);
	return Value::fromText(std::move(full));
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
	 .about = "A colour written '#rgb', '#rgba', '#rrggbb' or '#rrggbbaa' as '#rrggbb' or '#rrggbbaa' in lower case, "
	          "or null if the value isn't one",
	 .takes = Takes::One,
	 .apply = hexColor},
});

} // namespace

std::span<const Function> colorFunctions() { return table; }

} // namespace affresco::format::detail
