#pragma once

/// @file
/// @brief The `packages` service, which a host grants a screen to export and install packages: the work of each
/// method, done on a worker thread, and what it is refused with.

#include "affresco/PackagesProvider.hpp"
#include "affresco/core/Json.hpp"

#include <stdexcept>
#include <stop_token>
#include <string>

namespace affresco::detail {

/// @brief Why a method of the service is refused: a code for the screen, and a message naming roots, never paths.
class PackagesFailure : public std::runtime_error {
public:
	/// @brief A failure.
	/// @param failureCode The code ("invalid", "notFound", "exists", "tooLarge", "refused", "failed").
	/// @param message For people.
	PackagesFailure(std::string failureCode, const std::string& message)
	    : std::runtime_error(message), code_(std::move(failureCode)) {}
	/// @brief The code.
	/// @return It.
	[[nodiscard]] const std::string& code() const noexcept { return code_; }

private:
	std::string code_; ///< See the constructor.
};

/// @brief `export`: a package of an app in a granted root, with what it leaves out. The app's history is read from a
/// copy of its data, so the person's own data folder is never written, nor its lock taken.
/// @param grant What the service may touch.
/// @param with `{root, app, name, release?, description?, author?, whole?}`, checked by schema.
/// @param stop Asks the work to stop.
/// @return `{package, summary, leftOut: {records, commits}}`.
/// @throws PackagesFailure If it can't be made.
[[nodiscard]] json exportVersion(const PackagesGrant& grant, const json& with, const std::stop_token& stop);

/// @brief `inspect`: what installing a package would do, against the copy of its base app a granted root holds.
/// @param grant What the service may touch.
/// @param with `{package}`.
/// @return `{summary}`, its `problems` saying why it can't be installed.
/// @throws PackagesFailure If it isn't a package.
[[nodiscard]] json inspectVersion(const PackagesGrant& grant, const json& with);

/// @brief `install`: a new folder in the writable root, the package's base app in it (copied from the root that has
/// it, or the package's own files), and the package proposed on it. On any failure the folder goes.
/// @param grant What the service may touch.
/// @param with `{package, id, name}`.
/// @param stop Asks the work to stop.
/// @return `{id, sub, proposal}`: sub is the folder of its screen ("" or "/json"), proposal whether one waits.
/// @throws PackagesFailure If it can't be installed: nothing is left in the writable root.
[[nodiscard]] json installVersion(const PackagesGrant& grant, const json& with, const std::stop_token& stop);

/// @brief `setAside`: sets aside the data the data folder keeps for a folder of the writable root
/// (Screen::setDataAside()), once its app is deleted or before a new app there opens, so a new app of an old one's
/// name starts empty. Nothing is erased.
/// @param grant What the service may touch.
/// @param with `{app}`: the folder's name.
/// @return `{moved}`: how many were set aside.
/// @throws PackagesFailure If the name isn't a folder's, or one couldn't be moved (the others are).
[[nodiscard]] json setAsideData(const PackagesGrant& grant, const json& with);

/// @brief `catalogue`: the entries of the granted catalogue (PackagesCatalogue), its index read and checked again.
/// @param grant What the service may touch.
/// @return `{name, updated, signed, entries: [{id, kind, title, description, release, base, installed}]}`: kind is
/// "package" or "feature", installed whether an app of that id is in the writable root.
/// @throws PackagesFailure If no catalogue is granted, or its index is refused.
[[nodiscard]] json catalogueEntries(const PackagesGrant& grant);

/// @brief A catalogue's entry for `inspect` and `install`: its file, read from the catalogue's folder and checked
/// against the index's digest and size.
struct CatalogueEntry {
	std::string text = {};        ///< The file's text: a package, or a feature's manifest.
	bool feature = false;         ///< Whether it is a feature's manifest.
	json listed = json::object(); ///< `{catalogue, updated, signed}`, for the review's line.
};

/// @brief Reads the catalogue's entry a call names.
/// @param grant What the service may touch.
/// @param with The call's arguments: `catalogue` (the entry's id) and `release` (its newest if missing).
/// @return The entry.
/// @throws PackagesFailure If there is no such entry, or its file isn't the one the index lists.
[[nodiscard]] CatalogueEntry catalogueEntry(const PackagesGrant& grant, const json& with);

/// @brief What the review shows of a feature from a catalogue: its names and release, its signature, what it
/// provides and what it needs. A feature is added from inside an app, which binds what it needs, so it has no install.
/// @param text The manifest's text, checked against the index.
/// @param trusted The trusted publishers the person keeps.
/// @return `{kind: "feature", name, release, description, license, signature, definitions, needs, problems}`.
[[nodiscard]] json featureReview(const std::string& text, const json& trusted);

} // namespace affresco::detail
