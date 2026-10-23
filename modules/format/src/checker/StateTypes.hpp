#pragma once

/// @file
/// @brief Typed state: the "stateTypes" section, a JSON Schema per state key, and what reads it (the rules, the
/// checker, the outline). The schema is a small subset of JSON Schema, general enough for any value a screen keeps
/// (a settings page can describe its settings with it too).

#include "affresco/core/Json.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief The kinds of JSON value a type allows, or a value may hold, as bits.
enum class Kind : std::uint8_t {
	Null = 1,     ///< null.
	Boolean = 2,  ///< true or false.
	Integer = 4,  ///< A whole number.
	Fraction = 8, ///< A number that isn't whole.
	Text = 16,    ///< A string.
	List = 32,    ///< An array.
	Object = 64,  ///< An object.
};

/// @brief A set of kinds.
using Kinds = std::uint8_t;

/// @brief One kind as a set.
/// @param kind The kind.
/// @return The set with it alone.
[[nodiscard]] constexpr Kinds bit(Kind kind) noexcept { return static_cast<Kinds>(kind); }

inline constexpr Kinds anyKind = 127; ///< Every kind: unknown.
inline constexpr Kinds numberKinds = static_cast<Kinds>(bit(Kind::Integer) | bit(Kind::Fraction)); ///< Any number.

/// @brief The kind of a value.
/// @param value The value.
/// @return One kind.
[[nodiscard]] Kinds kindOf(const json& value);

/// @brief The kinds a type allows: from "type", else from "enum" or "const", else every kind.
/// @param type The type.
/// @return The kinds.
[[nodiscard]] Kinds allowedKinds(const json& type);

/// @brief Names kinds for a message: "a number", "text or null".
/// @param kinds The kinds.
/// @return The words.
[[nodiscard]] std::string describeKinds(Kinds kinds);

/// @brief Checks that a type is one this library reads: the keywords below, each well formed.
///
/// "type" (a name or a list of names: "string", "number", "integer", "boolean", "object", "array", "null"), "enum",
/// "const", "properties", "required", "additionalProperties" (true, false or a type), "items", "minimum", "maximum",
/// "minLength", "maxLength", "minItems", "maxItems", and the notes "description", "title", "default", "examples" and
/// "note". Types nest at most maxTypeDepth deep.
/// @param type The type.
/// @param place Its place, for errors.
/// @throws DocumentError If it isn't one.
void checkType(const json& type, const std::string& place);

/// @brief Checks the "stateTypes" section: an object of types, one per state key.
/// @param section The section.
/// @param name "stateTypes".
/// @throws DocumentError If a type is malformed.
void checkStateTypes(const json& section, const std::string& name);

/// @brief A value that doesn't match its type.
struct Mismatch {
	std::string place;   ///< Where, e.g. "state.cards[2].title".
	std::string problem; ///< What, e.g. "expected text, got 3".

	/// @brief The mismatch as one line: "<place>: <problem>".
	[[nodiscard]] std::string text() const { return place + ": " + problem; }
};

/// @brief The first way a value doesn't match a type.
/// @param value The value.
/// @param type The type (checked with checkType()).
/// @param place The value's place, e.g. "state.count".
/// @return The first mismatch found, or std::nullopt if it matches.
[[nodiscard]] std::optional<Mismatch> typeMismatch(const json& value, const json& type, const std::string& place);

/// @brief Every state key whose value doesn't match its declared type.
/// @param document The document; its "stateTypes" are read.
/// @param state The state checked against them.
/// @return One mismatch per key that has one, in the order the types are declared.
[[nodiscard]] std::vector<Mismatch> stateMismatches(const json& document, const json& state);

/// @brief The type of a value inside a typed one: a property of an object, an item of a list.
/// @param type The type.
/// @param key A property's name, or nullptr for an item (or a key computed while the screen runs).
/// @return Its type, or nullptr if the type says nothing about it (then anything may be there).
[[nodiscard]] const json* typeInside(const json& type, const std::string* key);

/// @brief Describes a type in a few words, for messages and outlines: "number", "'all' | 'bug'", "list of {title:
/// text, done: boolean}".
/// @param type The type.
/// @param depth Types inside types so far (stops at 3: "...").
/// @return The description.
[[nodiscard]] std::string describeType(const json& type, int depth = 0);

/// How deep types may nest inside each other.
inline constexpr int maxTypeDepth = 32;

} // namespace affresco::format
