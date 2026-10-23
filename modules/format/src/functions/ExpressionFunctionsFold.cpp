/// @file
/// @brief Expression functions that fold a list into a value carried from item to item, which the expression sees as
/// `acc` beside `it` and `index`: reduce, scan. A running total, a streak or a smoothed score is one expression.

#include "ExpressionFunctions.hpp"

#include <array>
#include <utility>
#include <vector>

namespace affresco::format::detail {
namespace {

/// @brief Folds the list, the first argument, calling a step with every value so far.
/// @param call [list, expression, initial?].
/// @param step Called with each new value so far.
/// @return The last value so far: the initial value (null without one) for an empty or null list.
/// @throws DocumentError If the list is neither a list nor null, or the expression is invalid.
template <typename Step> Value fold(const Call& call, Step&& step) {
	// The initial value is evaluated once, outside the items, where `acc` means what it means around the call
	Value accumulator = call.size() > 2 ? call.evaluate(2) : Value();
	const Value list = call.evaluate(0);
	if(list.isNull()) return accumulator;
	if(!list.isList()) call.fail("expected a list, got " + list.dump());
	for(std::size_t i = 0; i < list.size(); ++i) {
		accumulator = call.evaluateFold(1, list.item(i), i, accumulator);
		step(accumulator);
	}
	return accumulator;
}

/// @brief `reduce(list, expression, initial?)`.
/// @param call [list, expression, initial?].
/// @return The last value of the expression, which sees the value before it as `acc`.
Value reduce(const Call& call) {
	return fold(call, [](const Value&) {});
}

/// @brief `scan(list, expression, initial?)`.
/// @param call [list, expression, initial?].
/// @return Every value of the expression, one per item: [] for a null list.
Value scan(const Call& call) {
	std::vector<Value> values;
	fold(call, [&](const Value& value) { values.push_back(value); });
	return Value::fromList(std::move(values));
}

/// What these functions say when their arguments aren't [list, expression, initial?].
constexpr std::string_view shape = "expected [list, expression, initial?]";

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "reduce",
	 .arguments = "list, expression, initial?",
	 .about = "Folds the items into one value, the expression's last (it sees the value so far as acc)",
	 .group = Group::Items,
	 .fewest = 2,
	 .most = 3,
	 .decide = reduce,
	 .shape = shape},
    {.name = "scan",
	 .arguments = "list, expression, initial?",
	 .about = "Every value of a fold, one per item (running totals, streaks)",
	 .group = Group::Items,
	 .fewest = 2,
	 .most = 3,
	 .decide = scan,
	 .shape = shape},
});

} // namespace

std::span<const Function> foldFunctions() { return table; }

} // namespace affresco::format::detail
