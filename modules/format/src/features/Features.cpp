/// @file
/// @brief Features from catalogues: trusted publishers, catalogues, and the install and upgrade revisions.

#include "affresco/format/Features.hpp"

#include "FeatureCatalog.hpp"
#include "FeatureManifest.hpp"

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

/// @brief The record of an installed feature.
/// @param document The document.
/// @param name The feature.
/// @return It, or nullptr.
const json* installed(const json& document, std::string_view name) {
	const auto features = document.find("features");
	if(features == document.end() || !features->is_object()) return nullptr;
	const auto record = features->find(name);
	return record == features->end() ? nullptr : &*record;
}

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
				if(!best.release || *parseRelease(best.release->release) < *parseRelease(each.release))
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
			   row != rows.end() && *parseRelease(newest.release) <= *parseRelease(row->second["version"].get<std::string>()))
				continue;
			const auto publisher = index.publishers.find(newest.publisher);
			const json* record = installed(document, name);
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

json Features::installRevision(const json& document, std::string_view name, std::string_view release,
                               std::string_view author) const {
	if(installed(document, name)) fail(std::string(name), "is installed already (upgrade it, or remove it first)");
	const Found found = impl_->find(name, release);
	const json manifest = fetchManifest(*found.index, std::string(name), *found.release, impl_->trusted);
	// After the last one, in steps of 10; a peer may have written any number, so it is read in 64 bits and capped
	std::int64_t order = 0;
	if(const auto features = document.find("features"); features != document.end() && features->is_object())
		for(const auto& [other, record] : features->items())
			if(const auto value = record.find("order"); value != record.end() && value->is_number_integer())
				order = std::max(order, value->get<std::int64_t>());
	const int next = static_cast<int>(std::min<std::int64_t>(order, std::numeric_limits<int>::max() - 10) + 10);
	const json record =
	    recordFor(manifest, "catalog:" + found.index->name, found.release->digest, found.release->publisher, next);
	json set = json::object();
	if(document.contains("features"))
		set["/features/" + std::string(name)] = record;
	else
		set["/features"] = {{std::string(name), record}}; // a document without the section gets it whole
	return {{"author", author},
	        {"message", "Install " + std::string(name) + " " + found.release->release + " from " + found.index->name},
	        {"set", std::move(set)}};
}

json Features::upgradeRevision(const json& document, std::string_view name, std::string_view author) const {
	const json* record = installed(document, name);
	if(!record) fail(std::string(name), "isn't installed");
	const Found found = impl_->find(name, "");
	const auto current = parseRelease(record->value("release", ""));
	if(current && *parseRelease(found.release->release) <= *current)
		fail(std::string(name), "is up to date (" + record->value("release", "") + ")");
	// An upgrade never changes who the feature comes from: that takes a removal and a new install, both reviewed
	const json publisher = record->value("publisher", json());
	const auto installedKey = publisher.is_string() ? parsePublicKey(publisher.get_ref<const std::string&>()) : std::nullopt;
	const auto newKey = parsePublicKey(found.release->publisher);
	if(!installedKey || !newKey || !sameKey(*installedKey, *newKey))
		fail(std::string(name), "its new release comes from another publisher (remove it and install that one)");
	const json manifest = fetchManifest(*found.index, std::string(name), *found.release, impl_->trusted);
	json upgraded = recordFor(manifest, "catalog:" + found.index->name, found.release->digest,
	                          found.release->publisher, record->value("order", 0));
	upgraded["enabled"] = record->value("enabled", true);
	return {{"author", author},
	        {"message", "Upgrade " + std::string(name) + " to " + found.release->release},
	        {"set", {{"/features/" + std::string(name), std::move(upgraded)}}}};
}

} // namespace affresco::format
