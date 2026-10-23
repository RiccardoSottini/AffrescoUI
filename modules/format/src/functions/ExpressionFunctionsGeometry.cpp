/// @file
/// @brief Expression functions for drawing: sin, cos and atan2 in degrees, and sqrt, so documents can compute the
/// corners of a polygon or a star, or the angle of a drag, into path data.

#include "ExpressionFunctions.hpp"

#include <array>
#include <cmath>
#include <numbers>

namespace affresco::format::detail {
namespace {

constexpr double radiansPerDegree = std::numbers::pi / 180.0; ///< Degrees to radians.

/// @brief `sin(degrees)`.
/// @param call [degrees].
/// @return The sine.
Value sine(const Call& call) { return Value::fromNumber(std::sin(call.number(0) * radiansPerDegree)); }

/// @brief `cos(degrees)`.
/// @param call [degrees].
/// @return The cosine.
Value cosine(const Call& call) { return Value::fromNumber(std::cos(call.number(0) * radiansPerDegree)); }

/// @brief `atan2(y, x)`: the angle of the point (x, y) from the x axis.
/// @param call [y, x].
/// @return Degrees, -180 to 180 (0 when both are 0).
Value angle(const Call& call) {
	const double y = call.number(0); // y's error first
	return Value::fromNumber(std::atan2(y, call.number(1)) / radiansPerDegree);
}

/// @brief `sqrt(number)`.
/// @param call [number].
/// @return The square root.
/// @throws DocumentError If the number is below zero.
Value squareRoot(const Call& call) {
	const double number = call.number(0);
	if(number < 0.0) call.fail("expected a number of zero or more");
	return Value::fromNumber(std::sqrt(number));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "sin",
	 .arguments = "degrees",
	 .about = "The sine of an angle in degrees",
	 .takes = Takes::One,
	 .apply = sine},
    {.name = "cos",
	 .arguments = "degrees",
	 .about = "The cosine of an angle in degrees",
	 .takes = Takes::One,
	 .apply = cosine},
    {.name = "atan2",
	 .arguments = "y, x",
	 .about = "The angle of the point (x, y), in degrees from -180 to 180",
	 .fewest = 2,
	 .most = 2,
	 .apply = angle},
    {.name = "sqrt",
	 .arguments = "number",
	 .about = "The square root of a number of zero or more",
	 .takes = Takes::One,
	 .apply = squareRoot},
});

} // namespace

std::span<const Function> geometryFunctions() { return table; }

} // namespace affresco::format::detail
