/// @file
/// @brief Expression functions: trigonometry in radians, roots, powers, logarithms and the sign of a number, for
/// positions on a circle, angles, distances and curves. A result that isn't a finite number (the root of a negative
/// number, a power past the range) is an error at the function's place, as a division by zero is.

#include "ExpressionFunctions.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <string_view>

namespace affresco::format::detail {
namespace {

/// @brief Checks a result: the functions give finite numbers only.
/// @param call The call, for the error's place.
/// @param name The function, for the error.
/// @param value The result.
/// @return It.
/// @throws DocumentError If it is NaN or infinite: "<name> has no finite result here".
double finite(const Call& call, std::string_view name, double value) {
	if(!std::isfinite(value)) call.fail(std::string(name) + " has no finite result here");
	return value;
}

/// @brief A result with a fraction, checked.
/// @param call The call, for the error's place.
/// @param name The function, for the error.
/// @param value The result.
/// @return It.
/// @throws DocumentError If it is NaN or infinite.
Value fraction(const Call& call, std::string_view name, double value) {
	return Value::fromNumber(finite(call, name, value));
}

/// @brief `sin(radians)`.
/// @param call [radians].
/// @return The sine.
Value sine(const Call& call) { return fraction(call, "sin", std::sin(call.number(0))); }
/// @brief `cos(radians)`.
/// @param call [radians].
/// @return The cosine.
Value cosine(const Call& call) { return fraction(call, "cos", std::cos(call.number(0))); }
/// @brief `tan(radians)`.
/// @param call [radians].
/// @return The tangent.
Value tangent(const Call& call) { return fraction(call, "tan", std::tan(call.number(0))); }
/// @brief `asin(number)`.
/// @param call [-1 to 1].
/// @return Radians, -pi/2 to pi/2.
Value arcSine(const Call& call) { return fraction(call, "asin", std::asin(call.number(0))); }
/// @brief `acos(number)`.
/// @param call [-1 to 1].
/// @return Radians, 0 to pi.
Value arcCosine(const Call& call) { return fraction(call, "acos", std::acos(call.number(0))); }
/// @brief `atan(number)`.
/// @param call [number].
/// @return Radians, -pi/2 to pi/2.
Value arcTangent(const Call& call) { return fraction(call, "atan", std::atan(call.number(0))); }
/// @brief `exp(number)`.
/// @param call [the power].
/// @return e to it.
Value exponential(const Call& call) { return fraction(call, "exp", std::exp(call.number(0))); }
/// @brief `radians(degrees)`.
/// @param call [degrees].
/// @return Radians.
Value toRadians(const Call& call) { return fraction(call, "radians", call.number(0) * std::numbers::pi / 180.0); }
/// @brief `degrees(radians)`.
/// @param call [radians].
/// @return Degrees.
Value toDegrees(const Call& call) { return fraction(call, "degrees", call.number(0) * 180.0 / std::numbers::pi); }

/// @brief `atan2(y, x)`: the angle of the point (x, y) from the x axis.
/// @param call [y, x].
/// @return Radians, -π to π.
Value angleOf(const Call& call) {
	const double y = call.number(0); // y's error first
	return Value::fromNumber(std::atan2(y, call.number(1)));
}

/// @brief `sqrt(x)`: integers in, integers out when the root is whole (`sqrt(9)` is 3).
/// @param call [x].
/// @return The square root.
Value squareRoot(const Call& call) {
	return numeric(finite(call, "sqrt", std::sqrt(call.number(0))), call.allIntegers());
}

/// @brief `pow(base, exponent)`: integers in, integers out when the result is whole (`pow(2, 10)` is 1024).
/// @param call [base, exponent].
/// @return The power.
Value power(const Call& call) {
	const double base = call.number(0); // the base's error first
	return numeric(finite(call, "pow", std::pow(base, call.number(1))), call.allIntegers());
}

/// @brief `log(x, base?)`: the natural logarithm, or in a base.
/// @param call [x] or [x, base].
/// @return The logarithm.
Value logarithm(const Call& call) {
	const double x = call.number(0);
	const double natural = std::log(x);
	if(call.size() < 2) return fraction(call, "log", natural);
	return fraction(call, "log", natural / std::log(call.number(1)));
}

/// @brief `hypot(a, b, ...)`: the length of a vector, the distance from (0, 0) to (a, b).
/// @param call The components.
/// @return The square root of the sum of their squares.
Value length(const Call& call) {
	double sum = 0.0;
	for(std::size_t i = 0; i < call.size(); ++i) sum += call.number(i) * call.number(i);
	return numeric(finite(call, "hypot", std::sqrt(sum)), call.allIntegers());
}

/// @brief `sign(x)`.
/// @param call [x].
/// @return -1, 0 or 1.
Value signOf(const Call& call) {
	const double x = call.number(0);
	return Value::fromInteger(x > 0.0 ? 1 : x < 0.0 ? -1 : 0);
}

/// @brief `pi()`.
/// @return π.
Value pi(const Call& /*call*/) { return Value::fromNumber(std::numbers::pi); }

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "sin", .arguments = "radians", .about = "The sine of an angle", .takes = Takes::One, .apply = sine},
    {.name = "cos", .arguments = "radians", .about = "The cosine of an angle", .takes = Takes::One, .apply = cosine},
    {.name = "tan", .arguments = "radians", .about = "The tangent of an angle", .takes = Takes::One, .apply = tangent},
    {.name = "asin",
	 .arguments = "number",
	 .about = "The angle, in radians, whose sine it is (-1 to 1)",
	 .takes = Takes::One,
	 .apply = arcSine},
    {.name = "acos",
	 .arguments = "number",
	 .about = "The angle, in radians, whose cosine it is (-1 to 1)",
	 .takes = Takes::One,
	 .apply = arcCosine},
    {.name = "atan",
	 .arguments = "number",
	 .about = "The angle, in radians, whose tangent it is",
	 .takes = Takes::One,
	 .apply = arcTangent},
    {.name = "atan2",
	 .arguments = "y, x",
	 .about = "The angle, in radians from -pi to pi, of the point (x, y)",
	 .fewest = 2,
	 .most = 2,
	 .apply = angleOf},
    {.name = "radians", .arguments = "degrees", .about = "Degrees in radians", .takes = Takes::One, .apply = toRadians},
    {.name = "degrees", .arguments = "radians", .about = "Radians in degrees", .takes = Takes::One, .apply = toDegrees},
    {.name = "pi", .arguments = "", .about = "The number pi", .most = 0, .apply = pi},
    {.name = "sqrt",
	 .arguments = "number",
	 .about = "The square root (of 0 or more)",
	 .takes = Takes::One,
	 .apply = squareRoot},
    {.name = "pow",
	 .arguments = "base, exponent",
	 .about = "The base to the power of the exponent",
	 .fewest = 2,
	 .most = 2,
	 .apply = power},
    {.name = "exp",
	 .arguments = "number",
	 .about = "e to the power of the number",
	 .takes = Takes::One,
	 .apply = exponential},
    {.name = "log",
	 .arguments = "number, base?",
	 .about = "The logarithm (natural, or in the base)",
	 .fewest = 1,
	 .most = 2,
	 .apply = logarithm},
    {.name = "hypot",
	 .arguments = "a, b, ...",
	 .about = "The length of the vector (a, b, ...)",
	 .fewest = 1,
	 .apply = length,
	 .spreads = true,
	 .remark = "numbers or a list"},
    {.name = "sign",
	 .arguments = "number",
	 .about = "-1, 0 or 1, as the number is below, at or above 0",
	 .takes = Takes::One,
	 .apply = signOf},
});

} // namespace

std::span<const Function> mathsFunctions() { return table; }

} // namespace affresco::format::detail
