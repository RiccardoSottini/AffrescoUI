/// @file
/// @brief Expression functions of a list and an expression evaluated once per item, which sees the item as `it` and
/// its position as `index`: filter, map, count, sum, find, some, every, sort.

#include "ExpressionFunctions.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace affresco::format::detail {
namespace {

/// @brief Evaluates the list, the first argument.
/// @param call [list, expression, ...].
/// @return The list, or std::nullopt if it is null (each function then gives its own empty result).
/// @throws DocumentError If it is neither a list nor null.
std::optional<Value> listOf(const Call& call) {
	Value list = call.evaluate(0);
	if(list.isNull()) return std::nullopt;
	if(!list.isList()) call.fail("expected a list, got " + list.dump());
	return list;
}

/// @brief The expression's value for one item, or the item itself if there is no expression.
/// @param call [list, expression?, ...].
/// @param list The list.
/// @param index The item's position.
/// @return The value.
Value each(const Call& call, const Value& list, std::size_t index) {
	const Value item = list.item(index);
	return call.size() < 2 ? item : call.evaluateFor(1, item, index);
}

/// @brief `filter(list, condition)`.
/// @param call [list, condition].
/// @return The items for which it is true.
Value filter(const Call& call) {
	const std::optional<Value> list = listOf(call);
	std::vector<Value> result;
	for(std::size_t i = 0; list && i < list->size(); ++i)
		if(truthy(each(call, *list, i))) result.push_back(list->item(i));
	return Value::fromList(std::move(result));
}

/// @brief `map(list, expression)`.
/// @param call [list, expression].
/// @return Its value for every item.
Value map(const Call& call) {
	const std::optional<Value> list = listOf(call);
	std::vector<Value> result;
	if(list) result.reserve(list->size());
	for(std::size_t i = 0; list && i < list->size(); ++i) result.push_back(each(call, *list, i));
	return Value::fromList(std::move(result));
}

/// @brief `count(list, condition?)`.
/// @param call [list, condition?].
/// @return How many items it is true for (all of them without one).
Value count(const Call& call) {
	const std::optional<Value> list = listOf(call);
	if(!list) return Value::fromInteger(0);
	if(call.size() < 2) return Value::fromInteger(static_cast<std::int64_t>(list->size()));
	std::int64_t n = 0;
	for(std::size_t i = 0; i < list->size(); ++i)
		if(truthy(each(call, *list, i))) ++n;
	return Value::fromInteger(n);
}

/// @brief `sum(list, expression?)`.
/// @param call [list, expression?].
/// @return The sum of the expression's values (of the items without one).
Value sum(const Call& call) {
	const std::optional<Value> list = listOf(call);
	if(!list) return Value::fromInteger(0);
	double total = 0.0;
	bool integral = true;
	for(std::size_t i = 0; i < list->size(); ++i) {
		const Value value = each(call, *list, i);
		if(!value.isNumber()) call.fail("expected a number, got " + value.dump());
		total += value.number();
		integral = integral && value.isInteger();
	}
	return numeric(total, integral);
}

/// @brief `find(list, condition)`.
/// @param call [list, condition].
/// @return The first item it is true for, or null.
Value find(const Call& call) {
	const std::optional<Value> list = listOf(call);
	for(std::size_t i = 0; list && i < list->size(); ++i)
		if(truthy(each(call, *list, i))) return list->item(i);
	return {};
}

/// @brief `some(list, condition)` or `every(list, condition)`.
/// @param call [list, condition].
/// @param some true for some, false for every.
/// @return Whether it is true for some item, or for every one; [] for a null list.
Value any(const Call& call, bool some) {
	const std::optional<Value> list = listOf(call);
	if(!list) return Value::fromList({});
	for(std::size_t i = 0; i < list->size(); ++i)
		if(truthy(each(call, *list, i)) == some) return Value::fromBoolean(some);
	return Value::fromBoolean(!some);
}

/// @brief `some(list, condition)`.
/// @param call [list, condition].
/// @return Whether it is true for some item.
Value some(const Call& call) { return any(call, true); }

/// @brief `every(list, condition)`.
/// @param call [list, condition].
/// @return Whether it is true for every item.
Value every(const Call& call) { return any(call, false); }

/// @brief `sort(list, key?, 'desc'?)`: stable; numbers by size, texts alphabetically.
/// @param call [list, key?, direction?].
/// @return The items, sorted by the key's value (by themselves without one).
Value sort(const Call& call) {
	const std::optional<Value> list = listOf(call);
	if(!list) return Value::fromList({});
	std::vector<std::pair<Value, std::size_t>> keyed;
	keyed.reserve(list->size());
	for(std::size_t i = 0; i < list->size(); ++i) keyed.emplace_back(each(call, *list, i), i);
	const bool descending = call.size() > 2 && call.evaluate(2).toText() == "desc";
	std::ranges::stable_sort(keyed, [&](const auto& a, const auto& b) {
		const int c = order(a.first, b.first);
		return descending ? c > 0 : c < 0;
	});
	std::vector<Value> result;
	result.reserve(keyed.size());
	for(const auto& entry : keyed) result.push_back(list->item(entry.second));
	return Value::fromList(std::move(result));
}

/// What these functions say when their arguments aren't [list, expression, ...].
constexpr std::string_view shape = "expected [list, expression]";

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "filter",
	 .arguments = "list, condition",
	 .about = "The items for which the condition is true",
	 .group = Group::Items,
	 .fewest = 1,
	 .decide = filter,
	 .shape = shape},
    {.name = "map",
	 .arguments = "list, expression",
	 .about = "The expression's value for every item",
	 .group = Group::Items,
	 .fewest = 1,
	 .decide = map,
	 .shape = shape},
    {.name = "count",
	 .arguments = "list, condition?",
	 .about = "How many items the condition is true for",
	 .group = Group::Items,
	 .fewest = 1,
	 .decide = count,
	 .shape = shape},
    {.name = "sum",
	 .arguments = "list, expression?",
	 .about = "The sum of the expression's values",
	 .group = Group::Items,
	 .fewest = 1,
	 .decide = sum,
	 .shape = shape},
    {.name = "find",
	 .arguments = "list, condition",
	 .about = "The first item the condition is true for, or null",
	 .group = Group::Items,
	 .fewest = 1,
	 .decide = find,
	 .shape = shape},
    {.name = "some",
	 .arguments = "list, condition",
	 .about = "Whether the condition is true for some item",
	 .group = Group::Items,
	 .fewest = 1,
	 .decide = some,
	 .shape = shape},
    {.name = "every",
	 .arguments = "list, condition",
	 .about = "Whether the condition is true for every item",
	 .group = Group::Items,
	 .fewest = 1,
	 .decide = every,
	 .shape = shape},
    {.name = "sort",
	 .arguments = "list, key?, 'desc'?",
	 .about = "The items sorted by the key (stable)",
	 .group = Group::Items,
	 .fewest = 1,
	 .decide = sort,
	 .shape = shape},
});

} // namespace

std::span<const Function> itemFunctions() { return table; }

} // namespace affresco::format::detail
