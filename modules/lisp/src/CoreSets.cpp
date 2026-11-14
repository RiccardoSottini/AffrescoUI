/// @file
/// @brief Sets (Sets.hpp): what makes them and finds their members, the core functions on them (`hash-set`, `set?`,
/// `disj`, `%set`, which a literal with a form in it calls), and the `clojure.set` namespace's, written `set/union`
/// with its alias (Machine.hpp, defineSets()): native functions, and library entries written in the language
/// (Library.hpp) for those that call a function back.

#include "Library.hpp"
#include "Machine.hpp"
#include "Sequences.hpp"
#include "Sets.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

Value setOfItems(Heap& heap, Machine* machine, std::span<const Value> items, Place place,
                 std::optional<std::size_t>* repeat) {
	// Built as the set grows, so that a repeat is found against the members before it at a hash's cost
	Value set = heap.set({}, {}, place);
	for(std::size_t at = 0; at < items.size(); ++at) {
		const std::size_t hash = valueHash(machine, items[at]);
		if(!setPosition(machine, set, items[at], hash)) {
			set = heap.setAppended(set, items[at], hash);
		} else if(repeat) {
			*repeat = at;
			return {};
		}
	}
	return set;
}

std::optional<Value> setMember(Machine& machine, const Value& set, const Value& item) {
	const std::optional<std::uint32_t> position = setPosition(&machine, set, item, valueHash(&machine, item));
	if(!position) return std::nullopt;
	return set.at(*position);
}

Value setWith(Machine& machine, const Value& set, const Value& item) {
	const std::size_t hash = valueHash(&machine, item);
	return setPosition(&machine, set, item, hash) ? set : machine.heap.setAppended(set, item, hash);
}

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// @brief Anything with items as a set: a set as it is; nil, a vector, a list, a map's pairs, a string's characters
/// each once, in order. A set that comes back from an app's state is a vector, so `clojure.set` takes vectors too.
/// @param machine The machine.
/// @param value The value.
/// @param function The function, for an error.
/// @return The set.
Value asSet(Machine& machine, const Value& value, std::string_view function) {
	if(value.kind() == Kind::Set) return value;
	const Value items = sequenceOf(machine, value, function);
	std::vector<Value> copied;
	copied.reserve(items.count());
	for(std::size_t at = 0; at < items.count(); ++at) copied.push_back(items.at(at));
	return setOfItems(machine.heap, &machine, copied);
}

/// @brief `hash-set` and `%set` (what a literal with a form in it calls). @param machine The machine. @param arguments
/// The items. @return The set of them, each once.
Value hashSet(Machine& machine, Arguments arguments) { return setOfItems(machine.heap, &machine, arguments); }

/// @brief `%set-of`, what a template's set literal ends with (`#{~x ~@xs}`, CompilerForms.cpp): the set of a
/// vector's items, each once. @param machine The machine. @param arguments The vector. @return The set.
Value setOfVector(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "%set-of");
	return asSet(machine, arguments[0], "%set-of");
}

/// @brief `set?`. @param machine The machine. @param arguments A value. @return Whether it is a set.
Value isSet(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 1, 1, "set?");
	return Value::boolean(arguments[0].kind() == Kind::Set);
}

/// @brief `disj`: the set without the items, its other members in their order.
/// @param machine The machine.
/// @param arguments A set or nil, then items.
/// @return The set.
Value disjoin(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, static_cast<std::size_t>(-1), "disj");
	const Value& from = arguments[0];
	if(from.isNil()) return from;
	if(from.kind() != Kind::Set)
		throw Problem("\"disj\" takes a set, and was given " + std::string(describeKind(from)));
	std::vector<bool> removed(from.count(), false);
	bool any = false;
	for(const Value& item : arguments.subspan(1)) {
		const std::optional<std::uint32_t> position = setPosition(&machine, from, item, valueHash(&machine, item));
		if(position) removed[*position] = any = true;
	}
	if(!any) return from;
	const SetStorage& storage = *setObjectOf(from).storage;
	std::vector<Value> members;
	std::vector<std::size_t> hashes;
	machine.spend(from.count());
	machine.heap.charge(from.count() * (sizeof(Value) + sizeof(std::size_t)));
	for(std::size_t at = 0; at < from.count(); ++at) {
		if(removed[at]) continue;
		members.push_back(from.at(at));
		hashes.push_back(storage.hashes[at]);
	}
	return machine.heap.set(std::move(members), hashes, from.place());
}

/// @brief `union`: the first's members, then each other's that aren't members yet, in order.
/// @param machine The machine.
/// @param arguments Sets (or anything with items).
/// @return The set.
Value setUnion(Machine& machine, Arguments arguments) {
	if(arguments.empty()) return machine.heap.set({}, {});
	Value result = asSet(machine, arguments[0], "union");
	for(const Value& other : arguments.subspan(1)) {
		const Value items = other.kind() == Kind::Set ? other : sequenceOf(machine, other, "union");
		for(std::size_t at = 0; at < items.count(); ++at) result = setWith(machine, result, items.at(at));
	}
	return result;
}

/// @brief What `intersection` and `difference` share: the first's members kept by whether every other has them, or
/// none does.
/// @param machine The machine.
/// @param arguments Sets (or anything with items), at least one.
/// @param inAll Keep those every other has (intersection), else those none has (difference).
/// @param function The function, for an error.
/// @return The set, in the first's order.
Value keptMembers(Machine& machine, Arguments arguments, bool inAll, std::string_view function) {
	needCount(arguments, 1, static_cast<std::size_t>(-1), function);
	const Value first = asSet(machine, arguments[0], function);
	std::vector<Value> others;
	others.reserve(arguments.size() - 1);
	for(const Value& other : arguments.subspan(1)) others.push_back(asSet(machine, other, function));
	std::vector<Value> members;
	std::vector<std::size_t> hashes;
	const SetStorage& storage = *setObjectOf(first).storage;
	for(std::size_t at = 0; at < first.count(); ++at) {
		const Value item = first.at(at);
		bool kept = true;
		for(const Value& other : others) {
			const bool has = setPosition(&machine, other, item, storage.hashes[at]).has_value();
			if(has != inAll) {
				kept = false;
				break;
			}
		}
		if(!kept) continue;
		machine.heap.charge(sizeof(Value) + sizeof(std::size_t));
		members.push_back(item);
		hashes.push_back(storage.hashes[at]);
	}
	return machine.heap.set(std::move(members), hashes);
}

/// @brief `intersection`. @param machine The machine. @param arguments Sets. @return The first's members that every
/// other has.
Value intersection(Machine& machine, Arguments arguments) {
	return keptMembers(machine, arguments, true, "intersection");
}

/// @brief `difference`. @param machine The machine. @param arguments Sets. @return The first's members that no other
/// has.
Value difference(Machine& machine, Arguments arguments) { return keptMembers(machine, arguments, false, "difference"); }

/// @brief Whether every member of one is a member of the other.
/// @param machine The machine.
/// @param part The one.
/// @param whole The other.
/// @param function The function, for an error.
/// @return true if so.
bool containedIn(Machine& machine, const Value& part, const Value& whole, std::string_view function) {
	const Value small = asSet(machine, part, function);
	const Value large = asSet(machine, whole, function);
	if(small.count() > large.count()) return false;
	const SetStorage& storage = *setObjectOf(small).storage;
	for(std::size_t at = 0; at < small.count(); ++at)
		if(!setPosition(&machine, large, small.at(at), storage.hashes[at])) return false;
	return true;
}

/// @brief `subset?`. @param machine The machine. @param arguments Two sets. @return Whether the first's members are
/// all the second's.
Value isSubset(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "subset?");
	return Value::boolean(containedIn(machine, arguments[0], arguments[1], "subset?"));
}

/// @brief `superset?`. @param machine The machine. @param arguments Two sets. @return Whether the second's members are
/// all the first's.
Value isSuperset(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "superset?");
	return Value::boolean(containedIn(machine, arguments[1], arguments[0], "superset?"));
}

/// @brief `rename-keys`, as Clojure's: the map without the old keys, then each new key set to the old one's value; a
/// new key the map has keeps its place.
/// @param machine The machine.
/// @param arguments A map (or nil), and a map from old keys to new ones.
/// @return The map.
Value renameKeys(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "rename-keys");
	const Value& from = arguments[0];
	const Value& names = arguments[1];
	if((!from.isNil() && from.kind() != Kind::Map) || (!names.isNil() && names.kind() != Kind::Map))
		throw Problem("\"rename-keys\" takes two maps, and was given " + std::string(describeKind(from)) + " and " +
		              std::string(describeKind(names)));
	std::vector<MapEntry> members;
	machine.spend(from.count());
	machine.heap.charge((from.count() + names.count()) * sizeof(MapEntry));
	for(std::size_t at = 0; at < from.count(); ++at) {
		bool renamed = false;
		machine.spendLookup(names, from.keyAt(at));
		static_cast<void>(names.lookup(from.keyAt(at), &renamed));
		if(!renamed) members.push_back({from.keyAt(at), from.valueAt(at)});
	}
	for(std::size_t name = 0; name < names.count(); ++name) {
		bool present = false;
		machine.spendLookup(from, names.keyAt(name));
		const Value value = from.lookup(names.keyAt(name), &present);
		if(!present) continue;
		const std::string_view key = textKey(names.valueAt(name), "(set/rename-keys m {:old :new})");
		machine.spend(members.size());
		const auto found = std::ranges::find(members, key, &MapEntry::key);
		if(found != members.end()) found->value = value;
		else members.push_back({machine.heap.keep(key), value});
	}
	return machine.heap.map(std::move(members));
}

/// @brief `map-invert`: a map from each value to its key; a value twice keeps the last key, as Clojure's does.
/// @param machine The machine.
/// @param arguments A map, or nil.
/// @return The map.
Value mapInvert(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "map-invert");
	const Value& from = arguments[0];
	if(!from.isNil() && from.kind() != Kind::Map)
		throw Problem("\"map-invert\" takes a map, and was given " + std::string(describeKind(from)));
	std::vector<Value> pairs;
	pairs.reserve(from.count() + 1);
	pairs.push_back(machine.heap.map({}));
	for(std::size_t at = 0; at < from.count(); ++at) {
		static_cast<void>(textKey(from.valueAt(at), "(set/map-invert (update-vals m str))"));
		const std::vector<Value> pair = {from.valueAt(at), machine.heap.string(from.keyAt(at))};
		pairs.push_back(machine.heap.sequence(Kind::Vector, std::span<const Value>(pair)));
	}
	return conjMap(machine, pairs);
}

/// The namespace's functions that take a function, written in the language so that it runs on the machine's stack.
constexpr auto library = std::to_array<LibraryEntry>({
    {.name = "clojure.set/select",
	 .source = R"lisp((defn clojure.set/select [pred xset] (into #{} (filter pred xset))))lisp",
	 .takesFunctions = true},
});

} // namespace

void defineSets(Machine& machine) {
	machine.defineNative("%set", hashSet);
	machine.defineNative("%set-of", setOfVector);
	machine.defineNative("hash-set", hashSet);
	machine.defineNative("set?", isSet);
	machine.defineNative("disj", disjoin);
	machine.defineNative("clojure.set/union", setUnion);
	machine.defineNative("clojure.set/intersection", intersection);
	machine.defineNative("clojure.set/difference", difference);
	machine.defineNative("clojure.set/subset?", isSubset);
	machine.defineNative("clojure.set/superset?", isSuperset);
	machine.defineNative("clojure.set/rename-keys", renameKeys);
	machine.defineNative("clojure.set/map-invert", mapInvert);
	defineLibrary(machine, library);
}

} // namespace affresco::lisp
