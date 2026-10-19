/// @file
/// @brief Expression functions: arithmetic (`+ - * / %`), rounding, min, max and clamp. Integers in, integers out,
/// so indices and counters stay integers; `+` with a text joins texts.

#include "ExpressionFunctions.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace affresco::format::detail {
namespace {

/// @brief Folds the arguments, all numbers, with an operation.
/// @param call The arguments (at least one).
/// @param operation How two numbers combine.
/// @return The result: an integer if every argument is one and the result is whole.
template <typename Operation> Value fold(const Call& call, Operation operation) {
	double result = call.number(0);
	for(std::size_t i = 1; i < call.size(); ++i) result = operation(result, call.number(i));
	return numeric(result, call.allIntegers());
}

/// @brief `a + b + ...`: numbers add; if any is a text, all join as texts ("{{n + ' cards'}}").
/// @param call The operands.
/// @return The sum or the text.
Value add(const Call& call) {
	if(std::ranges::any_of(call.values(), &Value::isText)) {
		std::string result;
		for(const Value& part : call.values()) result += part.toText();
		return Value::fromText(std::move(result));
	}
	return fold(call, [](double a, double b) { return a + b; });
}

/// @brief `a * b * ...`.
/// @param call The operands.
/// @return The product.
Value multiply(const Call& call) {
	return fold(call, [](double a, double b) { return a * b; });
}

/// @brief `min(a, b, ...)` or `min(list)`.
/// @param call The numbers.
/// @return The smallest.
Value smallest(const Call& call) {
	return fold(call, [](double a, double b) { return std::min(a, b); });
}

/// @brief `max(a, b, ...)` or `max(list)`.
/// @param call The numbers.
/// @return The largest.
Value largest(const Call& call) {
	return fold(call, [](double a, double b) { return std::max(a, b); });
}

/// @brief `a - b` (and `-a`, which is `0 - a`).
/// @param call [a, b].
/// @return The difference.
Value subtract(const Call& call) {
	const double a = call.number(0); // a's error first
	return numeric(a - call.number(1), call.allIntegers());
}

/// @brief Reads a divisor.
/// @param call [a, divisor].
/// @return The divisor.
/// @throws DocumentError If it is 0 ("division by zero").
double divisorOf(const Call& call) {
	const double divisor = call.number(1);
	if(divisor == 0.0) call.fail("division by zero");
	return divisor;
}

/// @brief `a / b`: always a double (`8 / 2` is 4.0).
/// @param call [a, b].
/// @return The quotient.
Value divide(const Call& call) {
	const double divisor = divisorOf(call);
	return Value::fromNumber(call.number(0) / divisor);
}

/// @brief `a % b`.
/// @param call [a, b].
/// @return The remainder, with a's sign.
Value modulo(const Call& call) {
	const double divisor = divisorOf(call);
	return numeric(std::fmod(call.number(0), divisor), call.allIntegers());
}

/// @brief `clamp(value, low, high)`.
/// @param call [value, low, high].
/// @return The value, kept between low and high (high below low counts as low).
Value clamp(const Call& call) {
	const double low = call.number(1), high = call.number(2);
	return numeric(std::clamp(call.number(0), low, std::max(low, high)), call.allIntegers());
}

/// @brief `round(x)`: halves away from zero.
/// @param call [x].
/// @return The whole number.
Value roundNumber(const Call& call) { return numeric(std::round(call.number(0)), true); }
/// @brief `floor(x)`.
/// @param call [x].
/// @return The whole number below.
Value floorNumber(const Call& call) { return numeric(std::floor(call.number(0)), true); }
/// @brief `ceil(x)`.
/// @param call [x].
/// @return The whole number above.
Value ceilNumber(const Call& call) { return numeric(std::ceil(call.number(0)), true); }
/// @brief `abs(x)`.
/// @param call [x].
/// @return Its size.
Value absolute(const Call& call) { return numeric(std::abs(call.number(0)), call[0].isInteger()); }

/// The table.
const auto table = std::to_array<Function>({
    {.name = "add",
	 .arguments = "a, b, ...",
	 .about = "a + b; with a text, the texts joined",
	 .group = Group::Syntax,
	 .fewest = 1,
	 .apply = add},
    {.name = "subtract",
	 .arguments = "a, b",
	 .about = "a - b",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = subtract},
    {.name = "multiply",
	 .arguments = "a, b, ...",
	 .about = "a * b",
	 .group = Group::Syntax,
	 .fewest = 1,
	 .apply = multiply},
    {.name = "divide",
	 .arguments = "a, b",
	 .about = "a / b, always a number with a fraction",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = divide},
    {.name = "mod",
	 .arguments = "a, b",
	 .about = "a % b: the remainder",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = modulo},
    {.name = "round",
	 .arguments = "number",
	 .about = "The nearest whole number (halves away from zero)",
	 .takes = Takes::One,
	 .apply = roundNumber},
    {.name = "floor",
	 .arguments = "number",
	 .about = "The whole number at or below it",
	 .takes = Takes::One,
	 .apply = floorNumber},
    {.name = "ceil",
	 .arguments = "number",
	 .about = "The whole number at or above it",
	 .takes = Takes::One,
	 .apply = ceilNumber},
    {.name = "abs",
	 .arguments = "number",
	 .about = "Its size, without its sign",
	 .takes = Takes::One,
	 .apply = absolute},
    {.name = "min",
	 .arguments = "a, b, ...",
	 .about = "The smallest number",
	 .fewest = 1,
	 .apply = smallest,
	 .spreads = true,
	 .remark = "numbers or a list"},
    {.name = "max",
	 .arguments = "a, b, ...",
	 .about = "The largest number",
	 .fewest = 1,
	 .apply = largest,
	 .spreads = true,
	 .remark = "numbers or a list"},
    {.name = "clamp",
	 .arguments = "value, low, high",
	 .about = "The value, kept between low and high",
	 .fewest = 3,
	 .most = 3,
	 .apply = clamp},
});

} // namespace

std::span<const Function> numberFunctions() { return table; }

} // namespace affresco::format::detail
