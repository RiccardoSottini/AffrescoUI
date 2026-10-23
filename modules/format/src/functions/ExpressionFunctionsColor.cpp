/// @file
/// @brief Expression functions of the creative controls' values: curve, gradientColor.

#include "ExpressionFunctions.hpp"

#include "CreativeValues.hpp"

#include <array>

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
});

} // namespace

std::span<const Function> colorFunctions() { return table; }

} // namespace affresco::format::detail
