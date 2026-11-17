/// @file
/// @brief The `packages` service's catalogue (PackagesService.hpp): its index read and checked at each call, its
/// entries listed, an entry's file read from its folder against the index's digest, and a feature's review.
///
/// A host grants a catalogue with the keys it trusts to have signed the index (PackagesCatalogue). An unsigned index
/// is read only when the host was told to, for a development build. Listing an entry is no authority: a package from
/// the catalogue is installed as one from disk would be, through the same review, its changes a proposal only the
/// person accepts.

#include "packages/PackagesService.hpp"

#include "FeatureCatalog.hpp"
#include "FeatureManifest.hpp"
#include "FeatureSignatures.hpp"
#include "affresco/format/Package.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace affresco::detail {

namespace {

namespace fs = std::filesystem;

/// @brief The granted catalogue's index, read and checked: signed by a key the host trusts, or unsigned when the host
/// was told to trust that.
/// @param grant What the service may touch.
/// @return The index.
/// @throws PackagesFailure If no catalogue is granted, or its index is refused.
format::CatalogIndex openCatalogue(const PackagesGrant& grant) {
	const PackagesCatalogue& granted = grant.catalogue;
	if(granted.folder.empty()) throw PackagesFailure("notFound", "no catalogue is granted");
	std::error_code error;
	const fs::path folder = fs::canonical(granted.folder, error);
	if(error) throw PackagesFailure("notFound", "the catalogue isn't there");
	try {
		const std::string text = format::readBounded(folder / "index.json", format::maxCatalogBytes, "catalog");
		if(granted.trustUnsigned) return format::readUnsignedCatalog(text, folder);
		std::vector<format::PublicKey> trusted;
		for(const std::string& key : granted.trusted)
			if(const auto parsed = format::parsePublicKey(key)) trusted.push_back(*parsed);
		if(!parseUntrusted(text).contains("signature"))
			throw PackagesFailure("refused", "the catalogue's index is unsigned: only a release signs it");
		return format::readCatalog(text, folder, trusted);
	} catch(const PackagesFailure&) {
		throw;
	} catch(const std::exception& failure) {
		throw PackagesFailure("refused", std::string("the catalogue's index: ") + failure.what());
	}
}

/// @brief What the review's line says of where an entry came from.
/// @param index The index.
/// @return `{catalogue, updated, signed}`.
json listedIn(const format::CatalogIndex& index) {
	return {{"catalogue", index.name}, {"updated", index.updated}, {"signed", !index.signer.empty()}};
}

/// @brief Whether an app of an id is in the root the service installs into.
/// @param grant What the service may touch.
/// @param id The id.
/// @return Whether a folder of that name is there.
bool installedAs(const PackagesGrant& grant, const std::string& id) {
	const auto root = grant.roots.find(grant.writable);
	if(root == grant.roots.end()) return false;
	std::error_code error;
	return fs::is_directory(root->second / id, error);
}

/// @brief Picks a release of an entry: the one a call names, else the newest.
/// @param releases Its releases, newest first.
/// @param with The call's arguments.
/// @param id The entry's id, for errors.
/// @return The release.
/// @throws PackagesFailure If it doesn't list the one named.
template <typename Release>
const Release& pickRelease(const std::vector<Release>& releases, const json& with, const std::string& id) {
	const std::string wanted = with.value("release", std::string());
	if(wanted.empty()) return releases.front();
	const auto found = std::ranges::find(releases, wanted, &Release::release);
	if(found == releases.end()) throw PackagesFailure("notFound", "catalogue: " + id + " has no release " + wanted);
	return *found;
}

/// @brief The names a feature's "provides" adds, as "<part>.<name>" ("actions.pick"), its program's definitions as
/// "program.<file>/<address>".
/// @param provides The part.
/// @return Them, in the manifest's order.
json providedNames(const json& provides) {
	json names = json::array();
	for(const auto& [part, value] : provides.items()) {
		if(!value.is_object()) continue;
		for(const auto& [name, inner] : value.items()) {
			if(part != "program" || !inner.is_object()) names.push_back(std::string(part).append(".").append(name));
			else
				for(const auto& [address, unused] : inner.items())
					names.push_back(std::string("program.").append(name).append("/").append(address));
		}
	}
	return names;
}

} // namespace

json catalogueEntries(const PackagesGrant& grant) {
	const format::CatalogIndex index = openCatalogue(grant);
	json entries = json::array();
	for(const auto& [id, package] : index.packages)
		entries.push_back({{"id", id},
		                   {"kind", "package"},
		                   {"title", package.title.empty() ? id : package.title},
		                   {"description", package.description},
		                   {"release", package.releases.front().release},
		                   {"base", package.base},
		                   {"installed", installedAs(grant, id)}});
	for(const auto& [name, feature] : index.features)
		entries.push_back({{"id", name},
		                   {"kind", "feature"},
		                   {"title", feature.title.empty() ? name : feature.title},
		                   {"description", feature.description},
		                   {"release", feature.releases.front().release},
		                   {"base", ""},
		                   {"installed", false}});
	return {{"name", index.name},
	        {"updated", index.updated},
	        {"signed", !index.signer.empty()},
	        {"entries", std::move(entries)}};
}

CatalogueEntry catalogueEntry(const PackagesGrant& grant, const json& with) {
	const std::string id = with.at("catalogue").get<std::string>();
	const format::CatalogIndex index = openCatalogue(grant);
	CatalogueEntry entry;
	entry.listed = listedIn(index);
	try {
		if(const auto package = index.packages.find(id); package != index.packages.end()) {
			entry.text = format::fetchPackage(index, id, pickRelease(package->second.releases, with, id));
			return entry;
		}
		const auto feature = index.features.find(id);
		if(feature == index.features.end()) throw PackagesFailure("notFound", "catalogue: it lists no " + id);
		const format::CatalogRelease& release = pickRelease(feature->second.releases, with, id);
		entry.text = format::readListedFile(index, release.url, release.size, release.digest, format::maxManifestBytes,
		                                    id + " " + release.release);
		entry.feature = true;
		return entry;
	} catch(const PackagesFailure&) {
		throw;
	} catch(const std::exception& failure) {
		throw PackagesFailure("refused", std::string("catalogue: ") + failure.what());
	}
}

json featureReview(const std::string& text, const json& trusted) {
	json manifest;
	try {
		manifest = parseUntrusted(text);
		expect::object(manifest, "feature");
		if(manifest.contains("signature")) static_cast<void>(format::verifySignature(manifest, "feature"));
		format::checkManifest(manifest, "feature");
	} catch(const std::exception& failure) {
		throw PackagesFailure("refused", std::string("catalogue: ") + failure.what());
	}
	json needs = json::array();
	const json declared = manifest.value("needs", json::object()); // a local: value() returns a temporary
	for(const auto& [name, need] : declared.items())
		needs.push_back(
		    {{"name", name}, {"kind", need.value("kind", "")}, {"description", need.value("description", "")}});
	const std::string name = manifest.value("name", "");
	return {{"kind", "feature"},
	        {"name", manifest.value("title", name)},
	        {"release", manifest.value("release", "")},
	        {"description", manifest.value("description", "")},
	        {"license", manifest.value("license", "")},
	        {"signature", format::signatureStanding(manifest.value("signature", json()), trusted)},
	        {"definitions",
	         {{"added", providedNames(manifest.value("provides", json::object()))},
	          {"changed", json::array()},
	          {"removed", json::array()}}},
	        {"needs", std::move(needs)},
	        {"problems", json::array()},
	        {"conflicts", json::array()}};
}

} // namespace affresco::detail
