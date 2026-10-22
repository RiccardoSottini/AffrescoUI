#pragma once

/// @file
/// @brief format::KeyReference and the generators built on it: the JSON Schema and the tables of SCREEN_FORMAT.md,
/// from the registries.

#include "affresco/core/Json.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief A key of the format: the schema of its value and its row in SCREEN_FORMAT.md's tables.
///
/// The registries decide which keys exist and in what order; this adds what they don't say. A key's first entry
/// gives its schema; a later entry of the same name only lists it in another table.
struct KeyReference {
	std::string_view name;   ///< The key; a name with "…" stands for several (a table row only).
	std::string_view schema; ///< A "$defs/v2" name (a "$ref" to it), the JSON of its schema, or "" for {}.
	std::string_view about;  ///< Its row in the table; "" joins the row above (or none, without a table); "{types}"
	                         ///< in it lists the built-in node types.
	std::string_view table;  ///< The table that lists it ("" none), the name in the "generated" marker.
};

/// @brief The keys a node can have, its events among them (ReferenceNodes.cpp).
/// @return The keys, in the order of the tables.
[[nodiscard]] std::span<const KeyReference> nodeKeys();

/// @brief The keys the node types add, from the editor's to the timeline's: nodeKeys()'s middle part
/// (ReferenceNodeTypes.cpp).
/// @return The keys, in the order of the tables.
[[nodiscard]] std::span<const KeyReference> nodeTypeKeys();

/// @brief The events, the form's and the carousel's keys, and the keys no table lists: nodeKeys()'s last part
/// (ReferenceNodeEvents.cpp).
/// @return The keys, in the order of the tables.
[[nodiscard]] std::span<const KeyReference> nodeEventKeys();

/// @brief The top-level sections (ReferenceSections.cpp).
/// @return The sections, in the order of the table.
[[nodiscard]] std::span<const KeyReference> sectionKeys();

/// @brief The keys of a step: the steps, then the keys they read (ReferenceSections.cpp).
/// @return The keys.
[[nodiscard]] std::span<const KeyReference> stepKeys();

/// @brief The shared values the other schemas refer to (ReferenceSections.cpp).
/// @return {name, schema} entries.
[[nodiscard]] std::span<const KeyReference> definitions();

/// @brief The keys a node can have, as the registries say: "type", the properties of every type and set, the event
/// handlers and "slot".
/// @param engine The engine.
/// @return The names, in registration order.
[[nodiscard]] std::vector<std::string> nodeKeyNames(const Engine& engine);

/// @brief The keys of a step: the names of the registered steps, then the keys they read.
/// @param engine The engine.
/// @return The names, in registration order.
[[nodiscard]] std::vector<std::string> stepKeyNames(const Engine& engine);

/// @brief The registered top-level sections.
/// @param engine The engine.
/// @return The names, in registration order.
[[nodiscard]] std::vector<std::string> sectionNames(const Engine& engine);

/// @brief The JSON Schema of the format, with the types, sections and steps the engine has registered.
/// @param engine The engine.
/// @return The schema; "$defs/v2" holds the format's version 2.
[[nodiscard]] json schema(const Engine& engine);

/// @brief Writes the generated tables into SCREEN_FORMAT.md: the text between each "<!-- generated: name -->" and the
/// next "<!-- end generated -->" becomes table "name" (KeyReference::table).
/// @param engine The engine: a table lists only the keys it has registered.
/// @param markdown The document.
/// @return The document with its tables generated.
/// @throws std::invalid_argument If a marker has no end, or names a table that doesn't exist.
[[nodiscard]] std::string referenceTables(const Engine& engine, std::string_view markdown);

} // namespace affresco::format
