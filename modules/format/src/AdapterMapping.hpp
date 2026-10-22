#pragma once

/// @file
/// @brief Adapters: how a feature from another app gets what it needs from its host. Each need binds to a host
/// export, an expression over the host's exports, a host action with mapped arguments, a point or a capability the
/// host has; an adapter is a declarative JSON file of such bindings, expressions only (no code, no shader, no
/// capability), bounded in size and depth.
///
/// Adapters and bindings are untrusted input: every check here bounds its work.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

inline constexpr std::string_view adapterFormat = "affrescoui-adapter"; ///< An adapter's "format".
inline constexpr std::size_t maxAdapterBytes = 64u << 10;               ///< An adapter, as JSON: 64 KiB.
inline constexpr int maxAdapterDepth = 8;                               ///< An adapter's nesting.
inline constexpr std::size_t maxBindings = 64;                          ///< Bindings in one adapter or record.
inline constexpr std::size_t maxBindingExpression = 2048;               ///< Characters in one binding's text.

/// @brief Checks an adapter on its own: `{ "format": "affrescoui-adapter", "version": 1, "feature", "host", "note",
/// "bind": { "<need>": <binding> } }`, within the limits.
/// @param adapter The adapter, as parsed with parseUntrusted().
/// @throws DocumentError If it is malformed or too big.
void checkAdapter(const json& adapter);

/// @brief Checks bindings: `{ "<need>": { "export" } | { "value": "{{...}}" } | { "action", "with" } | { "point" } |
/// { "capability" } }`, each expression parsed.
/// @param bindings The bindings.
/// @param place Their place.
/// @throws DocumentError If one is malformed or too big.
void checkBindings(const json& bindings, const std::string& place);

/// @brief A feature's needs bound to a host.
struct BoundNeeds {
	json bindings = json::object(); ///< Need to binding, the direct ones written out: what the record keeps.
	json records = json::array();   ///< `[{ "need", "kind", "by": "host" | "adapter", "to" }]`, for the proof.
	json unmet = json::array();     ///< `[{ "need", "why" }]`.
};

/// @brief Binds a feature's needs to a host: through the adapter when it binds the need, otherwise directly (a host
/// export of the need's name and kind, the one point of its slot, or the capability the host declares). An action
/// is bound only to an action export one of the host's points receives.
/// @param needs The feature's needs (checked).
/// @param host The host document (raw).
/// @param adapter The adapter (checked), or null.
/// @return What bound and what didn't, with why.
[[nodiscard]] BoundNeeds bindNeeds(const json& needs, const json& host, const json& adapter);

/// @brief A feature's "provides" with its bindings applied, for Composition to merge.
struct AdaptedFeature {
	json provides;                                                 ///< Views moved to their points, actions added.
	std::vector<std::pair<std::string, std::string>> replacements; ///< `app.<need>` to what it stands for.
	std::vector<std::string> problems;                             ///< Each with its place.
};

/// @brief Applies a record's bindings: a slot need's views go to the bound point, an action need becomes the action
/// `<feature>.adapted.<need>` running the host's, and a data need stands for its expression.
/// @param name The feature.
/// @param record Its record, with "needs" and "bindings".
/// @param exports The host's exports.
/// @return The adapted parts and the problems (an unbound need, a feature action named "adapted.*").
[[nodiscard]] AdaptedFeature adaptFeature(const std::string& name, const json& record, const json& exports);

} // namespace affresco::format
