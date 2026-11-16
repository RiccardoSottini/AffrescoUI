#pragma once

/// @file
/// @brief Packages (schema/affrescoui-package.schema.json): a shared version of an app in one file, ending in
/// `.affresco.json`. A package of kind `changes` holds a version's source document and what turns it back into the
/// release it was made from; one of kind `app` also holds that release's files, so that it installs where the app
/// isn't. A package is untrusted input: it never approves itself, widens a capability or lands without the person's
/// yes. This header declares the format; the files of src/packages/ define it (README.md there).

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace affresco::format {

/// The value of a package's "format".
inline constexpr std::string_view packageFormat = "affrescoui-package";
/// The largest package read, in bytes.
inline constexpr std::size_t maxPackageBytes = std::size_t{16} << 20;
/// The most files in a package's `base.files`.
inline constexpr std::size_t maxPackageFiles = 1024;
/// The largest file in a package's `base.files`, in bytes once decoded.
inline constexpr std::size_t maxPackageFileBytes = std::size_t{4} << 20;
/// The longest name of a package or of its base, and its author, in characters.
inline constexpr std::size_t maxPackageNameLength = 80;
/// The longest description of a package, in characters.
inline constexpr std::size_t maxPackageDescriptionLength = 2000;
/// The largest file of trusted publishers the person keeps, in bytes.
inline constexpr std::size_t maxTrustedPublishersBytes = std::size_t{64} << 10;
/// The most publishers in that file.
inline constexpr std::size_t maxTrustedPublishers = 256;

/// @brief What a package holds.
enum class PackageKind {
	Changes, ///< "changes": a version of an app the person already has.
	App      ///< "app": a version and the files of the app it was made from.
};

/// @brief The release a package was made from.
struct PackageBase {
	std::string app;      ///< Its app's id.
	std::string name;     ///< Its app's name.
	std::string release;  ///< Its release, three numbers, or "" if the app has none.
	std::string notation; ///< "json" or "lisp": the notation of its source document.
	std::string tree;     ///< "sha256:<64 hex digits>" of its source document (documentTree()).
	json delta;           ///< A JSON Patch (add, remove and replace only) that turns the package's document into it.
	json files;           ///< For a package of kind App, the app's folder: {path: {"text"} or {"data": base64}}.
};

/// @brief A package, as parsePackage() read it.
struct Package {
	PackageKind kind = PackageKind::Changes; ///< What it holds.
	std::string name;                        ///< The version's name.
	std::string release;                     ///< The version's own release, three numbers.
	std::string description;                 ///< What the person wrote about it, or "".
	std::string author;                      ///< A name the person typed, or "": a claim, not a proof.
	std::string license;                     ///< Its licence, or "".
	std::string engine;                      ///< The AffrescoUI release that wrote it, three numbers.
	PackageBase base;                        ///< The release it was made from.
	json document;                           ///< The version's source document.
	json state;                              ///< The state keys the version adds, or null.
	json assets;                             ///< The assets the version adds, by name, or null.
	json proof;                              ///< {"scenarios": [...]}, as a change set's proof, or null.
	json signature;                          ///< {"publisher", "value"}, as a feature's, or null.
};

/// @brief Reads and checks a package, whole and within the limits above.
/// @param value The package's JSON.
/// @return What it says.
/// @throws DocumentError "AffrescoUI package: <place>: <problem>" if it isn't one.
[[nodiscard]] Package parsePackage(const json& value);

/// @brief Writes a package as its file holds it: the keys parsePackage() reads, the optional ones only when set.
/// @param package The package.
/// @return Its JSON.
[[nodiscard]] json packageJson(const Package& package);

/// @brief The tree of a source document: "sha256:<hex>" of its canonical JSON (RFC 8785), so that the same document
/// has the same tree whatever the order of its keys or its white space.
/// @param sourceDocument The document.
/// @return "sha256:" and 64 lowercase hex digits.
[[nodiscard]] std::string documentTree(const json& sourceDocument);

/// @brief The source document a package was made from: its document with `base.delta` applied.
/// @param package The package.
/// @return The base's document.
/// @throws DocumentError If the delta doesn't apply.
[[nodiscard]] json baseOf(const Package& package);

/// @brief What the person reviews before installing a package: its names and releases, its author and signature as
/// the trusted publishers judge it, and how its base compares with the release they have.
/// @param package The package.
/// @param installedBase The source document of the app the person has, or null if they don't have it.
/// @param trustedPublishers The trusted publishers the person keeps.
/// @return The summary.
[[nodiscard]] json packageSummary(const Package& package, const json& installedBase, const json& trustedPublishers);

} // namespace affresco::format
