#pragma once

/// @file
/// @brief Catalogue v0 (RFC 0004, "The catalogue"): a signed JSON index of features and packages (versions of apps)
/// and their releases, kept in a local folder beside the files it lists, and the checks a file passes before it may
/// be installed from it.
///
/// Indexes, manifests and packages are untrusted input: their sizes are bounded before they are read, their URLs stay
/// inside the catalogue's folder, digests and keys are compared in constant time, and every signature must be a
/// trusted publisher's. An unsigned index is read only by readUnsignedCatalog(), for a development build's explicit
/// option and for `affrescoui catalogue check`.

#include "FeatureManifest.hpp"
#include "FeatureSignatures.hpp"
#include "affresco/core/Json.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

inline constexpr std::string_view catalogFormat = "affrescoui-catalog"; ///< An index's "format".

/// @brief The numbers of a release that reading an index checked.
/// @param text The release, e.g. "1.2.0".
/// @return Them; 0.0.0 for a text that isn't one, which a checked index never holds.
[[nodiscard]] inline Release checkedRelease(std::string_view text) { return parseRelease(text).value_or(Release{}); }

/// @brief One release of a feature, as the index lists it.
struct CatalogRelease {
	std::string release;         ///< "1.2.0".
	std::string url;             ///< Its manifest, relative to the index's folder.
	std::string digest;          ///< "sha512:<base64url>" of the manifest's bytes.
	std::string publisher;       ///< "ed25519:<key>" that signed the manifest, or "" for one unsigned.
	std::size_t size = 0;        ///< The manifest's size in bytes.
	json needs = json::object(); ///< The manifest's "requires", to show before fetching it.
};

/// @brief One release of a package (a version of an app, or a whole app), as the index lists it.
struct CatalogPackageRelease {
	std::string release = {};     ///< The package's own release, "1.0.0".
	std::string url = {};         ///< Its file, relative to the index's folder.
	std::string digest = {};      ///< "sha512:<base64url>" of the file's bytes.
	std::string publisher = {};   ///< "ed25519:<key>" that signed the package, or "" for one unsigned.
	std::size_t size = 0;         ///< The file's size in bytes.
	std::string baseApp = {};     ///< The id of the app it was made on.
	std::string baseRelease = {}; ///< That app's release, or "" if it had none.
};

/// @brief One package of an index.
struct CatalogPackage {
	std::string title = {};                           ///< For people.
	std::string description = {};                     ///< For people.
	std::string base = {};                            ///< The id of the app it is a version of.
	std::vector<CatalogPackageRelease> releases = {}; ///< Its releases, newest first.
};

/// @brief One feature of an index.
struct CatalogFeature {
	std::string title;                    ///< For people.
	std::string description;              ///< For people.
	std::vector<CatalogRelease> releases; ///< Its releases, newest first.
};

/// @brief An index whose signature was checked (or, from readUnsignedCatalog(), an index that has none).
struct CatalogIndex {
	std::string name;                                                 ///< The catalogue's name.
	std::filesystem::path folder;                                     ///< Its folder (canonical): URLs stay in it.
	std::map<std::string, std::string, std::less<>> publishers;       ///< Each key's name for people.
	std::map<std::string, CatalogFeature, std::less<>> features;      ///< By name.
	std::map<std::string, CatalogPackage, std::less<>> packages = {}; ///< By id, never a feature's name.
	std::string updated = {};                                         ///< When it was checked ("2026-11-17"), or "".
	std::string signer = {}; ///< "ed25519:<key>" that signed it, or "" for none.
};

/// @brief Reads a file of at most a size, without reading more.
/// @param file The file.
/// @param limit The most bytes it may have.
/// @param place Its place, for errors.
/// @return Its bytes.
/// @throws DocumentError If it can't be read or is larger.
[[nodiscard]] std::string readBounded(const std::filesystem::path& file, std::size_t limit, const std::string& place);

/// @brief Tells whether a key is among the trusted ones, comparing each in constant time.
/// @param publisher "ed25519:<key>".
/// @param trusted The trusted keys.
/// @return true if it is.
[[nodiscard]] bool isTrusted(std::string_view publisher, const std::vector<PublicKey>& trusted);

/// @brief Checks an index's text and signature, and reads it.
/// @param text The index (at most maxCatalogBytes).
/// @param folder Its folder, canonical.
/// @param trusted The publishers the host trusts.
/// @return The index.
/// @throws DocumentError With the reason: malformed, too big, unsigned, signed by a key not trusted, or changed after
/// signing.
[[nodiscard]] CatalogIndex readCatalog(std::string_view text, const std::filesystem::path& folder,
                                       const std::vector<PublicKey>& trusted);

/// @brief Reads an index that may be unsigned, for a development build that was told to trust it and for the checks
/// a pull request runs: every check of readCatalog() but whether a trusted key signed it. A signature it has must
/// still verify.
/// @param text The index (at most maxCatalogBytes).
/// @param folder Its folder, canonical.
/// @return The index; its signer is "" if it has no signature.
/// @throws DocumentError With the reason: malformed, too big, or changed after signing.
[[nodiscard]] CatalogIndex readUnsignedCatalog(std::string_view text, const std::filesystem::path& folder);

/// @brief Fetches a release's manifest from an index's folder and checks it: its size, its digest, its signature by
/// the release's publisher, its shape (checkManifest()), its name and its release.
/// @param index The index.
/// @param name The feature.
/// @param release The release.
/// @param trusted The publishers the host trusts.
/// @return The manifest.
/// @throws DocumentError With the reason.
[[nodiscard]] json fetchManifest(const CatalogIndex& index, const std::string& name, const CatalogRelease& release,
                                 const std::vector<PublicKey>& trusted);

/// @brief Reads a file an index lists from its folder and checks it is the one the index describes: its URL stays in
/// the folder (links included), it has at most a size, and its size and digest are the index's.
/// @param index The index.
/// @param url The file's URL, relative to the index's folder.
/// @param size Its size, as the index says.
/// @param digest Its digest, as the index says.
/// @param most The most bytes such a file may have.
/// @param place The entry's place, for errors.
/// @return Its bytes.
/// @throws DocumentError With the reason.
[[nodiscard]] std::string readListedFile(const CatalogIndex& index, std::string_view url, std::size_t size,
                                         std::string_view digest, std::size_t most, const std::string& place);

/// @brief Fetches a release of a package from an index's folder and checks what the index says of it: its size, its
/// digest, that it reads as a package (readPackage()), that it is that release of a version of that base app, and,
/// when the index names a publisher, that the package is signed by it. What installing it would do is the install's
/// review, as for a file from disk.
/// @param index The index.
/// @param id The package's id.
/// @param release The release.
/// @return The package's text.
/// @throws DocumentError With the reason.
[[nodiscard]] std::string fetchPackage(const CatalogIndex& index, const std::string& id,
                                       const CatalogPackageRelease& release);

} // namespace affresco::format
