/// @file
/// @brief The language's own functions on numbers and truth: `+ - * / mod rem inc dec min max abs < > <= >= = not=
/// not`, `compare`, `==`, `quot`, `int`, `double`, the bit operations, and the tests (`even?`, `pos?`, `integer?`...).

#include "Machine.hpp"
#include "Sequences.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
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

/// @brief The order of two numbers for `compare`: by value, NaN after every other number, as sorting needs.
/// @param left One.
/// @param right The other.
/// @return -1, 0 or 1.
int compareNumbers(const Value& left, const Value& right) {
	const bool leftNaN = left.kind() == Kind::Float && std::isnan(left.asDouble());
	const bool rightNaN = right.kind() == Kind::Float && std::isnan(right.asDouble());
	if(leftNaN || rightNaN) return leftNaN == rightNaN ? 0 : leftNaN ? 1 : -1;
	if(below(left, right)) return -1;
	return below(right, left) ? 1 : 0;
}

} // namespace

int compareValues(Machine& machine, const Value& first, const Value& second, std::string_view function) {
	if(first.isNil() || second.isNil()) return first.isNil() == second.isNil() ? 0 : first.isNil() ? -1 : 1;
	if(first.isNumber() && second.isNumber()) return compareNumbers(first, second);
	const auto text = [](const Value& value) { return value.kind() == Kind::String || value.kind() == Kind::Keyword; };
	if(text(first) && text(second)) { // a keyword is the string of its text (decision D2)
		machine.spendText(std::min(first.text().size(), second.text().size()));
		const int order = first.text().compare(second.text());
		return (order > 0) - (order < 0);
	}
	throw Problem("\"" + std::string(function) + "\" compares numbers with numbers and text with text, and was given " +
	              std::string(describeKind(first)) + " and " + std::string(describeKind(second)));
}

namespace {

/// @brief `compare`. @param machine The machine. @param arguments Two values. @return -1, 0 or 1: whether the first
/// comes before the second, with it, or after it (compareValues()).
Value compared(Machine& machine, Arguments arguments) {
	if(arguments.size() != 2) throw Problem("\"compare\" takes two values");
	return Value::integer(compareValues(machine, arguments[0], arguments[1], "compare"));
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

/// @brief `truthy?`: an expression's truth, where nil, false, 0, "", [], {} and #{} are false, for a function that
/// tests a value of the running app as a binding would. @param arguments A value. @return Whether it is true so.
Value expressionTruth(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"truthy?\" takes one value");
	const Value& value = arguments[0];
	if(value.isNumber()) return Value::boolean(value.asDouble() != 0.0);
	if(value.kind() == Kind::String) return Value::boolean(!value.text().empty());
	if(value.kind() == Kind::Vector || value.kind() == Kind::List || value.kind() == Kind::Map ||
	   value.kind() == Kind::Set)
		return Value::boolean(value.count() > 0);
	return Value::boolean(value.truthy());
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

/// @brief `even?` and `odd?`.
/// @param arguments An integer, or a double whose value is whole.
/// @param odd Whether it tests for odd.
/// @param name The function, for an error.
/// @return Whether the number is so.
Value parity(Arguments arguments, bool odd, std::string_view name) {
	if(arguments.size() != 1) throw Problem("\"" + std::string(name) + "\" takes one number");
	const Value& number = arguments[0];
	bool isOdd = false;
	if(number.kind() == Kind::Integer) isOdd = number.asInteger() % 2 != 0;
	else if(number.kind() == Kind::Unsigned) isOdd = (number.asUnsigned() & 1U) != 0;
	else if(const double value = number.asDouble();
	        number.kind() == Kind::Float && std::isfinite(value) && std::trunc(value) == value)
		isOdd = std::fmod(value, 2.0) != 0;
	else throw Problem("\"" + std::string(name) + "\" takes a whole number");
	return Value::boolean(isOdd == odd);
}

/// @brief `even?`. @param arguments A whole number. @return Whether it is even.
Value isEven(Machine& /*machine*/, Arguments arguments) { return parity(arguments, false, "even?"); }

/// @brief `odd?`. @param arguments A whole number. @return Whether it is odd.
Value isOdd(Machine& /*machine*/, Arguments arguments) { return parity(arguments, true, "odd?"); }

/// @brief The sign of a number, for the tests on it.
/// @param arguments One number.
/// @param name The function, for an error.
/// @return -1, 0 or 1; 2 for NaN, which has none.
int signOf(Arguments arguments, std::string_view name) {
	if(arguments.size() != 1) throw Problem("\"" + std::string(name) + "\" takes one number");
	needNumber(arguments[0], name);
	if(arguments[0].kind() == Kind::Float && std::isnan(arguments[0].asDouble())) return 2;
	return below(arguments[0], Value::integer(0)) ? -1 : below(Value::integer(0), arguments[0]) ? 1 : 0;
}

/// @brief `zero?`. @param arguments A number. @return Whether it is zero.
Value isZero(Machine& /*machine*/, Arguments arguments) { return Value::boolean(signOf(arguments, "zero?") == 0); }

/// @brief `pos?`. @param arguments A number. @return Whether it is above zero.
Value isPositive(Machine& /*machine*/, Arguments arguments) { return Value::boolean(signOf(arguments, "pos?") == 1); }

/// @brief `neg?`. @param arguments A number. @return Whether it is below zero.
Value isNegative(Machine& /*machine*/, Arguments arguments) { return Value::boolean(signOf(arguments, "neg?") == -1); }

/// @brief `nat-int?`, `pos-int?` and `neg-int?`: an integer of some signs.
/// @tparam lowest The lowest sign it passes for.
/// @tparam highest The highest.
/// @param arguments A value.
/// @return Whether it is an integer of those signs.
template <int lowest, int highest> Value isSignedInteger(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("a test takes one value");
	if(arguments[0].kind() != Kind::Integer && arguments[0].kind() != Kind::Unsigned) return Value::boolean(false);
	const int sign = signOf(arguments, "an integer's test");
	return Value::boolean(sign >= lowest && sign <= highest);
}

/// @brief `true?` and `false?`.
/// @tparam truth Which.
/// @param arguments A value.
/// @return Whether it is that boolean, and not merely true or false as a test takes it.
template <bool truth> Value isBoolean(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("a test takes one value");
	return Value::boolean(arguments[0].kind() == Kind::Boolean && arguments[0].asBoolean() == truth);
}

/// @brief `boolean`. @param arguments A value. @return Whether it is true as a test takes it: not nil or false.
Value toBoolean(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"boolean\" takes one value");
	return Value::boolean(arguments[0].truthy());
}

/// @brief `quot`. @param arguments Two numbers. @return The quotient, toward zero: an integer for integers.
Value quotient(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 2) throw Problem("\"quot\" takes two numbers");
	needNumber(arguments[0], "quot");
	needNumber(arguments[1], "quot");
	if(arguments[1].asDouble() == 0) throw Problem("dividing by zero");
	if(arguments[0].kind() == Kind::Integer && arguments[1].kind() == Kind::Integer) {
		const std::int64_t a = arguments[0].asInteger();
		const std::int64_t b = arguments[1].asInteger();
		return a == lowestInteger && b == -1 ? Value::real(-static_cast<double>(a)) : Value::integer(a / b);
	}
	return Value::real(std::trunc(arguments[0].asDouble() / arguments[1].asDouble()));
}

/// @brief `int`. @param arguments A number. @return It as an integer, toward zero.
Value toInteger(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"int\" takes one number");
	needNumber(arguments[0], "int");
	const Value& number = arguments[0];
	if(number.kind() == Kind::Integer) return number;
	constexpr double limit = 9223372036854775808.0; // 2^63
	const double value = number.kind() == Kind::Unsigned ? limit : std::trunc(number.asDouble());
	if(std::isnan(value) || value >= limit || value < -limit)
		throw Problem("\"int\" takes a number within an integer's range");
	return Value::integer(static_cast<std::int64_t>(value));
}

/// @brief `double`. @param arguments A number. @return It as a double.
Value toDouble(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"double\" takes one number");
	needNumber(arguments[0], "double");
	return Value::real(arguments[0].asDouble());
}

/// @brief `==`. @param arguments Numbers. @return Whether they are all the same number: exactly for integers.
Value numbersEqual(Machine& /*machine*/, Arguments arguments) {
	needAtLeast(arguments, 1, "==");
	needNumber(arguments[0], "==");
	bool equal = true;
	for(std::size_t at = 1; at < arguments.size(); ++at) {
		const Value& a = arguments[at - 1];
		const Value& b = arguments[at];
		needNumber(b, "==");
		if(a.kind() == Kind::Float || b.kind() == Kind::Float) equal = equal && a.asDouble() == b.asDouble();
		else if(a.kind() != b.kind()) equal = false; // an Unsigned is above every Integer
		else
			equal = equal &&
			        (a.kind() == Kind::Integer ? a.asInteger() == b.asInteger() : a.asUnsigned() == b.asUnsigned());
	}
	return Value::boolean(equal);
}

/// @brief A bit operation's argument.
/// @param value The argument.
/// @param name The function, for an error.
/// @return It as an integer.
std::int64_t bitsOf(const Value& value, std::string_view name) {
	const std::optional<std::int64_t> whole = wholeNumber(value);
	if(!whole) throw Problem("\"" + std::string(name) + "\" takes whole numbers");
	return *whole;
}

/// @brief `bit-and`, `bit-or`, `bit-xor`: folds the integers' bits.
/// @tparam Operation The operation on two integers.
/// @param arguments Two integers or more.
/// @param name The function, for an error.
/// @return The result.
template <class Operation> Value foldBits(Arguments arguments, std::string_view name) {
	needAtLeast(arguments, 2, name);
	std::int64_t result = bitsOf(arguments[0], name);
	for(const Value& argument : arguments.subspan(1)) result = Operation{}(result, bitsOf(argument, name));
	return Value::integer(result);
}

/// @brief `bit-and`. @param arguments Integers. @return Their bits that are set in all.
Value bitAnd(Machine& /*machine*/, Arguments arguments) { return foldBits<std::bit_and<>>(arguments, "bit-and"); }

/// @brief `bit-or`. @param arguments Integers. @return Their bits that are set in any.
Value bitOr(Machine& /*machine*/, Arguments arguments) { return foldBits<std::bit_or<>>(arguments, "bit-or"); }

/// @brief `bit-xor`. @param arguments Integers. @return Their bits set in an odd number of them.
Value bitXor(Machine& /*machine*/, Arguments arguments) { return foldBits<std::bit_xor<>>(arguments, "bit-xor"); }

/// @brief `bit-not`. @param arguments An integer. @return Its bits flipped.
Value bitNot(Machine& /*machine*/, Arguments arguments) {
	if(arguments.size() != 1) throw Problem("\"bit-not\" takes one number");
	return Value::integer(~bitsOf(arguments[0], "bit-not"));
}

/// @brief `bit-shift-left` and `bit-shift-right`.
/// @param arguments An integer, and how many places: 0 to 63.
/// @param left Which way.
/// @return The integer shifted: to the left in two's complement, to the right keeping its sign.
Value shifted(Arguments arguments, bool left) {
	const std::string_view name = left ? "bit-shift-left" : "bit-shift-right";
	if(arguments.size() != 2) throw Problem("\"" + std::string(name) + "\" takes a number and how many places");
	const std::int64_t bits = bitsOf(arguments[0], name);
	const std::int64_t places = bitsOf(arguments[1], name);
	if(places < 0 || places > 63) throw Problem("\"" + std::string(name) + "\" shifts by 0 to 63 places");
	if(left) return Value::integer(static_cast<std::int64_t>(static_cast<std::uint64_t>(bits) << places));
	return Value::integer(bits >> places);
}

/// @brief `bit-shift-left`. @param arguments An integer and how many places. @return It shifted left.
Value shiftLeft(Machine& /*machine*/, Arguments arguments) { return shifted(arguments, true); }

/// @brief `bit-shift-right`. @param arguments An integer and how many places. @return It shifted right.
Value shiftRight(Machine& /*machine*/, Arguments arguments) { return shifted(arguments, false); }

/// @brief Registers the tests on numbers and on what a value is (`even?`, `coll?`...).
/// @param machine The machine.
void defineTests(Machine& machine) {
	machine.defineNative("even?", isEven);
	machine.defineNative("odd?", isOdd);
	machine.defineNative("zero?", isZero);
	machine.defineNative("pos?", isPositive);
	machine.defineNative("neg?", isNegative);
	machine.defineNative("true?", isBoolean<true>);
	machine.defineNative("false?", isBoolean<false>);
	machine.defineNative("integer?", isKind<Kind::Integer, Kind::Unsigned>);
	machine.defineNative("int?", isKind<Kind::Integer, Kind::Unsigned>);
	machine.defineNative("float?", isKind<Kind::Float>);
	machine.defineNative("double?", isKind<Kind::Float>);
	machine.defineNative("nat-int?", isSignedInteger<0, 1>);
	machine.defineNative("pos-int?", isSignedInteger<1, 1>);
	machine.defineNative("neg-int?", isSignedInteger<-1, -1>);
	machine.defineNative("coll?", isKind<Kind::List, Kind::Vector, Kind::Map, Kind::Set>);
	machine.defineNative("seq?", isKind<Kind::List>);
	machine.defineNative("sequential?", isKind<Kind::List, Kind::Vector>);
	machine.defineNative("associative?", isKind<Kind::Map, Kind::Vector>);
	machine.defineNative("ident?", isKind<Kind::Keyword, Kind::Symbol>);
	machine.defineNative("ifn?",
	                     isKind<Kind::Function, Kind::Native, Kind::Keyword, Kind::Map, Kind::Set, Kind::Vector>);
}

} // namespace

void defineNumbers(Machine& machine) {
	defineTests(machine);
	machine.defineNative("quot", quotient);
	machine.defineNative("int", toInteger);
	machine.defineNative("double", toDouble);
	machine.defineNative("==", numbersEqual);
	machine.defineNative("boolean", toBoolean);
	machine.defineNative("bit-and", bitAnd);
	machine.defineNative("bit-or", bitOr);
	machine.defineNative("bit-xor", bitXor);
	machine.defineNative("bit-not", bitNot);
	machine.defineNative("bit-shift-left", shiftLeft);
	machine.defineNative("bit-shift-right", shiftRight);
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
	machine.defineNative("compare", compared);
	machine.defineNative("not=", notEqual);
	machine.defineNative("not", negation);
	machine.defineNative("truthy?", expressionTruth);
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
