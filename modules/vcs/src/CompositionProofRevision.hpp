#pragma once

/// @file
/// @brief The revision that installs a feature from another app: one `set` of `/features/<name>` to its record, the
/// record carrying its bindings and the proof (CompositionProof.hpp in the format module). The `features` service
/// submits it, so the document's permissions decide whether it is reviewed; history and blame show it, and undo
/// reverts it like any revision.

#include "affresco/core/Json.hpp"

#include <string>
#include <string_view>

namespace affresco::vcs {

/// @brief The install revision of a proven composition.
/// @param name The feature.
/// @param record Its record, with "bindings" and a passed "proof".
/// @param author Who submits it: "features".
/// @param host The host document: a host with no "features" section gets one with the record.
/// @return `{ "author", "message", "set": { "/features/<name>": record } }`, or `"/features": { name: record }`.
/// @throws DocumentError If the record has no passed proof of that feature.
[[nodiscard]] json compositionRevision(const std::string& name, const json& record, std::string_view author,
                                       const json& host);

/// @brief One line on what a commit's proof says, for history and review: "proof: slideSorter 1.0.0 from
/// app:sinopia, passed, 5 needs bound, 3 scenarios".
/// @param record A feature record.
/// @return The line, or "" if it has no proof.
[[nodiscard]] std::string proofLine(const json& record);

} // namespace affresco::vcs
