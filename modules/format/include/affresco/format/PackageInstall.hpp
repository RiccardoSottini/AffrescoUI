#pragma once

/// @file
/// @brief Installing a package (VERSION_CONTROL.md, "Installing a package"): the summary the person reviews, worked
/// out from the package's content alone, what refuses one, the publishers the person trusts, and a kind "app"
/// package's files written into a new folder. What installing proposes is Screen::proposePackage()'s.

#include "affresco/core/Json.hpp"
#include "affresco/format/Package.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief Where a package would be installed, as the summary compares it: the person's copy of its base's app.
struct PackageReceiver {
	bool known = false;  ///< Whether the person's copy was looked for (false: the summary says nothing of it).
	std::string app;     ///< Its app's id, "" if there is none here.
	json copy = json();  ///< Its source document, or null if the app isn't here.
	std::string release; ///< The release its manifest names, "" if none.
	std::vector<std::string> capabilities; ///< The capabilities its manifest declares (AppCapability::key()).
	/// What a three-way merge of the base, the copy and the package's document can't merge
	/// (`[{"path", "reason", "base", "main", "proposal"}]`), or null if it wasn't worked out.
	json conflicts = json();
};

/// @brief What the person reviews before installing a package, all of it worked out from the package's content, none
/// taken from what it says of itself: packageSummary()'s, and its base against the person's copy (`base.exact`,
/// `base.installedRelease`), its impact per layer, every hook and advice it adds or takes away (`extensions`), what
/// its definitions and actions write (`writes`) and the services they use (`uses`), a kind "app" package's
/// capabilities, permissions and seed data, the models it changes with their migrations, why it can't be installed
/// (`problems`, empty if it can) and what doesn't merge with the person's copy (`conflicts`).
/// @param package The package, read with parsePackage().
/// @param receiver The person's copy.
/// @param trustedPublishers The trusted publishers the person keeps (readTrustedPublishers()).
/// @return The summary.
/// @throws DocumentError If its base can't be worked out (parsePackage() already refuses that).
[[nodiscard]] json packageSummary(const Package& package, const PackageReceiver& receiver,
                                  const json& trustedPublishers);

/// @brief Why a package can't be installed, each naming its place: a kind "changes" package that changes a section a
/// revision of a person's app doesn't (`permissions`, `channels`, `persist`, `services`, `connections`) or adds or
/// takes away its program as a whole, or that is made for an app that isn't here; a document of a newer format than
/// this engine reads; a definition it adds or changes holding a form that isn't a definition (which would run as the
/// program is built).
/// @param package The package, read with parsePackage().
/// @param receiver The person's copy (an app that isn't here counts only when PackageReceiver::known).
/// @return The problems; empty if it can be installed.
[[nodiscard]] std::vector<std::string> installProblems(const Package& package, const PackageReceiver& receiver);

/// @brief The author a package's install proposes as: "plugin:" and its name, made fit for one (appId()).
/// @param package The package.
/// @return It ("plugin:word-count").
[[nodiscard]] std::string packageAuthor(const Package& package);

/// @brief The state keys a package brings that the person's state lacks, as operations (`add /state/<key>`): a key the
/// person has stays as it is.
/// @param package The package.
/// @param receiverState The person's state (anything but an object counts as none).
/// @return The operations.
[[nodiscard]] json stateOperations(const Package& package, const json& receiverState);

/// @brief A package's assets as a revision brings them (`[{"name", "type", "text" | "data"}]`).
/// @param package The package.
/// @return Them; empty if it has none.
[[nodiscard]] json revisionAssets(const Package& package);

/// @brief Reads the trusted publishers' file's text, `{"ed25519:<key>": "<name>"}`: at most maxTrustedPublishersBytes
/// and maxTrustedPublishers keys, each a public key at its fixed length, each name text of at most
/// maxPackageNameLength characters. A malformed file is refused whole.
/// @param text The text.
/// @return The publishers.
/// @throws DocumentError If it is malformed.
[[nodiscard]] json parseTrustedPublishers(std::string_view text);

/// @brief Reads the trusted publishers' file (`publishers.json` in the data folder), as parseTrustedPublishers().
/// @param path The file.
/// @return The publishers; none if there is no file.
/// @throws DocumentError If it is malformed or can't be read.
[[nodiscard]] json readTrustedPublishers(const std::filesystem::path& path);

/// @brief Writes a kind "app" package's files (`base.files`) into a new folder: the app as its base is. Opening it and
/// proposing the package on it (Screen::proposePackage()) installs the version.
/// @param package The package, read with parsePackage().
/// @param folder The folder: it must not exist yet (not even as a link).
/// @throws DocumentError If the package isn't of kind "app", the folder exists, a path leaves the folder or is
/// absolute, a file is over the limits, or a file can't be written (then nothing is left of the folder).
void installAppFiles(const Package& package, const std::filesystem::path& folder);

} // namespace affresco::format
