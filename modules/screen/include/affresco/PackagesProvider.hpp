#pragma once

/// @file
/// @brief Packages for a host: the service it grants a screen to export the running app and install shared versions.
///
/// A screen declares it as `"services": {"packages": {"provider": "packages"}}`. `export` makes a package of an app
/// in a granted root (its changes, or the app as a whole), `inspect` says what installing one would do, and `install`
/// makes a new folder in the writable root from one, its changes waiting as a proposal only the person accepts
/// (VERSION_CONTROL.md, "Installing a package"). With a catalogue granted, `catalogue` lists its entries, and
/// `inspect` and `install` take one of them in place of a package (`catalogue/README.md`). Arguments, results and
/// errors name the roots, never a path.

#include "affresco/services/Provider.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace affresco {

/// @brief A catalogue the `packages` service lists and installs from: a folder with an `index.json` that lists
/// packages and features, each file pinned by its digest.
struct PackagesCatalogue {
	std::filesystem::path folder = {};     ///< The index's folder, or empty for no catalogue.
	std::vector<std::string> trusted = {}; ///< The keys ("ed25519:...") whose signature on the index is accepted.
	/// Also read an index with no signature: a development build's explicit option, never a release's default.
	bool trustUnsigned = false;
};

/// @brief What the `packages` service may touch.
struct PackagesGrant {
	/// The folders of apps it reads, by the names a screen gives them ("examples", "apps").
	std::map<std::string, std::filesystem::path, std::less<>> roots = {};
	std::string writable = {};                  ///< The root it installs into, or "" for none.
	std::filesystem::path dataDirectory = {};   ///< The host's data folder: the apps' histories, the trusted publishers.
	PackagesCatalogue catalogue = {};           ///< The catalogue it lists, if any.
};

/// @brief Makes the `packages` provider.
/// @param grant What it may touch.
/// @return The provider, for ServiceRegistry::add().
[[nodiscard]] std::unique_ptr<services::Provider> makePackagesProvider(PackagesGrant grant);

} // namespace affresco
