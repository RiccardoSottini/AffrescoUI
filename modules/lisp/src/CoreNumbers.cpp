/// @file
/// @brief The language's own functions on numbers and truth: `+ - * / mod rem inc dec min max abs < > <= >= = not=
/// not`, and the type tests.

#include "Machine.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace affresco::lisp {

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// @brief Checks an argument is a number.
/// @param value The argument.
/// @param function The function, for the error.
void needNumber(const Value& value, std::string_view function) {
	if(!value.isNumber())
		throw Problem("\"" + std::string(function) + "\" takes numbers, and was given " +
		              std::string(describeKind(value)));
}

/// @brief Checks how many arguments a function has.
/// @param arguments Its arguments.
/// @param least The least.
/// @param function The function, for the error.
void needAtLeast(Arguments arguments, std::size_t least, std::string_view function) {
	if(arguments.size() < least)
		throw Problem("\"" + std::string(function) + "\" takes at least " + std::to_string(least) + " arguments");
}

/// @brief An arithmetic operator.
enum class Arithmetic : std::uint8_t { Add, Subtract, Multiply };

/// The highest integer.
constexpr std::int64_t highestInteger = std::numeric_limits<std::int64_t>::max();
/// The lowest integer.
constexpr std::int64_t lowestInteger = std::numeric_limits<std::int64_t>::min();

/// @brief Tells whether a product of two integers fits one.
/// @param a One.
/// @param b The other.
/// @return true if it does.
constexpr bool productFits(std::int64_t a, std::int64_t b) noexcept {
	if(a == 0 || b == 0) return true;
	if(a > 0) return b > 0 ? a <= highestInteger / b : b >= lowestInteger / a;
	return b > 0 ? a >= lowestInteger / b : a >= highestInteger / b;
}

/// @brief Tells whether an operator's result on two integers fits one.
/// @param op The operator.
/// @param a One.
/// @param b The other.
/// @return true if it does.
constexpr bool fits(Arithmetic op, std::int64_t a, std::int64_t b) noexcept {
	if(op == Arithmetic::Add) return b >= 0 ? a <= highestInteger - b : a >= lowestInteger - b;
	if(op == Arithmetic::Subtract) return b >= 0 ? a >= lowestInteger + b : a <= highestInteger + b;
	return productFits(a, b);
}

/// @brief Applies an operator: integers stay integers while they fit, and go on as doubles past that.
/// @param op The operator.
/// @param first One number.
/// @param second The other.
/// @return The result.
Value arithmetic(Arithmetic op, const Value& first, const Value& second) noexcept {
	if(first.kind() == Kind::Integer && second.kind() == Kind::Integer &&
	   fits(op, first.asInteger(), second.asInteger())) {
		const std::int64_t a = first.asInteger();
		const std::int64_t b = second.asInteger();
		return Value::integer(op == Arithmetic::Add ? a + b : op == Arithmetic::Subtract ? a - b : a * b);
	}
	const double a = first.asDouble();
	const double b = second.asDouble();
	return Value::real(op == Arithmetic::Add ? a + b : op == Arithmetic::Subtract ? a - b : a * b);
}

/// @brief `+`, `*`: folds its arguments from a starting number.
/// @param arguments The arguments.
/// @param op The operator.
/// @param start The starting number.
/// @param name The function, for an error.
/// @return The result.
Value fold(Arguments arguments, Arithmetic op, std::int64_t start, std::string_view name) {
	Value result = Value::integer(start);
	for(const Value& argument : arguments) {
		needNumber(argument, name);
		result = arithmetic(op, result, argument);
	}
	return result;
}

/// @brief `+`. @param arguments The numbers. @return Their sum.
Value add(Machine& /*machine*/, Arguments arguments) { return fold(arguments, Arithmetic::Add, 0, "+"); }

/// @brief `*`. @param arguments The numbers. @return Their product.
Value multiply(Machine& /*machine*/, Arguments arguments) { return fold(arguments, Arithmetic::Multiply, 1, "*"); }

/// @brief `-`. @param arguments One number to negate, or a number and those to take from it. @return The result.
Value subtract(Machine& /*machine*/, Arguments arguments) {
	needAtLeast(arguments, 1, "-");
	needNumber(arguments[0], "-");
	if(arguments.size() == 1) return arithmetic(Arithmetic::Subtract, Value::integer(0), arguments[0]);
	Value result = arguments[0];
	for(const Value& argument : arguments.subspan(1)) {
		needNumber(argument, "-");
		result = arithmetic(Arithmetic::Subtract, result, argument);
	}
	return result;
}

/// @brief `/` on two numbers: exact integers stay integers.
/// @param first The dividend.
/// @param second The divisor.
/// @return The quotient.
Value divided(const Value& first, const Value& second) {
	if(second.asDouble() == 0) throw Problem("dividing by zero");
	if(first.kind() == Kind::Integer && second.kind() == Kind::Integer) {
		const std::int64_t a = first.asInteger();
		const std::int64_t b = second.asInteger();
		if((a != lowestInteger || b != -1) && a % b == 0) return Value::integer(a / b);
	}
	return Value::real(first.asDouble() / second.asDouble());
}

/// @brief `/`. @param arguments A number and those to divide it by, or one to invert. @return The result.
Value divide(Machine& /*machine*/, Arguments arguments) {
	needAtLeast(arguments, 1, "/");
	needNumber(arguments[0], "/");
	if(arguments.size() == 1) return divided(Value::integer(1), arguments[0]);
	Value result = arguments[0];
	for(const Value& argument : arguments.subspan(1)) {
		needNumber(argument, "/");
		result = divided(result, argument);
	}
	return result;
}

/// @brief What `mod` and `rem` share.
/// @param arguments The dividend and the divisor.
/// @param floored The result has the divisor's sign (`mod`), else the dividend's (`rem`).
/// @param name The function, for an error.
/// @return The remainder.
Value remainderOf(Arguments arguments, bool floored, std::string_view name) {
	if(arguments.size() != 2) throw Problem("\"" + std::string(name) + "\" takes two numbers");
	needNumber(arguments[0], name);
	needNumber(arguments[1], name);
	if(arguments[1].asDouble() == 0) throw Problem("dividing by zero");
	if(arguments[0].kind() == Kind::Integer && arguments[1].kind() == Kind::Integer) {
		const std::int64_t b = arguments[1].asInteger();
		if(b == -1) return Value::integer(0); // and no overflow from the lowest integer
		const std::int64_t r = arguments[0].asInteger() % b;
		return Value::integer(floored && r != 0 && (r < 0) != (b < 0) ? r + b : r);
	}
	const double b = arguments[1].asDouble();
	const double r = std::fmod(arguments[0].asDouble(), b);
	return Value::real(floored && r != 0 && (r < 0) != (b < 0) ? r + b : r);
}

/// @brief `mod`. @param arguments The numbers. @return The remainder, with the divisor's sign.
Value modulo(Machine& /*machine*/, Arguments arguments) { return remainderOf(arguments, true, "mod"); }

/// @brief `rem`. @param arguments The numbers. @return The remainder, with the dividend's sign.
Value remainder(Machine& /*machine*/, Arguments arguments) { return remainderOf(arguments, false, "rem"); }

/// @brief `inc`. @param arguments A number. @return It plus one.
Value increment(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"inc\" takes one number");
	needNumber(arguments[0], "inc");
	return arithmetic(Arithmetic::Add, arguments[0], Value::integer(1));
}

/// @brief `dec`. @param arguments A number. @return It minus one.
Value decrement(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"dec\" takes one number");
	needNumber(arguments[0], "dec");
	return arithmetic(Arithmetic::Subtract, arguments[0], Value::integer(1));
}

/// @brief Whether one number is below another, exactly for integers.
/// @param first One.
/// @param second The other.
/// @return true if it is.
bool below(const Value& first, const Value& second) noexcept {
	if(first.kind() == Kind::Integer && second.kind() == Kind::Integer) return first.asInteger() < second.asInteger();
	if(first.kind() == Kind::Unsigned && second.kind() == Kind::Unsigned)
		return first.asUnsigned() < second.asUnsigned();
	return first.asDouble() < second.asDouble();
}

/// @brief What `<`, `>`, `<=` and `>=` share: holds for each pair side by side.
/// @param arguments The numbers.
/// @param swap Compare the other way round.
/// @param negate Take the opposite.
/// @param name The function, for an error.
/// @return Whether it holds.
Value ordered(Arguments arguments, bool swap, bool negate, std::string_view name) {
	needAtLeast(arguments, 1, name);
	needNumber(arguments[0], name);
	for(std::size_t at = 1; at < arguments.size(); ++at) {
		needNumber(arguments[at], name);
		const bool holds = swap ? below(arguments[at], arguments[at - 1]) : below(arguments[at - 1], arguments[at]);
		if(holds == negate) return Value::boolean(false);
	}
	return Value::boolean(true);
}

/// @brief `<`. @param arguments The numbers. @return Whether they rise.
Value less(Machine& /*machine*/, Arguments arguments) { return ordered(arguments, false, false, "<"); }

/// @brief `>`. @param arguments The numbers. @return Whether they fall.
Value greater(Machine& /*machine*/, Arguments arguments) { return ordered(arguments, true, false, ">"); }

/// @brief `<=`. @param arguments The numbers. @return Whether they never fall.
Value lessOrEqual(Machine& /*machine*/, Arguments arguments) { return ordered(arguments, true, true, "<="); }

/// @brief `>=`. @param arguments The numbers. @return Whether they never rise.
Value greaterOrEqual(Machine& /*machine*/, Arguments arguments) { return ordered(arguments, false, true, ">="); }

/// @brief `min`. @param arguments The numbers. @return The lowest.
Value lowest(Machine& /*machine*/, Arguments arguments) {
	needAtLeast(arguments, 1, "min");
	Value result = arguments[0];
	for(const Value& argument : arguments) {
		needNumber(argument, "min");
		if(below(argument, result)) result = argument;
	}
	return result;
}

/// @brief `max`. @param arguments The numbers. @return The highest.
Value highest(Machine& /*machine*/, Arguments arguments) {
	needAtLeast(arguments, 1, "max");
	Value result = arguments[0];
	for(const Value& argument : arguments) {
		needNumber(argument, "max");
		if(below(result, argument)) result = argument;
	}
	return result;
}

/// @brief `abs`. @param arguments A number. @return Its size.
Value absolute(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"abs\" takes one number");
	needNumber(arguments[0], "abs");
	return below(arguments[0], Value::integer(0)) ? arithmetic(Arithmetic::Subtract, Value::integer(0), arguments[0])
	                                              : arguments[0];
}

/// @brief `=`. @param machine The machine. @param arguments The values. @return Whether they are all equal.
Value equal(Machine& machine, Arguments arguments) {
	needAtLeast(arguments, 1, "=");
	for(std::size_t at = 1; at < arguments.size(); ++at)
		if(!valuesEqual(machine, arguments[at - 1], arguments[at])) return Value::boolean(false);
	return Value::boolean(true);
}

/// @brief `not=`. @param machine The machine. @param arguments The values. @return The opposite of `=`.
Value notEqual(Machine& machine, Arguments arguments) { return Value::boolean(!equal(machine, arguments).asBoolean()); }

/// @brief `not`. @param arguments A value. @return Whether it is nil or false.
Value negation(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"not\" takes one value");
	return Value::boolean(!arguments[0].truthy());
}

/// @brief A test on one value's kind.
/// @tparam kinds What it passes for.
/// @param arguments The value.
/// @return Whether it is one of them.
template <Kind... kinds> Value isKind(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("a test takes one value");
	return Value::boolean(((arguments[0].kind() == kinds) || ...));
}

/// @brief `some?`. @param arguments A value. @return Whether it isn't nil.
Value isSome(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"some?\" takes one value");
	return Value::boolean(!arguments[0].isNil());
}

} // namespace

void defineNumbers(Machine& machine) {
	machine.defineNative("+", add);
	machine.defineNative("-", subtract);
	machine.defineNative("*", multiply);
	machine.defineNative("/", divide);
	machine.defineNative("mod", modulo);
	machine.defineNative("rem", remainder);
	machine.defineNative("inc", increment);
	machine.defineNative("dec", decrement);
	machine.defineNative("min", lowest);
	machine.defineNative("max", highest);
	machine.defineNative("abs", absolute);
	machine.defineNative("<", less);
	machine.defineNative(">", greater);
	machine.defineNative("<=", lessOrEqual);
	machine.defineNative(">=", greaterOrEqual);
	machine.defineNative("=", equal);
	machine.defineNative("not=", notEqual);
	machine.defineNative("not", negation);
	machine.defineNative("some?", isSome);
	machine.defineNative("nil?", isKind<Kind::Nil>);
	machine.defineNative("boolean?", isKind<Kind::Boolean>);
	machine.defineNative("number?", isKind<Kind::Integer, Kind::Unsigned, Kind::Float>);
	machine.defineNative("string?", isKind<Kind::String>);
	machine.defineNative("symbol?", isKind<Kind::Symbol>);
	machine.defineNative("keyword?", isKind<Kind::Keyword>);
	machine.defineNative("list?", isKind<Kind::List>);
	machine.defineNative("vector?", isKind<Kind::Vector>);
	machine.defineNative("map?", isKind<Kind::Map>);
	machine.defineNative("fn?", isKind<Kind::Function, Kind::Native>);
}

} // namespace affresco::lisp
