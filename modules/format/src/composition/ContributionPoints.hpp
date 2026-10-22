#pragma once

/// @file
/// @brief Contribution points across apps: the "points" section (where another app's views may go and what they
/// receive), the typed contract of an export (its kind, its items' fields, an action's arguments), what a feature
/// "needs", and an app's contributions as tools print them.
///
/// All of it is untrusted input, checked with bounded work. "points" is a capability: only the host writes it, so the
/// actions its points pass on (grantedActions()) are the host's choice, never a revision's.

#include "affresco/core/Json.hpp"

#include <array>
#include <set>
#include <string>
#include <string_view>

namespace affresco::format {

/// The slots a point can be: where a contributed view sits.
inline constexpr std::array<std::string_view, 4> pointSlots = {"dock", "panel", "menu", "toolbar"};

/// The kinds of an export: a plain value, a state path, a list of records, an action, a data model, a surface (a
/// node tree to draw into) or an image the host renders.
inline constexpr std::array<std::string_view, 7> exportKinds = {"value", "state",   "list", "action",
                                                                "model", "surface", "image"};

/// The kinds of a need: those of an export, a point's slot, or a capability the host must have.
inline constexpr std::array<std::string_view, 9> needKinds = {"value",   "state", "list", "action",    "model",
                                                              "surface", "image", "slot", "capability"};

/// Most needs one feature may declare.
inline constexpr std::size_t maxNeeds = 64;

/// @brief Checks the "points" section: `"<point>": { "slot", "receives": [<export>], "description", "note" }`.
/// @param section The section.
/// @param name "points".
/// @throws DocumentError If a point is malformed.
void checkPointsSection(const json& section, const std::string& name);

/// @brief Checks an export's typed contract: "kind", "item" (`{ "<field>": <type> }`, "image" a type too) and
/// "arguments" (`{ "<name>": <type> }`, for an action).
/// @param entry The export.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkExportContract(const json& entry, const std::string& place);

/// @brief Checks a manifest's or a record's "needs": `"<name>": { "kind", "type", "item", "arguments", "slot",
/// "capability", "description", "optional" }`.
/// @param needs The part.
/// @param place Its place.
/// @throws DocumentError If a need is malformed, or there are more than maxNeeds.
void checkNeeds(const json& needs, const std::string& place);

/// @brief The kind an export declares, "value" when it says none.
/// @param entry The export.
/// @return Its kind.
[[nodiscard]] std::string exportKind(const json& entry);

/// @brief The action an action export names, when its value is a literal: `"value": "{{'moveLayer'}}"`.
/// @param entry The export.
/// @return The action's name, or empty if its value computes it.
[[nodiscard]] std::string actionTarget(const json& entry);

/// @brief The actions a document's points pass on to contributed views: the literal targets of the action exports
/// they receive (`"value": "{{'moveLayer'}}"`).
/// @param document The document (raw, with its "points" and "exports").
/// @return Their names.
[[nodiscard]] std::set<std::string, std::less<>> grantedActions(const json& document);

/// @brief An installable part as a catalogue lists it: name, release, title, needs and the points it fills.
/// @param manifest A checked manifest.
/// @return `{ "name", "release", "title", "needs": {...}, "views": [<point or need>] }`.
/// @throws DocumentError If it isn't an object, its name, release or title isn't text, or its needs don't check (a
/// manifest nobody checked).
[[nodiscard]] json partOf(const json& manifest);

/// @brief A document's contributions: its points with what each receives, and its typed exports.
/// @param document The document (raw).
/// @param parts Its installable parts (partOf() each), or an empty list.
/// @return `{ "points": {...}, "exports": {...}, "parts": [...] }`.
[[nodiscard]] json contributionsOf(const json& document, const json& parts);

/// @brief Prints contributions compactly, one line per point, export and part.
/// @param contributions What contributionsOf() returned.
/// @return The text.
[[nodiscard]] std::string contributionsText(const json& contributions);

} // namespace affresco::format
