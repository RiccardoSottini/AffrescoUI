/// @file
/// @brief Features from catalogues: trusted publishers, catalogues, and the install and upgrade revisions.

#include "affresco/format/Features.hpp"

#include "FeatureCatalog.hpp"
#include "FeatureManifest.hpp"
#include "FeatureUpgrade.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

namespace affresco::format {

namespace {

using expect::fail;

/// @brief A release found in a catalogue.
struct Found {
	const CatalogIndex* index = nullptr;     ///< Its catalogue.
	const CatalogRelease* release = nullptr; ///< The release.
};

} // namespace

/// @brief The catalogues, by name, and the trusted keys.
struct Features::Impl {
	std::vector<PublicKey> trusted;                           ///< The publishers whose signatures are accepted.
	std::map<std::string, CatalogIndex, std::less<>> indexes; ///< The catalogues, by name.

	/// @brief Finds a release of a feature: the one asked for, or the newest any catalogue lists.
	/// @param name The feature.
	/// @param release The release, or "" for the newest.
	/// @return It.
	/// @throws DocumentError If no catalogue lists it.
	[[nodiscard]] Found find(std::string_view name, std::string_view release) const {
		Found best;
		for(const auto& [catalog, index] : indexes) {
			const auto feature = index.features.find(name);
			if(feature == index.features.end()) continue;
			for(const CatalogRelease& each : feature->second.releases) {
				if(!release.empty() && each.release != release) continue;
				if(!best.release || checkedRelease(best.release->release) < checkedRelease(each.release))
					best = {&index, &each};
			}
		}
		if(!best.release)
			fail(std::string(name), release.empty() ? "no catalogue lists it"
			                                        : "no catalogue lists its release " + std::string(release));
		return best;
	}
};

Features::Features() : impl_(std::make_unique<Impl>()) {}
Features::~Features() = default;
Features::Features(Features&&) noexcept = default;
Features& Features::operator=(Features&&) noexcept = default;

void Features::trustPublisher(std::string_view publisher) {
	const auto key = parsePublicKey(publisher);
	if(!key) fail("publisher", "expected \"ed25519:<public key>\"");
	impl_->trusted.push_back(*key);
}

std::string Features::addCatalog(const std::filesystem::path& index) {
	std::error_code error;
	const std::filesystem::path file = std::filesystem::canonical(index, error);
	if(error) fail("catalog", "can't be read");
	CatalogIndex catalog =
	    readCatalog(readBounded(file, maxCatalogBytes, "catalog"), file.parent_path(), impl_->trusted);
	std::string name = catalog.name;
	impl_->indexes.insert_or_assign(name, std::move(catalog));
	return name;
}

json Features::catalog(const json& document) const {
	std::map<std::string, json, std::less<>> rows;
	for(const auto& [catalog, index] : impl_->indexes)
		for(const auto& [name, feature] : index.features) {
			const CatalogRelease& newest = feature.releases.front();
			if(const auto row = rows.find(name);
			   row != rows.end() &&
			   checkedRelease(newest.release) <= checkedRelease(row->second["version"].get<std::string>()))
				continue;
			const auto publisher = index.publishers.find(newest.publisher);
			const json* record = installedRecord(document, name);
			rows[name] = {{"name", name},
			              {"version", newest.release},
			              {"publisher", publisher != index.publishers.end() && !publisher->second.empty()
			                                ? publisher->second
			                                : newest.publisher},
			              {"title", feature.title},
			              {"description", feature.description},
			              {"installed", record != nullptr},
			              {"enabled", record != nullptr && record->value("enabled", false)}};
		}
	json list = json::array();
	for(auto& [name, row] : rows) list.push_back(std::move(row));
	return list;
}

json Features::releaseRecord(std::string_view name, std::string_view release, int order) const {
	const Found found = impl_->find(name, release);
	const json manifest = fetchManifest(*found.index, std::string(name), *found.release, impl_->trusted);
	json record =
	    recordFor(manifest, "catalog:" + found.index->name, found.release->digest, found.release->publisher, order);
	record["installed"] = installedDigest(record); // so an edit made in place is told (FeatureUpgrade.hpp)
	return record;
}

std::string Features::newestRelease(std::string_view name) const {
	std::string newest;
	for(const auto& [catalog, index] : impl_->indexes)
		if(const auto feature = index.features.find(name); feature != index.features.end())
			for(const CatalogRelease& each : feature->second.releases)
				if(newest.empty() || checkedRelease(newest) < checkedRelease(each.release)) newest = each.release;
	return newest;
}

json Features::installRevision(const json& document, std::string_view name, std::string_view release,
                               std::string_view author) const {
	if(installedRecord(document, name))
		fail(std::string(name), "is installed already (upgrade it, or remove it first)");
	// After the last one, in steps of 10; a peer may have written any number, so it is read in 64 bits and capped
	std::int64_t order = 0;
	if(const auto features = document.find("features"); features != document.end() && features->is_object())
		for(const auto& [other, record] : features->items())
			if(const auto value = record.find("order"); value != record.end() && value->is_number_integer())
				order = std::max(order, value->get<std::int64_t>());
	const int next = static_cast<int>(std::min<std::int64_t>(order, std::numeric_limits<int>::max() - 10) + 10);
	const json record = releaseRecord(name, release, next);
	const std::string& catalog = record["source"].get_ref<const std::string&>();
	json set = json::object();
	if(document.contains("features"))
		set["/features/" + std::string(name)] = record;
	else
		set["/features"] = {{std::string(name), record}}; // a document without the section gets it whole
	return {{"author", author},
	        {"message", "Install " + std::string(name) + " " + record["release"].get<std::string>() + " from " +
	                        catalog.substr(catalog.find(':') + 1)},
	        {"set", std::move(set)}};
}

json Features::upgradeRevision(const json& document, std::string_view name, std::string_view author) const {
	UpgradeParts parts = upgradeParts(*this, document, name);
	return {{"author", author},
	        {"message", "Upgrade " + std::string(name) + " to " + parts.to},
	        {"set", {{"/features/" + std::string(name), std::move(parts.theirs)}}}};
}

} // namespace affresco::format
