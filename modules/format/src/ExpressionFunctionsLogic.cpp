/// @file
/// @brief Expression functions: choices, logic and comparisons (`c ? a : b`, `&&`, `||`, `??`, `!`, `==`, `<`, ...).

#include "ExpressionFunctions.hpp"

#include <array>

namespace affresco::format::detail {
namespace {

/// @brief `c ? a : b`: evaluates only the branch it gives.
/// @param call [condition, then, else].
/// @return The branch's value.
Value choose(const Call& call) { return truthy(call.evaluate(0)) ? call.evaluate(1) : call.evaluate(2); }

/// @brief `a && b`: the first operand that is false, else the last one, like JavaScript (D1); later ones aren't
/// evaluated.
/// @param call The operands.
/// @return An operand (true when there are none).
Value both(const Call& call) {
	Value last = Value::fromBoolean(true);
	for(std::size_t i = 0; i < call.size(); ++i) {
		last = call.evaluate(i);
		if(!truthy(last)) return last;
	}
	return last;
}

/// @brief `a || b`: the first operand that is true, else the last one, like JavaScript (D1); later ones aren't
/// evaluated.
/// @param call The operands.
/// @return An operand (false when there are none).
Value either(const Call& call) {
	Value last = Value::fromBoolean(false);
	for(std::size_t i = 0; i < call.size(); ++i) {
		last = call.evaluate(i);
		if(truthy(last)) return last;
	}
	return last;
}

/// @brief `a ?? b`: the first operand that isn't null; later ones aren't evaluated.
/// @param call The operands.
/// @return It, or null.
Value firstPresent(const Call& call) {
	for(std::size_t i = 0; i < call.size(); ++i)
		if(Value value = call.evaluate(i); !value.isNull()) return value;
	return {};
}

/// @brief `!a`.
/// @param call [a].
/// @return Whether a is false.
Value negate(const Call& call) { return Value::fromBoolean(!truthy(call[0])); }

/// @brief `a == b`: as JSON compares (1 == 1.0, lists item by item).
/// @param call [a, b].
/// @return Whether they are equal.
Value equal(const Call& call) { return Value::fromBoolean(call[0] == call[1]); }

/// @brief `a != b`.
/// @param call [a, b].
/// @return Whether they differ.
Value unequal(const Call& call) { return Value::fromBoolean(!(call[0] == call[1])); }

/// @brief Compares two texts alphabetically or two numbers by size.
/// @param call [a, b].
/// @return Negative, zero or positive.
/// @throws DocumentError If they are neither two texts nor two numbers.
int compare(const Call& call) {
	if(call[0].isText() && call[1].isText()) return call[0].text().compare(call[1].text());
	const double a = call.number(0), b = call.number(1);
	return a < b ? -1 : a > b ? 1 : 0;
}

/// @brief `a > b`.
/// @param call [a, b].
/// @return The comparison.
Value greater(const Call& call) { return Value::fromBoolean(compare(call) > 0); }
/// @brief `a < b`.
/// @param call [a, b].
/// @return The comparison.
Value less(const Call& call) { return Value::fromBoolean(compare(call) < 0); }
/// @brief `a >= b`.
/// @param call [a, b].
/// @return The comparison.
Value greaterOrEqual(const Call& call) { return Value::fromBoolean(compare(call) >= 0); }
/// @brief `a <= b`.
/// @param call [a, b].
/// @return The comparison.
Value lessOrEqual(const Call& call) { return Value::fromBoolean(compare(call) <= 0); }

/// @brief `empty(x)`: null, "", [] or {} (false and 0 are values).
/// @param call [x].
/// @return Whether it is empty.
Value empty(const Call& call) {
	const Value& value = call[0];
	return Value::fromBoolean(!truthy(value) && !value.isBoolean() && !value.isNumber());
}

/// @brief `type(x)`.
/// @param call [x].
/// @return "null", "boolean", "number", "string", "list" or "object".
Value typeName(const Call& call) {
	switch(call[0].type()) {
	case Value::Type::Null:
		return Value::fromText("null");
	case Value::Type::Boolean:
		return Value::fromText("boolean");
	case Value::Type::Number:
		return Value::fromText("number");
	case Value::Type::Text:
		return Value::fromText("string");
	case Value::Type::List:
		return Value::fromText("list");
	case Value::Type::Object:
		break;
	}
	return Value::fromText("object");
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "if",
	 .arguments = "condition, then, else",
	 .about = "then if the condition is true, else else; evaluates only that one",
	 .group = Group::Syntax,
	 .fewest = 3,
	 .most = 3,
	 .decide = choose,
	 .shape = "expected [condition, then, else]"},
    {.name = "and",
	 .arguments = "a, b, ...",
	 .about = "The first operand that is false, else the last one (a && b)",
	 .group = Group::Syntax,
	 .decide = both,
	 .shape = "expected an array of conditions"},
    {.name = "or",
	 .arguments = "a, b, ...",
	 .about = "The first operand that is true, else the last one (a || b)",
	 .group = Group::Syntax,
	 .decide = either,
	 .shape = "expected an array of conditions"},
    {.name = "not",
	 .arguments = "value",
	 .about = "Whether the value is false (!a)",
	 .group = Group::Syntax,
	 .takes = Takes::One,
	 .apply = negate},
    {.name = "coalesce",
	 .arguments = "a, b, ...",
	 .about = "The first operand that isn't null (a ?? b)",
	 .group = Group::Syntax,
	 .decide = firstPresent,
	 .shape = "expected an array of values"},
    {.name = "equals",
	 .arguments = "a, b",
	 .about = "Whether they are equal (a == b)",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = equal},
    {.name = "notEquals",
	 .arguments = "a, b",
	 .about = "Whether they differ (a != b)",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = unequal},
    {.name = "greater",
	 .arguments = "a, b",
	 .about = "a > b: two numbers, or two texts alphabetically",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = greater},
    {.name = "less",
	 .arguments = "a, b",
	 .about = "a < b",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = less},
    {.name = "greaterOrEqual",
	 .arguments = "a, b",
	 .about = "a >= b",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = greaterOrEqual},
    {.name = "lessOrEqual",
	 .arguments = "a, b",
	 .about = "a <= b",
	 .group = Group::Syntax,
	 .fewest = 2,
	 .most = 2,
	 .apply = lessOrEqual},
    {.name = "empty",
	 .arguments = "value",
	 .about = "Whether it is null, \"\", [] or {}",
	 .takes = Takes::One,
	 .apply = empty},
    {.name = "type",
	 .arguments = "value",
	 .about = R"("null", "boolean", "number", "string", "list" or "object")",
	 .takes = Takes::One,
	 .apply = typeName},
});

} // namespace

std::span<const Function> logicFunctions() { return table; }

} // namespace affresco::format::detail
