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
/// @return `{id, proposal}`: proposal is whether a proposal waits for the person.
/// @throws PackagesFailure If it can't be installed: nothing is left in the writable root.
[[nodiscard]] json installVersion(const PackagesGrant& grant, const json& with, const std::stop_token& stop);

} // namespace affresco::detail
