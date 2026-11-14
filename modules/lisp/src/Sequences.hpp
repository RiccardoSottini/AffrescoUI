#pragma once

/// @file
/// @brief What the language's native functions on collections share across their files (CoreCollections.cpp,
/// CoreSequences.cpp, CoreNumbers.cpp, PreludeFunctions.cpp, Tree.cpp): reading a collection's items, a map's key as
/// text, `conj` onto a map, the order of
/// `compare`, and a hash that agrees with `=`.

#include "Machine.hpp"

#include <cstddef>
#include <span>
#include <string_view>

namespace affresco::lisp {

/// @brief Checks how many arguments a function has.
/// @param arguments Its arguments.
/// @param least The least.
/// @param most The most.
/// @param function The function, for the error.
/// @throws Problem If there are fewer or more.
void needCount(std::span<const Value> arguments, std::size_t least, std::size_t most, std::string_view function);

/// @brief Anything with items as a list or a vector: nil is empty, a map is its [key value] pairs, a string its
/// characters. A list or a vector, a view of JSON too, is given back as it is, not copied (CoreCollections.cpp).
/// @param machine The machine, whose budget it spends.
/// @param value The value.
/// @param function The function, for an error.
/// @return The list or the vector.
/// @throws Problem If the value has no items.
[[nodiscard]] Value sequenceOf(Machine& machine, const Value& value, std::string_view function);

/// @brief A map's key from a value, where a function builds the map (decision D3: a map's key stays text).
/// @param key The value: a keyword or a string.
/// @param fix What to write instead, as the message's end: `(group-by (comp str f) xs)`.
/// @return Its text.
/// @throws Problem `a map's key is a keyword or a string, not a number: give the key as text, <fix>`.
[[nodiscard]] std::string_view textKey(const Value& key, std::string_view fix);

/// @brief `conj` onto a map (CoreCollections.cpp), which `into` a map uses too.
/// @param machine The machine.
/// @param arguments The map, then [key value] pairs, maps or nil.
/// @return The map with their members set.
/// @throws Problem If an item is none of those, or a key isn't text.
[[nodiscard]] Value conjMap(Machine& machine, std::span<const Value> arguments);

/// @brief The order of `compare` (CoreNumbers.cpp): numbers with numbers, text with text (a keyword is the string of
/// its text, decision D2), nil before everything.
/// @param machine The machine, whose budget comparing texts spends.
/// @param first One.
/// @param second The other.
/// @param function The function, for the error.
/// @return -1, 0 or 1.
/// @throws Problem If they can't be compared.
[[nodiscard]] int compareValues(Machine& machine, const Value& first, const Value& second, std::string_view function);

/// @brief A hash of a value that agrees with `=` (Tree.cpp): equal values hash the same, numbers by value, a keyword as
/// the string of its text, a list as the vector of its items, a map whatever its order. Walks as deep as `=` does.
/// @param machine The machine, whose budget it spends: one operation a value.
/// @param value The value.
/// @return The hash.
/// @throws Problem If the value is nested deeper than maxValueDepth, or the budget runs out.
[[nodiscard]] std::size_t valueHash(Machine& machine, const Value& value);

} // namespace affresco::lisp
