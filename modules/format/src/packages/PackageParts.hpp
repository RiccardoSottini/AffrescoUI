#pragma once

/// @file
/// @brief What Package.cpp and PackageInstall.cpp share: a document's definitions, and the summary's install parts.

#include "affresco/core/Json.hpp"
#include "affresco/format/PackageInstall.hpp"

namespace affresco::format::detail {

/// @brief The definitions of a document's programs, as "file/name": text.
/// @param document The document.
/// @return Them.
[[nodiscard]] json definitionsOf(const json& document);

/// @brief Adds to a package's summary what installing it would do (packageSummary()): its base against the person's
/// copy, impact, extensions, writes, uses, models, problems, conflicts, and a kind "app" package's capabilities,
/// permissions and seed data.
/// @param summary The summary, changed in place.
/// @param package The package.
/// @param base Its base's document (baseOf()).
/// @param receiver The person's copy.
void addInstallParts(json& summary, const Package& package, const json& base, const PackageReceiver& receiver);

} // namespace affresco::format::detail
