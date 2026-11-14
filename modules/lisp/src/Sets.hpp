#pragma once

/// @file
/// @brief Sets, the language's value that JSON hasn't (Kind::Set): distinct items in the order they were added, with
/// a hash index beside them. What makes them and finds their members, for the reader (Reader.cpp), the functions on
/// collections (CoreCollections.cpp, CoreSequences.cpp), the machine (a set called as a function) and clojure.set
/// (CoreSets.cpp). The walks that hash and compare are Tree.cpp's.

#include "Objects.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace affresco::lisp {

class Machine;

/// @brief A hash of a value that agrees with `=` (valueHash(Machine&, const Value&)), with no budget when there is no
/// machine: the reader's, whose text bounds it.
/// @param machine The machine, whose budget it spends, or nullptr.
/// @param value The value.
/// @return The hash.
/// @throws Problem If the value is nested deeper than maxValueDepth, or the budget runs out.
[[nodiscard]] std::size_t valueHash(Machine* machine, const Value& value);

/// @brief The position of a set's member equal (`=`) to an item (Tree.cpp).
/// @param machine The machine, whose budget it spends, or nullptr.
/// @param set The set.
/// @param item The item.
/// @param hash The item's hash (valueHash()).
/// @return The position, or std::nullopt if it isn't a member.
[[nodiscard]] std::optional<std::uint32_t> setPosition(Machine* machine, const Value& set, const Value& item,
                                                       std::size_t hash);

/// @brief A set of items, each once, where it first comes.
/// @param heap Where it goes.
/// @param machine The machine, whose budget it spends, or nullptr (the reader).
/// @param items The items.
/// @param place Where it was read.
/// @param repeat If given, an item equal to one before it isn't dropped: its index goes here and nil is given back,
/// for the reader's error, as a map's key twice is one.
/// @return The set, or nil when a repeat was found and @p repeat was given.
[[nodiscard]] Value setOfItems(Heap& heap, Machine* machine, std::span<const Value> items, Place place = {},
                               std::optional<std::size_t>* repeat = nullptr);

/// @brief A set's member equal to an item: `(s x)`, `get`, `contains?`.
/// @param machine The machine.
/// @param set The set.
/// @param item The item.
/// @return The member (as it was added: `(#{:a} "a")` gives `:a`), or std::nullopt.
[[nodiscard]] std::optional<Value> setMember(Machine& machine, const Value& set, const Value& item);

/// @brief A set with an item added, unless it is a member: `conj`, `into`.
/// @param machine The machine.
/// @param set The set.
/// @param item The item.
/// @return The set.
[[nodiscard]] Value setWith(Machine& machine, const Value& set, const Value& item);

} // namespace affresco::lisp
