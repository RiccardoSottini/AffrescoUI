#pragma once

/// @file
/// @brief Installing a package on a repository (ScreenPackages.cpp): what Screen::inspectPackage() and
/// Screen::proposePackage() do, for a host or a test that holds the repository and the app's manifest itself.

#include "affresco/core/VersionControl.hpp"
#include "affresco/format/App.hpp"
#include "affresco/vcs/Repository.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace affresco::detail {

/// @brief What installing a package would do: format::packageSummary(), with the repository as the person's copy.
/// @param app The app the repository's document was loaded from, if any: a package is for the app of its id.
/// @param repository The repository.
/// @param packageJson The package's file.
/// @param trustedPublishers The publishers the person trusts.
/// @return The summary.
/// @throws DocumentError If it isn't a package, naming the place.
[[nodiscard]] json inspectPackage(const std::optional<format::AppManifest>& app, const vcs::Repository& repository,
                                  std::string_view packageJson, const json& trustedPublishers);

/// @brief Installs a package as a proposal only the person accepts (Screen::proposePackage()).
/// @param app The app the repository's document was loaded from, if any.
/// @param repository The repository.
/// @param packageJson The package's file.
/// @param error Receives why it was refused (may be nullptr).
/// @return Proposed, or Rejected with why; never Applied.
RevisionResult proposePackage(const std::optional<format::AppManifest>& app, vcs::Repository& repository,
                              std::string_view packageJson, std::string* error);

} // namespace affresco::detail
