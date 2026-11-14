/// @file
/// @brief The language's sequence functions, native (Machine.hpp, defineSequences()): `seq`, `next`, `peek`, `pop`,
/// `subvec`, `partition`, `interleave`, `distinct`, `flatten`, `into` and the like. None calls a function back (those
/// are written in Lisp, PreludeFunctions.cpp). Each spends one operation per item it walks, and the heap charges what
/// it makes. Nothing is lazy: a result is a vector, or a part of the list or the vector it was given, sharing its
/// items where the heap can (Heap::part()).

#include "Machine.hpp"
#include "Sequences.hpp"
#include "Sets.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

using Arguments = std::span<const Value>; ///< A function's arguments.

/// The most arguments a function that takes any number of them is counted against.
constexpr std::size_t any = static_cast<std::size_t>(-1);

/// @brief A count or an index a function is given.
/// @param value The argument.
/// @param function The function, for the error.
/// @return Its whole value.
/// @throws Problem If it isn't a whole number.
std::int64_t wholeArgument(const Value& value, std::string_view function) {
	const std::optional<std::int64_t> whole = wholeNumber(value);
	if(!whole)
		throw Problem("\"" + std::string(function) + "\" takes a whole number, and was given " +
		              std::string(describeKind(value)));
	return *whole;
}

/// @brief A count a function takes, at least 1: a size or a step.
/// @param value The argument.
/// @param function The function, for the error.
/// @return It.
/// @throws Problem If it is below 1, which would never end.
std::size_t stepArgument(const Value& value, std::string_view function) {
	const std::int64_t step = wholeArgument(value, function);
	if(step < 1) throw Problem("\"" + std::string(function) + "\" takes a size and a step of at least 1");
	return static_cast<std::size_t>(step);
}

/// @brief A count clamped into a collection's size: below 0 is 0, past the end the end.
/// @param value The count.
/// @param size The collection's size.
/// @param function The function, for the error.
/// @return The count.
std::size_t clampedCount(const Value& value, std::size_t size, std::string_view function) {
	const std::int64_t count = wholeArgument(value, function);
	return count <= 0 ? 0 : std::min(static_cast<std::size_t>(count), size);
}

/// @brief A part of a list or a vector, or nil when it would be empty, as `next` gives.
/// @param machine The machine.
/// @param items The list or the vector.
/// @param offset Its first item.
/// @param size How many.
/// @return The part, or nil.
Value partOrNil(Machine& machine, const Value& items, std::size_t offset, std::size_t size) {
	return size == 0 ? Value() : machine.heap.part(items, offset, size);
}

/// @brief A vector of items. @param machine The machine. @param items The items. @return The vector.
Value vectorOf(Machine& machine, std::vector<Value>&& items) {
	return machine.heap.sequence(Kind::Vector, std::move(items));
}

/// @brief `seq`. @param machine The machine. @param arguments A collection, a string or nil. @return Its items: nil
/// when it has none; a list or a vector as it is; a map's [key value] pairs; a string's characters.
Value toSeq(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "seq");
	const Value items = sequenceOf(machine, arguments[0], "seq");
	return items.count() == 0 ? Value() : items;
}

/// @brief What `next` and `nnext` share.
/// @param machine The machine.
/// @param value The collection.
/// @param skip How many items to leave out.
/// @param function The function, for an error.
/// @return The items after them, or nil.
Value after(Machine& machine, const Value& value, std::size_t skip, std::string_view function) {
	const Value items = sequenceOf(machine, value, function);
	return items.count() <= skip ? Value() : machine.heap.part(items, skip, items.count() - skip);
}

/// @brief A collection's first item. @param machine The machine. @param value The collection. @param function The
/// function, for an error. @return The item, or nil.
Value firstOf(Machine& machine, const Value& value, std::string_view function) {
	const Value items = sequenceOf(machine, value, function);
	return items.count() == 0 ? Value() : items.at(0);
}

/// @brief `next`. @param machine The machine. @param arguments A collection. @return All but its first item, or nil.
Value next(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "next");
	return after(machine, arguments[0], 1, "next");
}

/// @brief `nnext`. @param machine The machine. @param arguments A collection. @return All but its first two, or nil.
Value nextOfNext(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "nnext");
	return after(machine, arguments[0], 2, "nnext");
}

/// @brief `ffirst`. @param machine The machine. @param arguments A collection. @return The first of its first.
Value firstOfFirst(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "ffirst");
	return firstOf(machine, firstOf(machine, arguments[0], "ffirst"), "ffirst");
}

/// @brief `fnext`. @param machine The machine. @param arguments A collection. @return Its second item, or nil.
Value firstOfNext(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "fnext");
	const Value items = sequenceOf(machine, arguments[0], "fnext");
	return items.count() < 2 ? Value() : items.at(1);
}

/// @brief `nfirst`. @param machine The machine. @param arguments A collection. @return The next of its first.
Value nextOfFirst(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "nfirst");
	return after(machine, firstOf(machine, arguments[0], "nfirst"), 1, "nfirst");
}

/// @brief `nthrest`. @param machine The machine. @param arguments A collection and a count. @return Its items after
/// that many: empty past its end.
Value nthRest(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "nthrest");
	const Value items = sequenceOf(machine, arguments[0], "nthrest");
	const std::size_t skip = clampedCount(arguments[1], items.count(), "nthrest");
	return machine.heap.part(items, skip, items.count() - skip);
}

/// @brief `nthnext`. @param machine The machine. @param arguments A collection and a count. @return Its items after
/// that many, or nil.
Value nthNext(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "nthnext");
	const Value items = sequenceOf(machine, arguments[0], "nthnext");
	const std::size_t skip = clampedCount(arguments[1], items.count(), "nthnext");
	return partOrNil(machine, items, skip, items.count() - skip);
}

/// @brief Checks `peek`'s and `pop`'s argument. @param value It. @param function The function, for the error.
void needStack(const Value& value, std::string_view function) {
	if(!value.isSequence() && !value.isNil())
		throw Problem("\"" + std::string(function) + "\" takes a list or a vector, and was given " +
		              std::string(describeKind(value)));
}

/// @brief `peek`. @param machine The machine. @param arguments A list, a vector or nil. @return A vector's last item,
/// a list's first, or nil.
Value peek(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 1, 1, "peek");
	const Value& items = arguments[0];
	needStack(items, "peek");
	if(items.count() == 0) return {};
	return items.kind() == Kind::Vector ? items.at(items.count() - 1) : items.at(0);
}

/// @brief `pop`. @param machine The machine. @param arguments A list, a vector or nil. @return A vector without its
/// last item, a list without its first; nil for nil.
Value pop(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "pop");
	const Value& items = arguments[0];
	needStack(items, "pop");
	if(items.isNil()) return {};
	if(items.count() == 0) throw Problem("\"pop\" was given an empty " + std::string(describeKind(items)).substr(2));
	return items.kind() == Kind::Vector ? machine.heap.part(items, 0, items.count() - 1)
	                                    : machine.heap.part(items, 1, items.count() - 1);
}

/// @brief `subvec`. @param machine The machine. @param arguments A vector, a start and maybe an end. @return The
/// vector's items from the start up to the end, or its end.
Value subvector(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 3, "subvec");
	const Value& items = arguments[0];
	if(items.kind() != Kind::Vector)
		throw Problem("\"subvec\" takes a vector, and was given " + std::string(describeKind(items)));
	const std::int64_t start = wholeArgument(arguments[1], "subvec");
	const std::int64_t end =
	    arguments.size() == 3 ? wholeArgument(arguments[2], "subvec") : static_cast<std::int64_t>(items.count());
	if(start < 0 || end < start || static_cast<std::size_t>(end) > items.count())
		throw Problem("\"subvec\": an index outside the vector");
	return machine.heap.part(items, static_cast<std::size_t>(start), static_cast<std::size_t>(end - start));
}

/// @brief `butlast`. @param machine The machine. @param arguments A collection. @return All but its last item, or nil.
Value butLast(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "butlast");
	const Value items = sequenceOf(machine, arguments[0], "butlast");
	return items.count() <= 1 ? Value() : machine.heap.part(items, 0, items.count() - 1);
}

/// @brief `take-last`. @param machine The machine. @param arguments A count and a collection. @return Its last items,
/// that many, or nil.
Value takeLast(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "take-last");
	const Value items = sequenceOf(machine, arguments[1], "take-last");
	const std::size_t size = clampedCount(arguments[0], items.count(), "take-last");
	return partOrNil(machine, items, items.count() - size, size);
}

/// @brief `drop-last`. @param machine The machine. @param arguments Maybe a count (1), and a collection. @return All
/// but its last items, that many.
Value dropLast(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 2, "drop-last");
	const Value items = sequenceOf(machine, arguments.back(), "drop-last");
	const std::size_t size = arguments.size() == 2 ? clampedCount(arguments[0], items.count(), "drop-last")
	                                               : std::min<std::size_t>(1, items.count());
	return machine.heap.part(items, 0, items.count() - size);
}

/// @brief `take-nth`. @param machine The machine. @param arguments A step and a collection. @return Its first item,
/// and every step-th after it.
Value takeNth(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "take-nth");
	const std::size_t step = stepArgument(arguments[0], "take-nth");
	const Value items = sequenceOf(machine, arguments[1], "take-nth");
	std::vector<Value> made;
	machine.spend(items.count() / step + 1);
	made.reserve(items.count() / step + 1);
	for(std::size_t at = 0; at < items.count(); at += step) made.push_back(items.at(at));
	return vectorOf(machine, std::move(made));
}

/// @brief `split-at`. @param machine The machine. @param arguments A count and a collection. @return A vector of its
/// first items, that many, and the rest.
Value splitAt(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "split-at");
	const Value items = sequenceOf(machine, arguments[1], "split-at");
	const std::size_t size = clampedCount(arguments[0], items.count(), "split-at");
	const std::vector<Value> halves = {machine.heap.part(items, 0, size),
	                                   machine.heap.part(items, size, items.count() - size)};
	return machine.heap.sequence(Kind::Vector, std::span<const Value>(halves));
}

/// @brief `interpose`. @param machine The machine. @param arguments A separator and a collection. @return Its items,
/// the separator between each two.
Value interpose(Machine& machine, Arguments arguments) {
	needCount(arguments, 2, 2, "interpose");
	const Value items = sequenceOf(machine, arguments[1], "interpose");
	std::vector<Value> made;
	machine.spend(items.count() * 2);
	made.reserve(items.count() * 2);
	for(std::size_t at = 0; at < items.count(); ++at) {
		if(at > 0) made.push_back(arguments[0]);
		made.push_back(items.at(at));
	}
	return vectorOf(machine, std::move(made));
}

/// @brief `interleave`. @param machine The machine. @param arguments Collections. @return The first item of each, then
/// the second of each..., up to the shortest.
Value interleave(Machine& machine, Arguments arguments) {
	std::vector<Value> collections;
	std::size_t shortest = arguments.empty() ? 0 : any;
	collections.reserve(arguments.size());
	for(const Value& argument : arguments) {
		collections.push_back(sequenceOf(machine, argument, "interleave"));
		shortest = std::min(shortest, collections.back().count());
	}
	std::vector<Value> made;
	machine.spend(shortest * collections.size());
	machine.heap.charge(shortest * collections.size() * sizeof(Value));
	made.reserve(shortest * collections.size());
	for(std::size_t at = 0; at < shortest; ++at)
		for(const Value& items : collections) made.push_back(items.at(at));
	return vectorOf(machine, std::move(made));
}

/// @brief What `partition` and `partition-all` share.
/// @param machine The machine.
/// @param arguments A size, maybe a step, maybe what pads the last part (`partition` only), and a collection.
/// @param all Whether the parts at the end shorter than the size are kept (`partition-all`).
/// @param function The function, for an error.
/// @return A vector of the parts.
Value partitioned(Machine& machine, Arguments arguments, bool all, std::string_view function) {
	needCount(arguments, 2, all ? 3 : 4, function);
	const std::size_t size = stepArgument(arguments[0], function);
	const std::size_t step = arguments.size() >= 3 ? stepArgument(arguments[1], function) : size;
	const Value items = sequenceOf(machine, arguments.back(), function);
	const std::optional<Value> pad =
	    arguments.size() == 4 ? std::optional(sequenceOf(machine, arguments[2], function)) : std::nullopt;
	std::vector<Value> parts;
	for(std::size_t start = 0; start < items.count(); start += step) {
		const std::size_t length = std::min(size, items.count() - start);
		machine.spend(length + 1);
		if(length == size || all) {
			parts.push_back(machine.heap.part(items, start, length));
		} else if(pad) { // the last part, completed from the pad as far as it goes
			std::vector<Value> padded;
			for(std::size_t at = start; at < items.count(); ++at) padded.push_back(items.at(at));
			for(std::size_t at = 0; at < pad->count() && padded.size() < size; ++at) padded.push_back(pad->at(at));
			machine.spend(padded.size());
			parts.push_back(machine.heap.sequence(items.kind(), std::move(padded)));
		}
		if(length < size || items.count() - start <= step) break;
	}
	return vectorOf(machine, std::move(parts));
}

/// @brief `partition`. @param machine The machine. @param arguments A size, maybe a step and a pad, a collection.
/// @return Its parts of that size, each a step after the last; a shorter one at the end only with a pad.
Value partition(Machine& machine, Arguments arguments) { return partitioned(machine, arguments, false, "partition"); }

/// @brief `partition-all`. @param machine The machine. @param arguments A size, maybe a step, a collection.
/// @return Its parts of that size, each a step after the last, and shorter ones at the end.
Value partitionAll(Machine& machine, Arguments arguments) {
	return partitioned(machine, arguments, true, "partition-all");
}

/// @brief `distinct`: hashes each item as `=` compares it (valueHash()), and compares those whose hashes meet.
/// @param machine The machine.
/// @param arguments A collection.
/// @return Its items, each once, where it first comes.
Value distinct(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "distinct");
	const Value items = sequenceOf(machine, arguments[0], "distinct");
	std::vector<Value> made;
	std::unordered_multimap<std::size_t, std::size_t> seen; // a hash, and the index in made of an item with it
	machine.heap.charge(items.count() * (sizeof(Value) + 4 * sizeof(std::size_t)));
	seen.reserve(items.count());
	for(std::size_t at = 0; at < items.count(); ++at) {
		const Value item = items.at(at);
		const std::size_t hash = valueHash(machine, item);
		const auto [first, last] = seen.equal_range(hash);
		if(std::any_of(first, last, [&](const auto& entry) { return valuesEqual(machine, made[entry.second], item); }))
			continue;
		seen.emplace(hash, made.size());
		made.push_back(item);
	}
	return vectorOf(machine, std::move(made));
}

/// @brief `dedupe`. @param machine The machine. @param arguments A collection. @return Its items, each run of equal
/// ones once.
Value dedupe(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "dedupe");
	const Value items = sequenceOf(machine, arguments[0], "dedupe");
	std::vector<Value> made;
	machine.spend(items.count());
	made.reserve(items.count());
	for(std::size_t at = 0; at < items.count(); ++at)
		if(made.empty() || !valuesEqual(machine, made.back(), items.at(at))) made.push_back(items.at(at));
	return vectorOf(machine, std::move(made));
}

/// @brief `flatten`, on a stack of its own rather than the native one, so that a deep value ends at a limit.
/// @param machine The machine.
/// @param arguments A value.
/// @return The items of its lists and vectors, however nested, in order; empty for anything else.
Value flatten(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "flatten");
	std::vector<Value> made;
	if(!arguments[0].isSequence()) return vectorOf(machine, std::move(made));
	std::vector<std::pair<Value, std::size_t>> stack = {{arguments[0], 0}}; // a list or a vector, and its next item
	while(!stack.empty()) {
		auto& [items, at] = stack.back();
		if(at == items.count()) {
			stack.pop_back();
			continue;
		}
		const Value item = items.at(at++);
		machine.spend(1);
		if(!item.isSequence()) {
			made.push_back(item);
			machine.heap.charge(sizeof(Value));
		} else if(stack.size() >= static_cast<std::size_t>(maxValueDepth)) {
			Machine::exhausted("values nested too deeply to flatten");
		} else {
			stack.emplace_back(item, 0);
		}
	}
	return vectorOf(machine, std::move(made));
}

/// @brief `into` a vector, a list or nil: a vector gets the items at its end, a list at its front, one at a time.
/// @param machine The machine.
/// @param target The vector, the list or nil.
/// @param items The items.
/// @return The collection with them.
Value intoSequence(Machine& machine, const Value& target, const Value& items) {
	std::vector<Value> made;
	machine.spend(target.count() + items.count());
	made.reserve(target.count() + items.count());
	if(target.kind() == Kind::List) {
		for(std::size_t at = items.count(); at > 0; --at) made.push_back(items.at(at - 1));
		for(std::size_t at = 0; at < target.count(); ++at) made.push_back(target.at(at));
		return machine.heap.sequence(Kind::List, std::move(made));
	}
	for(std::size_t at = 0; at < target.count(); ++at) made.push_back(target.at(at));
	for(std::size_t at = 0; at < items.count(); ++at) made.push_back(items.at(at));
	return vectorOf(machine, std::move(made));
}

/// @brief `into`. A map takes [key value] pairs or maps, as `conj` onto it does (conjMap()); a set the items that
/// aren't members yet, at its end.
/// @param machine The machine.
/// @param arguments A vector, a list, a map, a set or nil, and a collection.
/// @return The first with the second's items.
Value into(Machine& machine, Arguments arguments) {
	if(arguments.size() == 3) throw Problem("\"into\" takes a collection and what to put in it, not a transducer");
	needCount(arguments, 2, 2, "into");
	const Value& target = arguments[0];
	if(!target.isSequence() && !target.isNil() && target.kind() != Kind::Map && target.kind() != Kind::Set)
		throw Problem("\"into\" takes a vector, a list, a map, a set or nil, and was given " +
		              std::string(describeKind(target)));
	const Value items = sequenceOf(machine, arguments[1], "into");
	if(target.kind() == Kind::Set) {
		Value set = target;
		for(std::size_t at = 0; at < items.count(); ++at) set = setWith(machine, set, items.at(at));
		return set;
	}
	if(target.kind() != Kind::Map) return intoSequence(machine, target, items);
	std::vector<Value> both = {target};
	machine.spend(items.count());
	both.reserve(items.count() + 1);
	for(std::size_t at = 0; at < items.count(); ++at) both.push_back(items.at(at));
	return conjMap(machine, both);
}

/// @brief `vector`. @param machine The machine. @param arguments The items. @return The vector of them.
Value makeVector(Machine& machine, Arguments arguments) { return machine.heap.sequence(Kind::Vector, arguments); }

/// @brief `list*`. @param machine The machine. @param arguments Items, then a collection. @return A list of the items
/// and the collection's, or nil when there are none.
Value listStar(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, any, "list*");
	const Value items = sequenceOf(machine, arguments.back(), "list*");
	std::vector<Value> made(arguments.begin(), arguments.end() - 1);
	machine.spend(made.size() + items.count());
	made.reserve(made.size() + items.count());
	for(std::size_t at = 0; at < items.count(); ++at) made.push_back(items.at(at));
	return made.empty() ? Value() : machine.heap.sequence(Kind::List, std::move(made));
}

/// @brief `not-empty`. @param machine The machine. @param arguments A collection, a string or nil. @return It, or nil
/// when it has nothing.
Value notEmpty(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 1, 1, "not-empty");
	const Value& value = arguments[0];
	if(value.kind() == Kind::String) return value.text().empty() ? Value() : value;
	if(!value.isNil() && !value.isSequence() && value.kind() != Kind::Map && value.kind() != Kind::Set)
		throw Problem("\"not-empty\" takes a collection or a string, and was given " +
		              std::string(describeKind(value)));
	return value.count() == 0 ? Value() : value;
}

/// @brief `rseq`. @param machine The machine. @param arguments A vector. @return Its items backwards, or nil.
Value reversedSeq(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "rseq");
	const Value& items = arguments[0];
	if(items.kind() != Kind::Vector)
		throw Problem("\"rseq\" takes a vector, and was given " + std::string(describeKind(items)));
	if(items.count() == 0) return {};
	std::vector<Value> made;
	machine.spend(items.count());
	made.reserve(items.count());
	for(std::size_t at = items.count(); at > 0; --at) made.push_back(items.at(at - 1));
	return vectorOf(machine, std::move(made));
}

/// @brief What `key` and `val` share. @param arguments A [key value] pair. @param index 0 for the key, 1 for the
/// value. @param function The function, for an error. @return It.
Value ofPair(Arguments arguments, std::size_t index, std::string_view function) {
	needCount(arguments, 1, 1, function);
	if(arguments[0].kind() != Kind::Vector || arguments[0].count() != 2)
		throw Problem("\"" + std::string(function) + "\" takes a [key value] pair, and was given " +
		              std::string(describeKind(arguments[0])));
	return arguments[0].at(index);
}

/// @brief `key`. @param arguments A [key value] pair, as `seq` gives a map's. @return Its key.
Value keyOf(Machine& /*machine*/, Arguments arguments) { return ofPair(arguments, 0, "key"); }

/// @brief `val`. @param arguments A [key value] pair. @return Its value.
Value valueOf(Machine& /*machine*/, Arguments arguments) { return ofPair(arguments, 1, "val"); }

/// @brief `doall`. @param arguments Maybe a count, and a collection. @return The collection: nothing is lazy.
Value doAll(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 1, 2, "doall");
	return arguments.back();
}

/// @brief `dorun`. @param arguments Maybe a count, and a collection. @return nil: nothing is lazy.
Value doRun(Machine& /*machine*/, Arguments arguments) {
	needCount(arguments, 1, 2, "dorun");
	return {};
}

/// @brief `%columns`: what `map` over several collections calls its function with.
/// @param machine The machine.
/// @param arguments A list or a vector of collections.
/// @return A vector of vectors: of the collections' first items, of their second..., up to the shortest.
Value columns(Machine& machine, Arguments arguments) {
	needCount(arguments, 1, 1, "map");
	const Value given = sequenceOf(machine, arguments[0], "map");
	std::vector<Value> collections;
	std::size_t shortest = given.count() == 0 ? 0 : any;
	collections.reserve(given.count());
	for(std::size_t at = 0; at < given.count(); ++at) {
		collections.push_back(sequenceOf(machine, given.at(at), "map"));
		shortest = std::min(shortest, collections.back().count());
	}
	std::vector<Value> made;
	machine.spend(shortest * (collections.size() + 1));
	made.reserve(shortest);
	for(std::size_t at = 0; at < shortest; ++at) {
		std::vector<Value> column;
		column.reserve(collections.size());
		for(const Value& items : collections) column.push_back(items.at(at));
		made.push_back(vectorOf(machine, std::move(column)));
	}
	return vectorOf(machine, std::move(made));
}

} // namespace

void defineSequences(Machine& machine) {
	machine.defineNative("seq", toSeq);
	machine.defineNative("next", next);
	machine.defineNative("nnext", nextOfNext);
	machine.defineNative("ffirst", firstOfFirst);
	machine.defineNative("fnext", firstOfNext);
	machine.defineNative("nfirst", nextOfFirst);
	machine.defineNative("nthrest", nthRest);
	machine.defineNative("nthnext", nthNext);
	machine.defineNative("peek", peek);
	machine.defineNative("pop", pop);
	machine.defineNative("subvec", subvector);
	machine.defineNative("butlast", butLast);
	machine.defineNative("take-last", takeLast);
	machine.defineNative("drop-last", dropLast);
	machine.defineNative("take-nth", takeNth);
	machine.defineNative("split-at", splitAt);
	machine.defineNative("interpose", interpose);
	machine.defineNative("interleave", interleave);
	machine.defineNative("partition", partition);
	machine.defineNative("partition-all", partitionAll);
	machine.defineNative("distinct", distinct);
	machine.defineNative("dedupe", dedupe);
	machine.defineNative("flatten", flatten);
	machine.defineNative("into", into);
	machine.defineNative("vector", makeVector);
	machine.defineNative("list*", listStar);
	machine.defineNative("not-empty", notEmpty);
	machine.defineNative("rseq", reversedSeq);
	machine.defineNative("key", keyOf);
	machine.defineNative("val", valueOf);
	machine.defineNative("doall", doAll);
	machine.defineNative("dorun", doRun);
	machine.defineNative("%columns", columns);
}

} // namespace affresco::lisp
