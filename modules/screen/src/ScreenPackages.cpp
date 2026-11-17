/// @file
/// @brief A screen's packages: what installing one would do (inspectPackage()), and installing it as a proposal only
/// the person accepts (proposePackage()), as VERSION_CONTROL.md, "Installing a package", says.

#include "ScreenPackages.hpp"
#include "ScreenImpl.hpp"
#include "affresco/format/PackageInstall.hpp"
#include "affresco/vcs/Merge.hpp"

#include <optional>
#include <tuple>
#include <utility>

namespace affresco {

namespace {

/// @brief The person's copy of a package's base app, as this screen holds it.
/// @param app The app the screen was loaded from, if any.
/// @param repository Its repository.
/// @param package The package.
/// @return The receiver: the screen's source document if it is the package's app, with what doesn't merge.
format::PackageReceiver receiverOf(const std::optional<format::AppManifest>& app, const vcs::Repository& repository,
                                   const format::Package& package) {
	format::PackageReceiver receiver;
	receiver.known = true;
	if(!app || app->id != package.base.app) return receiver;
	receiver.app = app->id;
	receiver.release = app->release;
	for(const format::AppCapability& capability : app->capabilities) receiver.capabilities.push_back(capability.key());
	receiver.copy = repository.sourceDocument();
	// mine is main, theirs the proposal, as the merge's names read for someone else's version
	receiver.conflicts = vcs::mergeDocuments(format::baseOf(package), receiver.copy, package.document).conflicts;
	return receiver;
}

/// @brief Reads a package's text, as a file holds it.
/// @param packageJson The text.
/// @return The package and its JSON.
/// @throws DocumentError If it isn't a package (or isn't JSON), naming the place.
std::pair<format::Package, json> readPackageText(std::string_view packageJson) {
	try {
		format::Package package = format::readPackage(packageJson);
		return {std::move(package), parseUntrusted(packageJson)};
	} catch(const json::exception& error) {
		throw DocumentError(std::string("AffrescoUI package: not JSON: ") + error.what());
	}
}

/// @brief A rejected result.
/// @param revision main's revision number.
/// @param why Why.
/// @param error Receives it (may be nullptr).
/// @return The result.
RevisionResult refused(int revision, const std::string& why, std::string* error) {
	if(error) *error = why;
	return {.status = RevisionStatus::Rejected, .revision = revision, .commit = {}, .proposal = 0, .error = why};
}

} // namespace

namespace detail {

json inspectPackage(const std::optional<format::AppManifest>& app, const vcs::Repository& repository,
                    std::string_view packageJson, const json& trustedPublishers) {
	const format::Package package = readPackageText(packageJson).first;
	return format::packageSummary(package, receiverOf(app, repository, package), trustedPublishers);
}

RevisionResult proposePackage(const std::optional<format::AppManifest>& app, vcs::Repository& repository,
                              std::string_view packageJson, std::string* error) {
	format::Package package;
	json value;
	try {
		std::tie(package, value) = readPackageText(packageJson);
	} catch(const DocumentError& failure) {
		return refused(repository.head().number, failure.what(), error);
	}
	if(repository.hasImported(value))
		return refused(repository.head().number,
		               "AffrescoUI package: " + package.name + " " + package.release + " is already installed", error);
	const format::PackageReceiver receiver = receiverOf(app, repository, package);
	if(const auto problems = format::installProblems(package, receiver); !problems.empty()) {
		std::string why = "AffrescoUI package: can't be installed:";
		for(const std::string& problem : problems) why += "\n  " + problem;
		return refused(repository.head().number, why, error);
	}
	vcs::Repository::MergeRequest request;
	request.base = format::baseOf(package);
	request.theirs = package.document;
	request.author = format::packageAuthor(package);
	request.message = "Install " + package.name + " " + package.release;
	request.personOnly = true;
	request.assets = format::revisionAssets(package);
	request.extra = format::stateOperations(package, repository.state());
	vcs::Repository::MergeProposed result = repository.proposeMerge(request);
	if(result.status == RevisionStatus::Proposed) repository.recordImported(value); // the same file again is refused
	else if(error) *error = result.error;
	return RevisionResult{result.status, result.revision, std::move(result.commit), result.proposal,
	                      std::move(result.error)};
}

} // namespace detail

json Screen::inspectPackage(std::string_view packageJson, const json& trustedPublishers) const {
	return detail::inspectPackage(impl_->app, impl_->repository, packageJson, trustedPublishers);
}

RevisionResult Screen::proposePackage(std::string_view packageJson, std::string* error) {
	return detail::proposePackage(impl_->app, impl_->repository, packageJson, error);
}

} // namespace affresco
