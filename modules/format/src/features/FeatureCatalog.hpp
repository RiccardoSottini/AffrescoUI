#pragma once

/// @file
/// @brief Catalogue v0 (RFC 0004, "The catalogue"): a signed JSON index of features and their releases, kept in a
/// local folder beside the manifests it lists, and the checks a manifest passes before it may be installed from it.
///
/// Indexes and manifests are untrusted input: their sizes are bounded before they are read, their URLs stay inside the
/// catalogue's folder, digests and keys are compared in constant time, and every signature must be a trusted
/// publisher's.

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

/// @brief One release of a feature, as the index lists it.
struct CatalogRelease {
	std::string release;          ///< "1.2.0".
	std::string url;              ///< Its manifest, relative to the index's folder.
	std::string digest;           ///< "sha512:<base64url>" of the manifest's bytes.
	std::string publisher;        ///< "ed25519:<key>" that signed the manifest.
	std::size_t size = 0;         ///< The manifest's size in bytes.
	json needs = json::object();  ///< The manifest's "requires", to show before fetching it.
};

/// @brief One feature of an index.
struct CatalogFeature {
	std::string title;                    ///< For people.
	std::string description;              ///< For people.
	std::vector<CatalogRelease> releases; ///< Its releases, newest first.
};

/// @brief An index whose signature was checked.
struct CatalogIndex {
	std::string name;                                            ///< The catalogue's name.
	std::filesystem::path folder;                                ///< Its folder (canonical): URLs stay in it.
	std::map<std::string, std::string, std::less<>> publishers;  ///< Each key's name for people.
	std::map<std::string, CatalogFeature, std::less<>> features; ///< By name.
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

} // namespace affresco::format
