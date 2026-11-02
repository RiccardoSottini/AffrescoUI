/// @file
/// @brief The language's own functions on lists, vectors and maps. What calls a function back (`map`, `reduce`...) is
/// written in Lisp (Prelude.cpp), so that it runs on the machine's stack and not the native one.

#include "Machine.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// @brief Checks how many arguments a function has.
/// @param arguments Its arguments.
/// @param least The least.
/// @param most The most.
/// @param function The function, for the error.
void needCount(Arguments arguments, std::size_t least, std::size_t most, std::string_view function) {
	if(arguments.size() < least || arguments.size() > most)
		throw Problem("\"" + std::string(function) + "\" was given " + std::to_string(arguments.size()) + " arguments");
}

/// @brief Anything with items as a list or a vector: nil is empty, a map is its [key value] pairs, a string its
/// characters.
/// @param machine The machine.
/// @param value The value.
/// @param function The function, for an error.
/// @return The list or the vector.
Value sequenceOf(Machine& machine, const Value& value, std::string_view function);

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

Value sequenceOf(Machine& machine, const Value& value, std::string_view function) {
	switch(value.kind()) {
	case Kind::Nil:
		return machine.heap.sequence(Kind::Vector, std::vector<Value>());
	case Kind::List:
	case Kind::Vector:
		return value;
	case Kind::Map:
		return pairsOf(machine, value);
	case Kind::String:
		return charactersOf(machine, value.text());
	default:
		throw Problem("\"" + std::string(function) + "\" takes something with items, and was given " +
		              std::string(describeKind(value)));
	}
}

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

/// @brief `conj`: on a vector or nil adds at the end, without copying when it can; on a list at the front.
/// @param machine The machine.
/// @param arguments A collection and the items.
/// @return The collection with them.
Value conj(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, static_cast<std::size_t>(-1), "conj");
	Value result = arguments[0].isNil() ? machine.heap.sequence(Kind::Vector, std::vector<Value>()) : arguments[0];
	if(!result.isSequence())
		throw Problem("\"conj\" takes a list or a vector, and was given " + std::string(describeKind(result)));
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
		const std::int64_t index = arguments[at].kind() == Kind::Integer ? arguments[at].asInteger() : -1;
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
/// @param arguments A map, a vector or nil, a key or an index, and maybe what to give if it isn't there.
/// @return The member, or what was given, or nil.
Value getMember(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 2, 3, "get");
	const Value& from = arguments[0];
	const Value fallback = arguments.size() == 3 ? arguments[2] : Value();
	if(from.isNil()) return fallback;
	if(from.kind() == Kind::Map) {
		bool found = false;
		const Value member = from.lookup(keyText(arguments[1]), &found);
		return found ? member : fallback;
	}
	if(from.isSequence()) {
		const std::int64_t index = arguments[1].kind() == Kind::Integer ? arguments[1].asInteger() : -1;
		return index >= 0 && static_cast<std::size_t>(index) < from.count() ? from.at(static_cast<std::size_t>(index))
		                                                                    : fallback;
	}
	throw Problem("\"get\" takes a map or a vector, and was given " + std::string(describeKind(from)));
}

/// @brief `nth`: an index outside it is an error, unless a default is given.
/// @param machine The machine.
/// @param arguments A collection, an index, maybe a default.
/// @return The item.
Value nthItem(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 3, "nth");
	const Value items = sequenceOf(machine, arguments[0], "nth");
	if(arguments[1].kind() != Kind::Integer) throw Problem("\"nth\" takes an integer index");
	const std::int64_t index = arguments[1].asInteger();
	if(index >= 0 && static_cast<std::size_t>(index) < items.count()) return items.at(static_cast<std::size_t>(index));
	if(arguments.size() == 3) return arguments[2];
	throw Problem("\"nth\": an index outside the collection");
}

/// @brief `%nth`: what a vector pattern binds: nil past the end.
/// @param machine The machine.
/// @param arguments A collection and an index.
/// @return The item, or nil.
Value patternItem(Machine& machine, Arguments arguments) {
	const Value items = sequenceOf(machine, arguments[0], "a vector pattern");
	const auto index = static_cast<std::size_t>(arguments[1].asInteger());
	return index < items.count() ? items.at(index) : Value();
}

/// @brief `%drop`: what `& more` binds in a vector pattern.
/// @param machine The machine.
/// @param arguments A collection and an index.
/// @return Its items from there.
Value patternRest(Machine& machine, Arguments arguments) {
	const Value items = sequenceOf(machine, arguments[0], "a vector pattern");
	const auto index = static_cast<std::size_t>(arguments[1].asInteger());
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

/// @brief `count`. @param arguments A collection, a string or nil. @return How many items or characters.
Value countOf(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 1, 1, "count");
	const Value& value = arguments[0];
	if(value.kind() == Kind::String) {
		const std::string_view text = value.text();
		const auto characters =
		    std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; });
		return Value::integer(characters);
	}
	if(value.isNil() || value.isSequence() || value.kind() == Kind::Map)
		return Value::integer(static_cast<std::int64_t>(value.count()));
	throw Problem("\"count\" takes a collection or a string, and was given " + std::string(describeKind(value)));
}

/// @brief `empty?`. @param machine The machine. @param arguments A collection, a string or nil. @return Whether it has
/// nothing.
Value isEmpty(Machine& machine, Arguments arguments) {
	return Value::boolean(countOf(machine, arguments).asInteger() == 0);
}

/// @brief `contains?`.
/// @param arguments A map (with a key) or a vector (with an index), or nil.
/// @return Whether it has it.
Value containsMember(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 2, 2, "contains?");
	if(arguments[0].kind() == Kind::Map) {
		bool found = false;
		static_cast<void>(arguments[0].lookup(keyText(arguments[1]), &found));
		return Value::boolean(found);
	}
	if(arguments[0].isSequence() && arguments[1].kind() == Kind::Integer)
		return Value::boolean(arguments[1].asInteger() >= 0 &&
		                      static_cast<std::size_t>(arguments[1].asInteger()) < arguments[0].count());
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
	for(const Value& argument : arguments)
		if(argument.kind() != Kind::Integer) throw Problem("\"range\" takes integers");
	const std::int64_t start = arguments.size() > 1 ? arguments[0].asInteger() : 0;
	const std::int64_t end = arguments.size() > 1 ? arguments[1].asInteger() : arguments[0].asInteger();
	const std::int64_t step = arguments.size() > 2 ? arguments[2].asInteger() : 1;
	if(step == 0) throw Problem("\"range\": a step of zero never ends");
	const double steps = (static_cast<double>(end) - static_cast<double>(start)) / static_cast<double>(step);
	const std::size_t size = steps > 0 ? static_cast<std::size_t>(std::min(steps + 0.999999, 1e15)) : 0;
	machine.spend(size);
	machine.heap.charge(size * sizeof(Value));
	std::vector<Value> made;
	made.reserve(size);
	for(std::size_t at = 0; at < size; ++at)
		made.push_back(Value::integer(start + static_cast<std::int64_t>(at) * step));
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

/// @brief The natural order of two numbers or two strings.
/// @param first One.
/// @param second The other.
/// @return Whether one comes first.
bool comesFirst(const Value& first, const Value& second) {
	if(first.isNumber() && second.isNumber()) return first.asDouble() < second.asDouble();
	if(first.kind() == Kind::String && second.kind() == Kind::String) return first.text() < second.text();
	throw Problem("\"sort\" compares numbers with numbers and strings with strings");
}

/// @brief `sort`. @param machine The machine. @param arguments A collection. @return A vector of its items, in order.
Value sorted(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "sort");
	const Value items = sequenceOf(machine, arguments[0], "sort");
	std::vector<Value> made;
	machine.spend(items.count() * 16);
	made.reserve(items.count());
	for(std::size_t at = 0; at < items.count(); ++at) made.push_back(items.at(at));
	std::ranges::stable_sort(made, comesFirst);
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

/// @brief `%sort-by-keys`: what `sort-by` ends with.
/// @param machine The machine.
/// @param arguments A vector of keys and a vector of items as long.
/// @return The items, in the order of their keys.
Value sortedByKeys(Machine& machine, Arguments arguments) {
	std::vector<std::pair<Value, Value>> pairs;
	machine.spend(arguments[0].count() * 16);
	pairs.reserve(arguments[0].count());
	for(std::size_t at = 0; at < arguments[0].count(); ++at)
		pairs.emplace_back(arguments[0].at(at), arguments[1].at(at));
	std::ranges::stable_sort(pairs, comesFirst, &std::pair<Value, Value>::first);
	std::vector<Value> made;
	made.reserve(pairs.size());
	for(const auto& [key, item] : pairs) made.push_back(item);
	return machine.heap.sequence(Kind::Vector, std::move(made));
}

} // namespace

void defineCollections(Machine& machine) {
	machine.defineNative("%seq", toSequence);
	machine.defineNative("%nth", patternItem);
	machine.defineNative("%drop", patternRest);
	machine.defineNative("%get", getMember);
	machine.defineNative("%sort-by-keys", sortedByKeys);
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
	machine.defineNative("sort", sorted);
	machine.defineNative("apply", nullptr, true);
}

} // namespace affresco::lisp
