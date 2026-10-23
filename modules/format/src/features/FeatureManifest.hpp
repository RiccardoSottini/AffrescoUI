#pragma once

/// @file
/// @brief Feature manifests (rfcs/0004-features.md): releases and their ranges, a manifest's shape, the records of the
/// "features" section, and the app's "exports" and "imports" sections.
///
/// Manifests and records are untrusted input: every check here bounds its work by the RFC's size limits, and a
/// feature can never widen a capability (no capability section, no control step).

#include "affresco/core/Json.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <optional>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

inline constexpr std::size_t maxManifestBytes = 1u << 20;   ///< A manifest, as JSON: 1 MiB (RFC 0004, "The catalogue").
inline constexpr std::size_t maxCatalogBytes = 8u << 20;    ///< A catalogue's index, as JSON: 8 MiB.
inline constexpr std::size_t maxCatalogFeatures = 10000;    ///< Features in one index.
inline constexpr std::string_view manifestFormat = "affrescoui-feature"; ///< A manifest's "format".

/// The sections only a host grants: a feature that provides one is refused (RFC 0004, "Who may install what").
inline constexpr std::array<std::string_view, 5> capabilitySections = {"services", "connections", "channels", "persist",
                                                                       "permissions"};

/// The steps that reach outside the screen or review it: a feature installed by revision can't contain them.
inline constexpr std::array<std::string_view, 15> controlSteps = {
    "call",    "send",       "connect", "patch",  "respond", "accept", "reject", "preview",
    "endPreview", "restore", "revert", "undo",    "signIn", "signOut", "ask"};

/// @brief A feature's release: semver's three numbers (no pre-release or build part in v1).
struct Release {
	int major = 0; ///< Incompatible changes.
	int minor = 0; ///< Additions.
	int patch = 0; ///< Fixes.
	/// @brief Orders releases as semver does.
	auto operator<=>(const Release&) const = default;
};

/// @brief Reads a release such as "1.2.3".
/// @param text The text.
/// @return The release, or std::nullopt if it isn't three numbers of at most 9 digits joined by dots.
[[nodiscard]] std::optional<Release> parseRelease(std::string_view text);

/// @brief Tells whether a range is one the format knows: "1.2.3", "^1.2.3", "~1.2.3" or ">=1.2.3".
/// @param range The range.
/// @return true if it is.
[[nodiscard]] bool validRange(std::string_view range);

/// @brief Tells whether a release meets a range, with semver's meanings (`^0.2.0` allows 0.2.x only).
/// @param release The release.
/// @param range The range (validRange()).
/// @return true if it does; false if either is malformed.
[[nodiscard]] bool satisfies(std::string_view release, std::string_view range);

/// @brief Tells whether a name can be a feature's: one camelCase word, starting with a lower-case letter, at most 64
/// characters.
/// @param name The name.
/// @return true if it can.
[[nodiscard]] bool validFeatureName(std::string_view name);

/// @brief The control steps in what a feature provides, each with its place.
/// @param provides The manifest's "provides".
/// @param place Its place, e.g. "provides".
/// @return "provides.actions.upload.do: 'call'" for each, in document order.
[[nodiscard]] std::vector<std::string> controlStepsIn(const json& provides, const std::string& place);

/// @brief Checks a manifest on its own: the keys, their types, the names in its namespace and the limits (check 1 of
/// RFC 0004's "Composition"); what it needs from an app is Composition's to check.
/// @param manifest The manifest, as parsed with parseUntrusted().
/// @param place Its place, for errors (e.g. "wordCount").
/// @throws DocumentError If it is malformed, too big, or brings a capability or a control step.
void checkManifest(const json& manifest, const std::string& place);

/// @brief Checks one record of the "features" section: the manifest's "requires" and "provides", kept whole, with
/// where it came from and whether it is on.
/// @param name The feature's name (its key in the section).
/// @param record The record.
/// @param place Its place, for errors ("features.wordCount").
/// @throws DocumentError If it is malformed.
void checkFeatureRecord(const std::string& name, const json& record, const std::string& place);

/// @brief Checks the "features" section: each record (checkFeatureRecord()).
/// @param section The section.
/// @param name "features".
/// @throws DocumentError If a record is malformed.
void checkFeaturesSection(const json& section, const std::string& name);

/// @brief Checks the "exports" section: `"<name>": { "type", "value": "{{...}}", "description" }`, the names dotted
/// identifiers.
/// @param section The section.
/// @param name "exports".
/// @throws DocumentError If an export is malformed.
void checkExportsSection(const json& section, const std::string& name);

/// @brief Checks the "imports" section: `"<alias>": "<feature>@<range>"`.
/// @param section The section.
/// @param name "imports".
/// @throws DocumentError If an import is malformed.
void checkImportsSection(const json& section, const std::string& name);

/// @brief Splits an import, "workbench@^1.0.0", into the feature and its range.
/// @param text The import.
/// @return The feature and the range, or std::nullopt if it isn't one.
[[nodiscard]] std::optional<std::pair<std::string, std::string>> splitImport(std::string_view text);

/// @brief The record installing a manifest writes under /features/<name> (RFC 0004, "The features section").
/// @param manifest A checked manifest.
/// @param source Where it came from ("catalog:<name>", "revision").
/// @param digest Its digest ("sha512:<base64url>"), or empty for none.
/// @param publisher Its publisher ("ed25519:<key>"), or empty for an unsigned feature.
/// @param order Its place among the features.
/// @return The record, enabled.
[[nodiscard]] json recordFor(const json& manifest, const std::string& source, const std::string& digest,
                             const std::string& publisher, int order);

} // namespace affresco::format
