/// @file
/// @brief Catalogue v0: reading a signed index and fetching a release's manifest from its folder, with every check
/// RFC 0004 asks of untrusted packages.

#include "FeatureCatalog.hpp"
#include "NodeKeys.hpp"

#include "FeatureManifest.hpp"
#include "affresco/format/Migrate.hpp"

#include <algorithm>
#include <fstream>
#include <ranges>

namespace affresco::format {

namespace {

using expect::fail;

/// The keys an index may have.
constexpr auto indexKeys = std::to_array<std::string_view>(
    {"format", "version", "name", "updated", "publishers", "features", "signature"});

/// The keys of a feature in an index.
constexpr auto featureKeys = std::to_array<std::string_view>({"title", "description", "releases"});

/// The keys of a release in an index.
constexpr auto releaseKeys =
    std::to_array<std::string_view>({"release", "url", "digest", "publisher", "requires", "size"});

/// The most releases one feature may list: older ones belong in another index.
constexpr std::size_t maxReleases = 100;

/// The longest text an index may hold in a name, a title, a URL or a description.
constexpr std::size_t maxText = 1024;

/// @brief Refuses a key a list doesn't have.
/// @param object An object.
/// @param keys The keys it may have.
/// @param place Its place.
template <std::size_t N>
void onlyKeys(const json& object, const std::array<std::string_view, N>& keys, const std::string& place) {
	for(const auto& [key, value] : object.items())
		if(std::ranges::find(keys, key) == keys.end()) fail(concatenate(place, ".", key), "unknown key");
}

/// @brief Reads a member that must be a short string.
/// @param object The object.
/// @param key The member.
/// @param place The object's place.
/// @param required Whether it must be there.
/// @return It, or "" if it is optional and missing.
std::string shortText(const json& object, const char* key, const std::string& place, bool required = true) {
	const auto found = object.find(key);
	if(found == object.end()) {
		if(required) fail(place, std::string("missing \"") + key + "\"");
		return {};
	}
	std::string value = expect::string(*found, place + "." + key);
	if(value.size() > maxText) fail(place + "." + key, "longer than " + std::to_string(maxText) + " characters");
	return value;
}

/// @brief Tells whether a catalogue's name is one: lower-case letters, digits and '-', at most 64.
/// @param name The name.
/// @return true if it is.
bool validCatalogName(std::string_view name) noexcept {
	return !name.empty() && name.size() <= 64 && std::ranges::all_of(name, [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
	});
}

/// @brief Tells whether a URL stays in the catalogue's folder: relative, forward slashes, no "." or ".." part, no
/// scheme, at most maxText characters. Remote catalogues are a later version's.
/// @param url The URL.
/// @return true if it does.
bool localUrl(std::string_view url) noexcept {
	if(url.empty() || url.size() > maxText || url.front() == '/') return false;
	if(url.find_first_of(":\\%?#") != std::string_view::npos) return false;
	return std::ranges::none_of(std::views::split(url, '/'), [](const auto& range) {
		const std::string_view part(range.begin(), range.end());
		return part.empty() || part == "." || part == "..";
	});
}

/// @brief Reads one release of an index.
/// @param value The release.
/// @param place Its place.
/// @return The release.
CatalogRelease readRelease(const json& value, const std::string& place) {
	expect::object(value, place);
	onlyKeys(value, releaseKeys, place);
	CatalogRelease release;
	release.release = shortText(value, "release", place);
	if(!parseRelease(release.release)) fail(place + ".release", "expected a release such as \"1.2.0\"");
	release.url = shortText(value, "url", place);
	if(!localUrl(release.url)) fail(place + ".url", "expected a path inside the catalogue's folder");
	release.digest = shortText(value, "digest", place);
	if(!sameDigest(release.digest, release.digest)) fail(place + ".digest", "expected \"sha512:<base64url>\"");
	release.publisher = shortText(value, "publisher", place);
	if(!parsePublicKey(release.publisher)) fail(place + ".publisher", "expected \"ed25519:<public key>\"");
	const auto size = value.find("size");
	if(size == value.end() || !size->is_number_unsigned() || size->get<std::size_t>() > maxManifestBytes)
		fail(place + ".size", "expected the manifest's size in bytes, at most " + std::to_string(maxManifestBytes));
	release.size = size->get<std::size_t>();
	if(const auto needs = value.find("requires"); needs != value.end()) release.needs = expect::object(*needs, place + ".requires");
	return release;
}

/// @brief Reads one feature of an index.
/// @param name Its name.
/// @param value The feature.
/// @param place Its place.
/// @return The feature, its releases newest first.
CatalogFeature readFeature(const std::string& name, const json& value, const std::string& place) {
	if(!validFeatureName(name)) fail(place, "expected a feature's name (one camelCase word)");
	expect::object(value, place);
	onlyKeys(value, featureKeys, place);
	CatalogFeature feature;
	feature.title = shortText(value, "title", place, false);
	feature.description = shortText(value, "description", place, false);
	const auto releases = value.find("releases");
	if(releases == value.end() || !releases->is_array() || releases->empty() || releases->size() > maxReleases)
		fail(place + ".releases", "expected from 1 to " + std::to_string(maxReleases) + " releases");
	for(std::size_t i = 0; i < releases->size(); ++i)
		feature.releases.push_back(readRelease((*releases)[i], place + ".releases[" + std::to_string(i) + "]"));
	std::ranges::sort(feature.releases, [](const CatalogRelease& a, const CatalogRelease& b) {
		return checkedRelease(b.release) < checkedRelease(a.release);
	});
	for(std::size_t i = 1; i < feature.releases.size(); ++i)
		if(feature.releases[i].release == feature.releases[i - 1].release)
			fail(place + ".releases", "lists " + feature.releases[i].release + " twice");
	return feature;
}

} // namespace

std::string readBounded(const std::filesystem::path& file, std::size_t limit, const std::string& place) {
	std::ifstream in(file, std::ios::binary);
	if(!in) fail(place, "can't be read");
	std::string bytes(limit + 1, '\0');
	in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	const auto count = static_cast<std::size_t>(in.gcount());
	if(count > limit) fail(place, "is larger than " + std::to_string(limit) + " bytes");
	bytes.resize(count);
	return bytes;
}

bool isTrusted(std::string_view publisher, const std::vector<PublicKey>& trusted) {
	const auto key = parsePublicKey(publisher);
	if(!key) return false;
	bool found = false;
	for(const PublicKey& each : trusted) found = sameKey(*key, each) || found; // no early exit
	return found;
}

CatalogIndex readCatalog(std::string_view text, const std::filesystem::path& folder,
                         const std::vector<PublicKey>& trusted) {
	if(text.size() > maxCatalogBytes) fail("catalog", "is larger than " + std::to_string(maxCatalogBytes) + " bytes");
	const json index = parseUntrusted(text);
	expect::object(index, "catalog");
	// The signature first: nothing an untrusted index says is worth reading
	const std::string signer = verifySignature(index, "catalog");
	if(!isTrusted(signer, trusted)) fail("catalog", "it is signed by " + signer + ", which isn't a trusted publisher");
	onlyKeys(index, indexKeys, "catalog");
	if(index.value("format", json()) != catalogFormat) fail("catalog.format", "expected \"affrescoui-catalog\"");
	if(const json version = index.value("version", json()); version != 1 && version != formatVersion)
		fail("catalog.version", "expected 1 or 2");
	CatalogIndex catalog;
	catalog.name = shortText(index, "name", "catalog");
	if(!validCatalogName(catalog.name)) fail("catalog.name", "expected lower-case letters, digits and '-'");
	catalog.folder = folder;
	const json publishers = index.value("publishers", json::object()); // a copy: value() returns a temporary
	expect::object(publishers, "catalog.publishers");
	for(const auto& [key, about] : publishers.items()) {
		if(!parsePublicKey(key)) fail("catalog.publishers." + key, "expected \"ed25519:<public key>\"");
		catalog.publishers[key] = about.is_object() ? shortText(about, "name", "catalog.publishers." + key, false) : "";
	}
	const auto features = index.find("features");
	if(features == index.end()) fail("catalog", "missing \"features\"");
	expect::object(*features, "catalog.features");
	if(features->size() > maxCatalogFeatures)
		fail("catalog.features", "lists more than " + std::to_string(maxCatalogFeatures) + " features");
	for(const auto& [name, feature] : features->items())
		catalog.features[name] = readFeature(name, feature, "catalog.features." + name);
	return catalog;
}

json fetchManifest(const CatalogIndex& index, const std::string& name, const CatalogRelease& release,
                   const std::vector<PublicKey>& trusted) {
	const std::string place = name + " " + release.release;
	if(!localUrl(release.url)) fail(place, "its URL leaves the catalogue's folder");
	// Links inside the folder may still lead out of it
	std::error_code error;
	const std::filesystem::path file = std::filesystem::weakly_canonical(index.folder / release.url, error);
	const auto [outside, rest] = std::ranges::mismatch(index.folder, file);
	if(error || outside != index.folder.end() || rest == file.end())
		fail(place, "its URL leaves the catalogue's folder");
	const std::string bytes = readBounded(file, maxManifestBytes, place);
	if(bytes.size() != release.size) fail(place, "its size isn't the catalogue's (the package was changed)");
	if(!sameDigest(digestOf(bytes), release.digest))
		fail(place, "its digest isn't the catalogue's (the package was changed)");
	json manifest = parseUntrusted(bytes);
	const std::string signer = verifySignature(manifest, place);
	const auto key = parsePublicKey(signer);
	const auto expected = parsePublicKey(release.publisher);
	if(!key || !expected || !sameKey(*key, *expected))
		fail(place, "it is signed by " + signer + ", not by the publisher the catalogue names");
	if(!isTrusted(signer, trusted)) fail(place, "it is signed by " + signer + ", which isn't a trusted publisher");
	static_cast<void>(migrate(manifest)); // after the signature, which covers the manifest as it was published
	checkManifest(manifest, place);
	if(manifest["name"] != name || manifest["release"] != release.release)
		fail(place, "the manifest is " + manifest["name"].get<std::string>() + " " +
		                manifest["release"].get<std::string>() + ", not what the catalogue lists");
	return manifest;
}

} // namespace affresco::format
