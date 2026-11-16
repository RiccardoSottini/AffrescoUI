#pragma once

/// @file
/// @brief Upgrading an installed feature without losing what was edited in place (rfcs/0004-features.md, "Upgrades
/// keep local edits"): when a record was edited, the three records an upgrade merges, and what the person reads about
/// a feature.
///
/// The format doesn't merge: the `features` service (in the screen module, which links version control) merges the
/// three records with vcs::mergeDocuments(), and the merged record goes through the same checks as any record.

#include "affresco/core/Json.hpp"
#include "affresco/format/Features.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief The records an upgrade of one feature works with.
struct UpgradeParts {
	json base;              ///< The record the installed release installed, when baseKnown.
	json mine;              ///< The record now, with any edit made in place.
	json theirs;            ///< The record the newest release installs, with mine's "enabled" and "order".
	std::string from;       ///< The installed release.
	std::string to;         ///< The newest release.
	bool baseKnown = false; ///< A catalogue still lists the installed release, with the record's digest.
};

/// The most edits an upgrade that drops them names in its message; a count says how many more.
inline constexpr std::size_t maxDroppedEditsNamed = 20;

/// @brief The digest a record keeps of its parts as installed, "installed": its "requires", "provides" and "needs".
/// vcs::compositionRevision() writes the same digest (a unit test holds them equal).
/// @param record A feature's record.
/// @return "sha256:<hex>".
[[nodiscard]] std::string installedDigest(const json& record);

/// @brief Tells whether a record was edited in place since it was installed.
/// @param record The record now.
/// @param base The record its release installs, when a catalogue still has it, or nullptr: a record installed before
/// records kept "installed" is compared with it, and counts as edited when there is none.
/// @return true if its parts differ from what was installed.
[[nodiscard]] bool edited(const json& record, const json* base);

/// @brief The three records an upgrade to the newest release merges.
/// @param features The catalogues.
/// @param document The document, as version control keeps it.
/// @param name The feature.
/// @return The parts.
/// @throws DocumentError If it isn't installed, is up to date, the newest release comes from another publisher, or
/// its manifest is refused.
[[nodiscard]] UpgradeParts upgradeParts(const Features& features, const json& document, std::string_view name);

/// @brief The places where a record differs from another, as JSON Pointers inside it: what an upgrade that can't
/// merge drops. Where it came from (release, digest, installed) is left out: every upgrade changes it.
/// @param mine The record now.
/// @param theirs The record that replaces it.
/// @return The paths, in document order, each once.
[[nodiscard]] std::vector<std::string> differingPaths(const json& mine, const json& theirs);

/// @brief What the person reads about one feature.
/// @param features The catalogues.
/// @param document The document, as version control keeps it.
/// @param name The feature.
/// @return `{installed, release, newest, edited, enabled}`: release and newest "" when there is none.
[[nodiscard]] json featureStatus(const Features& features, const json& document, std::string_view name);

/// @brief The record of an installed feature.
/// @param document The document.
/// @param name The feature.
/// @return It, or nullptr.
[[nodiscard]] const json* installedRecord(const json& document, std::string_view name);

} // namespace affresco::format
