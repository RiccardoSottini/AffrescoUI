/// @file
/// @brief The language's own functions on lists, vectors and maps. What calls a function back (`map`, `reduce`...) is
/// written in Lisp (Prelude.cpp), so that it runs on the machine's stack and not the native one.

#include "Machine.hpp"
#include "Sequences.hpp"
#include "Sets.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

void needCount(std::span<const Value> arguments, std::size_t least, std::size_t most, std::string_view function) {
	if(arguments.size() < least || arguments.size() > most)
		throw Problem("\"" + std::string(function) + "\" was given " + std::to_string(arguments.size()) + " arguments");
}

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// @brief A map's [key value] pairs.
/// @param machine The machine.
/// @param map The map.
/// @return The vector.
Value pairsOf(Machine& machine, const Value& map) {
	std::vector<Value> pairs;
	pairs.reserve(map.count());
	machine.spend(map.count());
	for(std::size_t at = 0; at < map.count(); ++at) {
		const std::vector<Value> pair = {machine.heap.string(map.keyAt(at)), map.valueAt(at)};
		pairs.push_back(machine.heap.sequence(Kind::Vector, std::span<const Value>(pair)));
	}
	return machine.heap.sequence(Kind::Vector, std::move(pairs));
}

/// @brief A string's characters.
/// @param machine The machine.
/// @param text The string.
/// @return The vector of one-character strings.
Value charactersOf(Machine& machine, std::string_view text) {
	std::vector<Value> characters;
	std::size_t start = 0;
	while(start < text.size()) {
		std::size_t end = start + 1;
		while(end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) ++end;
		characters.push_back(machine.heap.string(text.substr(start, end - start)));
		start = end;
	}
	machine.spend(characters.size());
	return machine.heap.sequence(Kind::Vector, std::move(characters));
}

} // namespace

Value sequenceOf(Machine& machine, const Value& value, std::string_view function) {
	switch(value.kind()) {
	case Kind::Nil:
		return machine.heap.sequence(Kind::Vector, std::vector<Value>());
	case Kind::List:
	case Kind::Vector:
		return value;
	case Kind::Map:
		return pairsOf(machine, value);
	case Kind::Set: // its members in the order they were added, without a copy
		return machine.heap.setItems(value);
	case Kind::String:
		return charactersOf(machine, value.text());
	default:
		throw Problem("\"" + std::string(function) + "\" takes something with items, and was given " +
		              std::string(describeKind(value)));
	}
}

std::string_view textKey(const Value& key, std::string_view fix) {
	if(key.kind() != Kind::String && key.kind() != Kind::Keyword)
		throw Problem("a map's key is a keyword or a string, not " + std::string(describeKind(key)) +
		              ": give the key as text, " + std::string(fix));
	return key.text();
}

namespace {

/// @brief `%seq`. @param machine The machine. @param arguments A value. @return It as a list or a vector.
Value toSequence(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "seq");
	return sequenceOf(machine, arguments[0], "map");
}

/// @brief `first`. @param machine The machine. @param arguments A collection. @return Its first item, or nil.
Value firstItem(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "first");
	const Value items = sequenceOf(machine, arguments[0], "first");
	return items.count() == 0 ? Value() : items.at(0);
}

/// @brief `rest`. @param machine The machine. @param arguments A collection. @return All but its first item.
Value restOf(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "rest");
	const Value items = sequenceOf(machine, arguments[0], "rest");
	return items.count() == 0 ? items : machine.heap.part(items, 1, items.count() - 1);
}

/// @brief `cons`. @param machine The machine. @param arguments An item and a collection. @return The list of both.
Value cons(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "cons");
	const Value items = sequenceOf(machine, arguments[1], "cons");
	std::vector<Value> made;
	machine.spend(items.count());
	made.reserve(items.count() + 1);
	made.push_back(arguments[0]);
	for(std::size_t at = 0; at < items.count(); ++at) made.push_back(items.at(at));
	return machine.heap.sequence(Kind::List, std::move(made));
}

/// @brief A copy of a map's members.
/// @param machine The machine.
/// @param map The map, or nil.
/// @param extra Room for more.
/// @return The members.
std::vector<MapEntry> membersOf(Machine& machine, const Value& map, std::size_t extra = 0) {
	std::vector<MapEntry> members;
	machine.spend(map.count());
	machine.heap.charge((map.count() + extra) * sizeof(MapEntry));
	members.reserve(map.count() + extra);
	for(std::size_t at = 0; at < map.count(); ++at) members.push_back({map.keyAt(at), map.valueAt(at)});
	return members;
}

/// @brief Sets a member of a map's members: a key it has keeps its place.
/// @param members The members.
/// @param key The key.
/// @param value The value.
void setMember(std::vector<MapEntry>& members, std::string_view key, const Value& value) {
	const auto found = std::ranges::find(members, key, &MapEntry::key);
	if(found != members.end()) found->value = value;
	else members.push_back({key, value});
}

} // namespace

/// @brief `conj` onto a map, `into` a map too: each item a `[key value]` pair or a map, whose members it sets.
/// @param machine The machine.
/// @param arguments The map and the items.
/// @return The map with them.
Value conjMap(Machine& machine, Arguments arguments) {
	std::vector<MapEntry> members = membersOf(machine, arguments[0], arguments.size() - 1);
	const auto set = [&](const Value& key, const Value& value) {
		machine.spend(members.size());
		setMember(members, machine.heap.keep(textKey(key, "(conj m [(str key) value])")), value);
	};
	for(const Value& item : arguments.subspan(1)) {
		if(item.kind() == Kind::Map) {
			for(std::size_t at = 0; at < item.count(); ++at) set(machine.heap.string(item.keyAt(at)), item.valueAt(at));
		} else if(item.kind() == Kind::Vector && item.count() == 2) {
			set(item.at(0), item.at(1));
		} else if(!item.isNil()) {
			throw Problem("\"conj\" onto a map takes [key value] pairs or maps, and was given " +
			              std::string(describeKind(item)));
		}
	}
	return machine.heap.map(std::move(members));
}

namespace {

/// @brief `conj`: on a vector or nil adds at the end, without copying when it can; on a list at the front; on a map
/// sets members (conjMap()); on a set adds those that aren't members, at the end.
/// @param machine The machine.
/// @param arguments A collection and the items.
/// @return The collection with them.
Value conj(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, static_cast<std::size_t>(-1), "conj");
	if(arguments[0].kind() == Kind::Map) return conjMap(machine, arguments);
	if(arguments[0].kind() == Kind::Set) {
		Value set = arguments[0];
		for(const Value& item : arguments.subspan(1)) set = setWith(machine, set, item);
		return set;
	}
	Value result = arguments[0].isNil() ? machine.heap.sequence(Kind::Vector, std::vector<Value>()) : arguments[0];
	if(!result.isSequence())
		throw Problem("\"conj\" takes a list, a vector, a set or a map, and was given " +
		              std::string(describeKind(result)));
	for(const Value& item : arguments.subspan(1)) {
		if(result.kind() == Kind::Vector) {
			result = machine.heap.appended(result, item);
			continue;
		}
		const std::vector<Value> both = {item, result};
		result = cons(machine, std::span<const Value>(both));
	}
	return result;
}

/// @brief `assoc` on a vector: the same vector with an item replaced, or one added past the end.
/// @param machine The machine.
/// @param arguments The vector, then indexes and items.
/// @return The vector.
Value assocVector(Machine& machine, Arguments arguments) {
	std::vector<Value> items;
	machine.spend(arguments[0].count());
	items.reserve(arguments[0].count() + 1);
	for(std::size_t at = 0; at < arguments[0].count(); ++at) items.push_back(arguments[0].at(at));
	for(std::size_t at = 1; at + 1 < arguments.size(); at += 2) {
		const std::int64_t index = wholeNumber(arguments[at]).value_or(-1);
		if(index < 0 || static_cast<std::size_t>(index) > items.size())
			throw Problem("\"assoc\": an index outside the vector");
		if(static_cast<std::size_t>(index) == items.size()) items.push_back(arguments[at + 1]);
		else items[static_cast<std::size_t>(index)] = arguments[at + 1];
	}
	return machine.heap.sequence(Kind::Vector, std::move(items));
}

/// @brief `assoc`.
/// @param machine The machine.
/// @param arguments A map or nil, then keys and values; or a vector, then indexes and items.
/// @return The collection with them.
Value assoc(Machine& machine, Arguments arguments) {
	if(arguments.size() < 3 || arguments.size() % 2 == 0)
		throw Problem("\"assoc\" takes a map and pairs of a key and a value");
	if(arguments[0].kind() == Kind::Vector) return assocVector(machine, arguments);
	if(arguments[0].kind() != Kind::Map && !arguments[0].isNil())
		throw Problem("\"assoc\" takes a map or a vector, and was given " + std::string(describeKind(arguments[0])));
	std::vector<MapEntry> members = membersOf(machine, arguments[0], (arguments.size() - 1) / 2);
	for(std::size_t at = 1; at + 1 < arguments.size(); at += 2) {
		machine.spend(members.size());
		setMember(members, machine.heap.keep(keyText(arguments[at])), arguments[at + 1]);
	}
	return machine.heap.map(std::move(members));
}

/// @brief `dissoc`. @param machine The machine. @param arguments A map or nil, and keys. @return The map without them.
Value dissoc(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, static_cast<std::size_t>(-1), "dissoc");
	if(arguments[0].kind() != Kind::Map && !arguments[0].isNil())
		throw Problem("\"dissoc\" takes a map, and was given " + std::string(describeKind(arguments[0])));
	std::vector<MapEntry> members = membersOf(machine, arguments[0]);
	for(const Value& key : arguments.subspan(1)) {
		machine.spend(members.size());
		std::erase_if(members, [text = keyText(key)](const MapEntry& member) { return member.key == text; });
	}
	return machine.heap.map(std::move(members));
}

/// @brief `get` (also `%get`, which a program can't redefine).
/// @param machine The machine.
/// @param arguments A map, a vector, a set or nil, a key, an index or an item, and maybe what to give if it isn't
/// there.
/// @return The member, or what was given, or nil.
Value getMember(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 3, "get");
	const Value& from = arguments[0];
	const Value fallback = arguments.size() == 3 ? arguments[2] : Value();
	if(from.isNil()) return fallback;
	if(from.kind() == Kind::Map) {
		bool found = false;
		const std::string_view key = keyText(arguments[1]);
		machine.spendLookup(from, key);
		const Value member = from.lookup(key, &found);
		return found ? member : fallback;
	}
	if(from.kind() == Kind::Set) return setMember(machine, from, arguments[1]).value_or(fallback);
	if(from.isSequence()) {
		const std::int64_t index = wholeNumber(arguments[1]).value_or(-1);
		return index >= 0 && static_cast<std::size_t>(index) < from.count() ? from.at(static_cast<std::size_t>(index))
		                                                                    : fallback;
	}
	throw Problem("\"get\" takes a map, a vector or a set, and was given " + std::string(describeKind(from)));
}

/// @brief `nth`: an index outside it is an error, unless a default is given.
/// @param machine The machine.
/// @param arguments A collection, an index, maybe a default.
/// @return The item.
Value nthItem(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 3, "nth");
	const Value items = sequenceOf(machine, arguments[0], "nth");
	const std::optional<std::int64_t> whole = wholeNumber(arguments[1]);
	if(!whole) throw Problem("\"nth\" takes a whole number as its index");
	const std::int64_t index = *whole;
	if(index >= 0 && static_cast<std::size_t>(index) < items.count()) return items.at(static_cast<std::size_t>(index));
	if(arguments.size() == 3) return arguments[2];
	throw Problem("\"nth\": an index outside the collection");
}

/// @brief `%nth`: what a vector pattern binds: nil past the end.
/// @param machine The machine.
/// @param arguments A collection and an index.
/// @return The item, or nil.
Value patternItem(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "%nth");
	const std::int64_t whole = wholeNumber(arguments[1]).value_or(-1);
	if(whole < 0) throw Problem("\"%nth\" takes an index");
	const Value items = sequenceOf(machine, arguments[0], "a vector pattern");
	const auto index = static_cast<std::size_t>(whole);
	return index < items.count() ? items.at(index) : Value();
}

/// @brief `%drop`: what `& more` binds in a vector pattern.
/// @param machine The machine.
/// @param arguments A collection and an index.
/// @return Its items from there.
Value patternRest(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "%drop");
	const std::int64_t whole = wholeNumber(arguments[1]).value_or(-1);
	if(whole < 0) throw Problem("\"%drop\" takes an index");
	const Value items = sequenceOf(machine, arguments[0], "a vector pattern");
	const auto index = static_cast<std::size_t>(whole);
	return index < items.count() ? machine.heap.part(items, index, items.count() - index)
	                             : machine.heap.sequence(items.kind(), std::vector<Value>());
}

/// @brief `merge`. @param machine The machine. @param arguments Maps or nils. @return One map, the last's values
/// winning.
Value merge(Machine& machine, Arguments arguments) {
	std::vector<MapEntry> members;
	for(const Value& map : arguments) {
		if(map.isNil()) continue;
		if(map.kind() != Kind::Map)
			throw Problem("\"merge\" takes maps, and was given " + std::string(describeKind(map)));
		machine.spend(map.count() * (members.size() + 1));
		machine.heap.charge(map.count() * sizeof(MapEntry));
		for(std::size_t at = 0; at < map.count(); ++at) setMember(members, map.keyAt(at), map.valueAt(at));
	}
	return machine.heap.map(std::move(members));
}

/// @brief `keys`. @param machine The machine. @param arguments A map or nil. @return Its keys, as strings.
Value keysOf(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "keys");
	if(arguments[0].kind() != Kind::Map && !arguments[0].isNil())
		throw Problem("\"keys\" takes a map, and was given " + std::string(describeKind(arguments[0])));
	std::vector<Value> keys;
	machine.spend(arguments[0].count());
	keys.reserve(arguments[0].count());
	for(std::size_t at = 0; at < arguments[0].count(); ++at)
		keys.push_back(machine.heap.string(arguments[0].keyAt(at)));
	return machine.heap.sequence(Kind::Vector, std::move(keys));
}

/// @brief `vals`. @param machine The machine. @param arguments A map or nil. @return Its values.
Value valuesOf(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "vals");
	if(arguments[0].kind() != Kind::Map && !arguments[0].isNil())
		throw Problem("\"vals\" takes a map, and was given " + std::string(describeKind(arguments[0])));
	std::vector<Value> values;
	machine.spend(arguments[0].count());
	values.reserve(arguments[0].count());
	for(std::size_t at = 0; at < arguments[0].count(); ++at) values.push_back(arguments[0].valueAt(at));
	return machine.heap.sequence(Kind::Vector, std::move(values));
}

/// @brief `count`.
/// @param machine The machine.
/// @param arguments A collection, a string or nil.
/// @return How many items or characters.
Value countOf(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "count");
	const Value& value = arguments[0];
	if(value.kind() == Kind::String) {
		const std::string_view text = value.text();
		machine.spendText(text.size());
		const auto characters =
		    std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; });
		return Value::integer(characters);
	}
	if(value.isNil() || value.isSequence() || value.kind() == Kind::Map || value.kind() == Kind::Set)
		return Value::integer(static_cast<std::int64_t>(value.count()));
	throw Problem("\"count\" takes a collection or a string, and was given " + std::string(describeKind(value)));
}

/// @brief `empty?`. @param machine The machine. @param arguments A collection, a string or nil. @return Whether it has
/// nothing.
Value isEmpty(Machine& machine, Arguments arguments) {
	return Value::boolean(countOf(machine, arguments).asInteger() == 0);
}

/// @brief `contains?`.
/// @param machine The machine.
/// @param arguments A map (with a key), a vector (with an index), a set (with an item), or nil.
/// @return Whether it has it.
Value containsMember(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "contains?");
	if(arguments[0].kind() == Kind::Map) {
		bool found = false;
		const std::string_view key = keyText(arguments[1]);
		machine.spendLookup(arguments[0], key);
		static_cast<void>(arguments[0].lookup(key, &found));
		return Value::boolean(found);
	}
	if(arguments[0].kind() == Kind::Set)
		return Value::boolean(setMember(machine, arguments[0], arguments[1]).has_value());
	if(const std::optional<std::int64_t> index = wholeNumber(arguments[1]); arguments[0].isSequence() && index)
		return Value::boolean(*index >= 0 && static_cast<std::size_t>(*index) < arguments[0].count());
	return Value::boolean(false);
}

/// @brief `list`. @param machine The machine. @param arguments The items. @return The list.
Value makeList(Machine& machine, Arguments arguments) { return machine.heap.sequence(Kind::List, arguments); }

/// @brief `vec`. @param machine The machine. @param arguments A collection. @return Its items as a vector.
Value makeVector(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "vec");
	const Value items = sequenceOf(machine, arguments[0], "vec");
	std::vector<Value> made;
	machine.spend(items.count());
	made.reserve(items.count());
	for(std::size_t at = 0; at < items.count(); ++at) made.push_back(items.at(at));
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

/// @brief `concat`. @param machine The machine. @param arguments Collections. @return A vector of all their items.
Value concat(Machine& machine, Arguments arguments) {
	std::vector<Value> made;
	for(const Value& collection : arguments) {
		const Value items = sequenceOf(machine, collection, "concat");
		machine.spend(items.count());
		machine.heap.charge(items.count() * sizeof(Value));
		for(std::size_t at = 0; at < items.count(); ++at) made.push_back(items.at(at));
	}
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

/// @brief `reverse`. @param machine The machine. @param arguments A collection. @return A vector of its items,
/// reversed.
Value reversed(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "reverse");
	const Value items = sequenceOf(machine, arguments[0], "reverse");
	std::vector<Value> made;
	machine.spend(items.count());
	made.reserve(items.count());
	for(std::size_t at = items.count(); at > 0; --at) made.push_back(items.at(at - 1));
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

/// @brief `range`.
/// @param machine The machine.
/// @param arguments An end; a start and an end; or a start, an end and a step.
/// @return The vector of integers, within the budget.
Value range(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 3, "range");
	std::array<std::int64_t, 3> whole{};
	for(std::size_t at = 0; at < arguments.size(); ++at) {
		const std::optional<std::int64_t> number = wholeNumber(arguments[at]);
		if(!number) throw Problem("\"range\" takes whole numbers");
		whole.at(at) = *number;
	}
	const std::int64_t start = arguments.size() > 1 ? whole[0] : 0;
	const std::int64_t end = arguments.size() > 1 ? whole[1] : whole[0];
	const std::int64_t step = arguments.size() > 2 ? whole[2] : 1;
	if(step == 0) throw Problem("\"range\": a step of zero never ends");
	const double steps = (static_cast<double>(end) - static_cast<double>(start)) / static_cast<double>(step);
	const std::size_t size = steps > 0 ? static_cast<std::size_t>(std::min(steps + 0.999999, 1e15)) : 0;
	machine.spend(size);
	machine.heap.charge(size * sizeof(Value));
	std::vector<Value> made;
	made.reserve(size);
	// In unsigned arithmetic, which wraps: each result fits, though the product on the way to it may not
	for(std::size_t at = 0; at < size; ++at)
		made.push_back(Value::integer(static_cast<std::int64_t>(
		    static_cast<std::uint64_t>(start) + static_cast<std::uint64_t>(at) * static_cast<std::uint64_t>(step))));
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

/// @brief The natural order of two numbers or two texts: what isn't a number comes after all that are, so that it is
/// an order, as sorting needs. A keyword is the string of its text (decision D2). Comparing costs what the texts' sizes
/// cost.
/// @param machine The machine.
/// @param first One.
/// @param second The other.
/// @return Whether one comes first.
bool comesFirst(Machine& machine, const Value& first, const Value& second) {
	if(first.isNumber() && second.isNumber()) {
		const double a = first.asDouble();
		const double b = second.asDouble();
		return std::isnan(a) || std::isnan(b) ? !std::isnan(a) && std::isnan(b) : a < b;
	}
	const auto text = [](const Value& value) { return value.kind() == Kind::String || value.kind() == Kind::Keyword; };
	if(text(first) && text(second)) {
		machine.spendText(std::min(first.text().size(), second.text().size()));
		return first.text() < second.text();
	}
	throw Problem("\"sort\" compares numbers with numbers and strings with strings");
}

/// @brief The orders the language sorts in natively: its own `<`, `>` and `compare` as a comparator, or none.
enum class Order : std::uint8_t { Natural, Rising, Falling, Compared };

/// @brief The order a comparator stands for, if the language sorts in it natively.
/// @param machine The machine.
/// @param comparator The comparator.
/// @return The order, or nothing for any other function, which a sort written in Lisp calls.
std::optional<Order> nativeOrder(const Machine& machine, const Value& comparator) {
	constexpr std::array<std::pair<std::string_view, Order>, 3> orders{
	    {{"<", Order::Rising}, {">", Order::Falling}, {"compare", Order::Compared}}};
	if(comparator.kind() != Kind::Native) return std::nullopt;
	for(const auto& [name, order] : orders)
		if(const Global* own = machine.findLanguageGlobal(name); own && own->value.pointer() == comparator.pointer())
			return order;
	return std::nullopt;
}

/// @brief Whether one value comes before another in an order.
/// @param machine The machine.
/// @param order The order.
/// @param first One.
/// @param second The other.
/// @return true if it does.
bool inOrder(Machine& machine, Order order, const Value& first, const Value& second) {
	if(order == Order::Natural) return comesFirst(machine, first, second);
	if(order == Order::Compared) return compareValues(machine, first, second, "compare") < 0;
	const std::string_view name = order == Order::Rising ? "<" : ">";
	for(const Value* value : {&first, &second})
		if(!value->isNumber())
			throw Problem("\"" + std::string(name) + "\" takes numbers, and was given " +
			              std::string(describeKind(*value)));
	const Value& earlier = order == Order::Rising ? first : second; // `>` puts the larger first
	const Value& later = order == Order::Rising ? second : first;
	return compareValues(machine, earlier, later, name) < 0;
}

/// @brief The order of a native sort's arguments: a comparator before the rest, or none.
/// @param machine The machine.
/// @param arguments The arguments.
/// @param plain How many there are without a comparator.
/// @param function The function, for an error.
/// @return The order.
Order orderOf(const Machine& machine, Arguments arguments, std::size_t plain, std::string_view function) {
	needCount(arguments, plain, plain + 1, function);
	if(arguments.size() == plain) return Order::Natural;
	const std::optional<Order> order = nativeOrder(machine, arguments[0]);
	if(!order)
		throw Problem("\"" + std::string(function) + "\" takes the language's <, > or compare as its comparator");
	return *order;
}

/// @brief `%sort`: what `sort` does with no comparator, or with one the language sorts in natively.
/// @param machine The machine.
/// @param arguments Maybe `<`, `>` or `compare`, then a collection.
/// @return A vector of its items, in order: a stable sort.
Value sorted(Machine& machine, Arguments arguments) {
	const Order order = orderOf(machine, arguments, 1, "sort");
	const Value items = sequenceOf(machine, arguments.back(), "sort");
	std::vector<Value> made;
	machine.spend(items.count() * 16);
	made.reserve(items.count());
	for(std::size_t at = 0; at < items.count(); ++at) made.push_back(items.at(at));
	std::ranges::stable_sort(made, [&](const Value& a, const Value& b) { return inOrder(machine, order, a, b); });
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

/// @brief `%sort-by-keys`: what `sort-by` ends with.
/// @param machine The machine.
/// @param arguments Maybe `<`, `>` or `compare`, then a vector of keys and a vector of items as long.
/// @return The items, in the order of their keys.
Value sortedByKeys(Machine& machine, Arguments arguments) {
	const Order order = orderOf(machine, arguments, 2, "%sort-by-keys");
	const Value& keys = arguments[arguments.size() - 2];
	const Value& items = arguments.back();
	if(!keys.isSequence() || !items.isSequence() || keys.count() != items.count())
		throw Problem("\"%sort-by-keys\" takes two lists as long as each other");
	std::vector<std::pair<Value, Value>> pairs;
	machine.spend(keys.count() * 16);
	pairs.reserve(keys.count());
	for(std::size_t at = 0; at < keys.count(); ++at) pairs.emplace_back(keys.at(at), items.at(at));
	std::ranges::stable_sort(pairs,
	                         [&](const auto& a, const auto& b) { return inOrder(machine, order, a.first, b.first); });
	std::vector<Value> made;
	made.reserve(pairs.size());
	for(const auto& [key, item] : pairs) made.push_back(item);
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

/// @brief `%sorts-natively?`. @param machine The machine. @param arguments A function. @return Whether it is the
/// language's `<`, `>` or `compare`, which `%sort` takes.
// NOLINTNEXTLINE(misc-const-correctness): a native function's signature takes the machine to change
Value sortsNatively(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "%sorts-natively?");
	return Value::boolean(nativeOrder(machine, arguments[0]).has_value());
}

/// @brief `%before?`: what a comparator's result says, for the sort written in Lisp.
/// @param arguments What `(comparator b a)` gave.
/// @return true for true (as `<` gives) or a negative number (as `compare` gives): b comes before a.
Value comesBefore(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 1, 1, "%before?");
	const Value& result = arguments[0];
	if(result.kind() == Kind::Boolean) return result;
	if(result.isNumber()) return Value::boolean(result.asDouble() < 0);
	if(result.isNil()) return Value::boolean(false);
	throw Problem("a comparator gives a boolean or a number, and gave " + std::string(describeKind(result)));
}

} // namespace

void defineCollections(Machine& machine) {
	machine.defineNative("%seq", toSequence);
	machine.defineNative("%nth", patternItem);
	machine.defineNative("%drop", patternRest);
	machine.defineNative("%get", getMember);
	machine.defineNative("%sort-by-keys", sortedByKeys);
	machine.defineNative("%sort", sorted);
	machine.defineNative("%sorts-natively?", sortsNatively);
	machine.defineNative("%before?", comesBefore);
	machine.defineNative("first", firstItem);
	machine.defineNative("rest", restOf);
	machine.defineNative("cons", cons);
	machine.defineNative("conj", conj);
	machine.defineNative("assoc", assoc);
	machine.defineNative("dissoc", dissoc);
	machine.defineNative("get", getMember);
	machine.defineNative("nth", nthItem);
	machine.defineNative("merge", merge);
	machine.defineNative("keys", keysOf);
	machine.defineNative("vals", valuesOf);
	machine.defineNative("count", countOf);
	machine.defineNative("empty?", isEmpty);
	machine.defineNative("contains?", containsMember);
	machine.defineNative("list", makeList);
	machine.defineNative("vec", makeVector);
	machine.defineNative("concat", concat);
	machine.defineNative("reverse", reversed);
	machine.defineNative("range", range);
	machine.defineNative("apply", nullptr, true);
}

} // namespace affresco::lisp
