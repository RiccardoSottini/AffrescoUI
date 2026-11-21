/// @file
/// @brief Upgrading an installed feature without losing its edits (FeatureUpgrade.hpp).

#include "FeatureUpgrade.hpp"

#include "FeatureCatalog.hpp"
#include "FeatureSignatures.hpp"

#include "affresco/core/JsonDigest.hpp"

#include <algorithm>
#include <array>
#include <set>

namespace affresco::format {

namespace {

using expect::fail;

/// The parts of a record the "installed" digest covers: what the runtime merges into the app.
constexpr std::array<std::string_view, 3> digestedParts = {"requires", "provides", "needs"};

/// The keys where a record says where it came from: every upgrade changes them, so they are never an edit dropped.
constexpr std::array<std::string_view, 3> originKeys = {"release", "digest", "installed"};

/// @brief Tells whether two records hold the same parts.
/// @param record One.
/// @param other The other.
/// @return true if each digested part is the same on both.
bool sameParts(const json& record, const json& other) {
	return std::ranges::all_of(digestedParts, [&](std::string_view part) {
		const std::string key(part);
		return record.value(key, json()) == other.value(key, json());
	});
}

/// @brief Tells whether a record and a release's record both name the same publisher.
/// @param mine The installed record.
/// @param theirs The release's.
/// @return true if both are keys and the same.
bool samePublisher(const json& mine, const json& theirs) {
	const json& installed = mine.value("publisher", json());
	const json& offered = theirs.value("publisher", json());
	const auto installedKey =
	    installed.is_string() ? parsePublicKey(installed.get_ref<const std::string&>()) : std::nullopt;
	const auto offeredKey = offered.is_string() ? parsePublicKey(offered.get_ref<const std::string&>()) : std::nullopt;
	return installedKey && offeredKey && sameKey(*installedKey, *offeredKey);
}

/// @brief The record the installed release installed, when a catalogue still lists it with the record's digest.
/// @param features The catalogues.
/// @param name The feature.
/// @param mine The record now.
/// @return It, with mine's "enabled", "order" and "installed" (so the merge sees no change there); or null.
json baseRecord(const Features& features, std::string_view name, const json& mine) {
	const json& digest = mine.value("digest", json());
	if(!digest.is_string()) return nullptr;
	json base;
	try {
		base = features.releaseRecord(name, mine.value("release", ""), mine.value("order", 0));
	} catch(const DocumentError&) {
		return nullptr; // no catalogue lists it any more, or its manifest is refused: the base is gone
	}
	if(!base.value("digest", json()).is_string() ||
	   !sameDigest(base["digest"].get_ref<const std::string&>(), digest.get_ref<const std::string&>()))
		return nullptr;
	base["enabled"] = mine.value("enabled", true);
	if(mine.contains("installed")) base["installed"] = mine["installed"];
	else base.erase("installed");
	return base;
}

} // namespace

const json* installedRecord(const json& document, std::string_view name) {
	const auto features = document.find("features");
	if(features == document.end() || !features->is_object()) return nullptr;
	const auto record = features->find(name);
	return record == features->end() ? nullptr : &*record;
}

std::string installedDigest(const json& record) {
	json parts = json::object();
	for(const std::string_view part : digestedParts) {
		const std::string key(part);
		parts[key] = record.value(key, json());
	}
	return jsonDigest(parts);
}

bool edited(const json& record, const json* base) {
	if(const auto installed = record.find("installed"); installed != record.end() && installed->is_string())
		return installed->get_ref<const std::string&>() != installedDigest(record);
	return base == nullptr || !sameParts(record, *base);
}

UpgradeParts upgradeParts(const Features& features, const json& document, std::string_view name) {
	const json* record = installedRecord(document, name);
	if(!record) fail(std::string(name), "isn't installed");
	UpgradeParts parts;
	parts.mine = *record;
	parts.from = parts.mine.value("release", "");
	// Told from the index, before any manifest is fetched
	if(const auto current = parseRelease(parts.from);
	   current && checkedRelease(features.newestRelease(name)) <= *current)
		fail(std::string(name), "is up to date (" + parts.from + ")");
	parts.theirs = features.releaseRecord(name, "", parts.mine.value("order", 0));
	parts.to = parts.theirs.value("release", "");
	// An upgrade never changes who the feature comes from: that takes a removal and a new install, both reviewed
	if(!samePublisher(parts.mine, parts.theirs))
		fail(std::string(name), "its new release comes from another publisher (remove it and install that one)");
	parts.theirs["enabled"] = parts.mine.value("enabled", true);
	parts.base = baseRecord(features, name, parts.mine);
	parts.baseKnown = !parts.base.is_null();
	return parts;
}

std::vector<std::string> differingPaths(const json& mine, const json& theirs) {
	std::vector<std::string> paths;
	std::set<std::string, std::less<>> seen;
	for(const json& operation : json::diff(mine, theirs)) {
		const std::string& path = operation["path"].get_ref<const std::string&>();
		const std::string_view top = path.empty() ? "" : std::string_view(path).substr(1, path.find('/', 1) - 1);
		if(std::ranges::find(originKeys, top) != originKeys.end()) continue;
		if(seen.insert(path).second) paths.push_back(path);
	}
	return paths;
}

json featureStatus(const Features& features, const json& document, std::string_view name) {
	const json* record = installedRecord(document, name);
	json status = {{"installed", record != nullptr},
	               {"release", record ? record->value("release", "") : ""},
	               {"newest", features.newestRelease(name)},
	               {"edited", false},
	               {"enabled", record != nullptr && record->value("enabled", false)}};
	if(record) {
		// Only a record installed before records kept "installed" needs its release fetched to tell
		const json base = record->contains("installed") ? json() : baseRecord(features, name, *record);
		status["edited"] = edited(*record, base.is_null() ? nullptr : &base);
	}
	return status;
}

} // namespace affresco::format
