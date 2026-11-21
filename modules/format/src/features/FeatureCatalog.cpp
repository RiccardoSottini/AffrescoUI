/// @file
/// @brief Catalogue v0: reading a signed index and fetching a release's manifest or package from its folder, with
/// every check RFC 0004 asks of untrusted packages.

#include "FeatureCatalog.hpp"
#include "NodeKeys.hpp"

#include "FeatureManifest.hpp"
#include "affresco/format/Migrate.hpp"
#include "affresco/format/Package.hpp"
#include "affresco/format/PackageFiles.hpp"

#include <algorithm>
#include <fstream>
#include <ranges>

namespace affresco::format {

namespace {

using expect::fail;

/// The keys an index may have.
constexpr auto indexKeys = std::to_array<std::string_view>(
    {"format", "version", "name", "updated", "publishers", "features", "packages", "signature"});

/// The keys of a feature in an index.
constexpr auto featureKeys = std::to_array<std::string_view>({"title", "description", "releases"});

/// The keys of a release in an index.
constexpr auto releaseKeys =
    std::to_array<std::string_view>({"release", "url", "digest", "publisher", "requires", "size"});

/// The keys of a package in an index.
constexpr auto packageKeys = std::to_array<std::string_view>({"title", "description", "base", "releases"});

/// The keys of a package's release in an index.
constexpr auto packageReleaseKeys =
    std::to_array<std::string_view>({"release", "url", "digest", "publisher", "size", "base"});

/// The keys of a package release's base.
constexpr auto packageBaseKeys = std::to_array<std::string_view>({"app", "release"});

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
	release.publisher = shortText(value, "publisher", place, false);
	if(value.contains("publisher") && !parsePublicKey(release.publisher))
		fail(place + ".publisher", "expected \"ed25519:<public key>\"");
	const auto size = value.find("size");
	if(size == value.end() || !size->is_number_unsigned() || size->get<std::size_t>() > maxManifestBytes)
		fail(place + ".size", "expected the manifest's size in bytes, at most " + std::to_string(maxManifestBytes));
	release.size = size->get<std::size_t>();
	if(const auto needs = value.find("requires"); needs != value.end())
		release.needs = expect::object(*needs, place + ".requires");
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

/// @brief Reads the size of a file an index lists.
/// @param value The release.
/// @param place Its place.
/// @param most The most bytes such a file may have.
/// @return The size.
std::size_t listedSize(const json& value, const std::string& place, std::size_t most) {
	const auto size = value.find("size");
	if(size == value.end() || !size->is_number_unsigned() || size->get<std::size_t>() > most)
		fail(place + ".size", "expected the file's size in bytes, at most " + std::to_string(most));
	return size->get<std::size_t>();
}

/// @brief Reads one release of a package in an index.
/// @param value The release.
/// @param base The id of the app its package is a version of.
/// @param place Its place.
/// @return The release.
CatalogPackageRelease readPackageRelease(const json& value, const std::string& base, const std::string& place) {
	expect::object(value, place);
	onlyKeys(value, packageReleaseKeys, place);
	CatalogPackageRelease release;
	release.release = shortText(value, "release", place);
	if(!parseRelease(release.release)) fail(place + ".release", "expected a release such as \"1.2.0\"");
	release.url = shortText(value, "url", place);
	if(!localUrl(release.url)) fail(place + ".url", "expected a path inside the catalogue's folder");
	release.digest = shortText(value, "digest", place);
	if(!sameDigest(release.digest, release.digest)) fail(place + ".digest", "expected \"sha512:<base64url>\"");
	release.publisher = shortText(value, "publisher", place, false);
	if(value.contains("publisher") && !parsePublicKey(release.publisher))
		fail(place + ".publisher", "expected \"ed25519:<public key>\"");
	release.size = listedSize(value, place, maxPackageBytes);
	const auto made = value.find("base");
	if(made == value.end()) fail(place, "missing \"base\"");
	expect::object(*made, place + ".base");
	onlyKeys(*made, packageBaseKeys, place + ".base");
	release.baseApp = shortText(*made, "app", place + ".base");
	if(release.baseApp != base) fail(place + ".base.app", "expected \"" + base + "\", the package's base");
	release.baseRelease = shortText(*made, "release", place + ".base", false);
	if(!release.baseRelease.empty() && !parseRelease(release.baseRelease))
		fail(place + ".base.release", "expected a release such as \"1.2.0\"");
	return release;
}

/// @brief Reads one package of an index.
/// @param id Its id.
/// @param value The package.
/// @param place Its place.
/// @return The package, its releases newest first.
CatalogPackage readCatalogPackage(const std::string& id, const json& value, const std::string& place) {
	if(!validCatalogName(id)) fail(place, "expected a package's id (lower-case letters, digits and '-')");
	expect::object(value, place);
	onlyKeys(value, packageKeys, place);
	CatalogPackage package;
	package.title = shortText(value, "title", place, false);
	package.description = shortText(value, "description", place, false);
	package.base = shortText(value, "base", place);
	if(!validCatalogName(package.base))
		fail(place + ".base", "expected an app's id (lower-case letters, digits and '-')");
	const auto releases = value.find("releases");
	if(releases == value.end() || !releases->is_array() || releases->empty() || releases->size() > maxReleases)
		fail(place + ".releases", "expected from 1 to " + std::to_string(maxReleases) + " releases");
	for(std::size_t i = 0; i < releases->size(); ++i)
		package.releases.push_back(
		    readPackageRelease((*releases)[i], package.base, place + ".releases[" + std::to_string(i) + "]"));
	std::ranges::sort(package.releases, [](const CatalogPackageRelease& a, const CatalogPackageRelease& b) {
		return checkedRelease(b.release) < checkedRelease(a.release);
	});
	for(std::size_t i = 1; i < package.releases.size(); ++i)
		if(package.releases[i].release == package.releases[i - 1].release)
			fail(place + ".releases", "lists " + package.releases[i].release + " twice");
	return package;
}

/// @brief Reads what an index lists, once its signature is settled.
/// @param index The index, an object.
/// @param folder Its folder, canonical.
/// @param signer The key that signed it, or "".
/// @return The index.
CatalogIndex readIndex(const json& index, const std::filesystem::path& folder, std::string signer) {
	onlyKeys(index, indexKeys, "catalog");
	if(index.value("format", json()) != json(catalogFormat)) fail("catalog.format", "expected \"affrescoui-catalog\"");
	if(const json version = index.value("version", json()); version != 1 && version != formatVersion)
		fail("catalog.version", "expected 1 or 2");
	CatalogIndex catalog;
	catalog.signer = std::move(signer);
	catalog.name = shortText(index, "name", "catalog");
	if(!validCatalogName(catalog.name)) fail("catalog.name", "expected lower-case letters, digits and '-'");
	catalog.updated = shortText(index, "updated", "catalog", false);
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
	const auto packages = index.find("packages");
	if(packages == index.end()) return catalog; // an index of features only, as before
	expect::object(*packages, "catalog.packages");
	if(packages->size() > maxCatalogFeatures)
		fail("catalog.packages", "lists more than " + std::to_string(maxCatalogFeatures) + " packages");
	for(const auto& [id, package] : packages->items()) {
		// one id, one entry: a person picks an entry by its id, whichever kind it is
		if(catalog.features.contains(id)) fail("catalog.packages." + id, "is a feature's name too");
		catalog.packages[id] = readCatalogPackage(id, package, "catalog.packages." + id);
	}
	return catalog;
}

/// @brief The file a URL of an index names, once it is sure to stay in the index's folder, links included.
/// @param index The index.
/// @param url The URL.
/// @param place The entry's place, for errors.
/// @return The file.
std::filesystem::path listedFile(const CatalogIndex& index, std::string_view url, const std::string& place) {
	if(!localUrl(url)) fail(place, "its URL leaves the catalogue's folder");
	// Links inside the folder may still lead out of it
	std::error_code error;
	const std::filesystem::path file = std::filesystem::weakly_canonical(index.folder / url, error);
	const auto [outside, rest] = std::ranges::mismatch(index.folder, file);
	if(error || outside != index.folder.end() || rest == file.end())
		fail(place, "its URL leaves the catalogue's folder");
	return file;
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
	std::string signer = verifySignature(index, "catalog");
	if(!isTrusted(signer, trusted)) fail("catalog", "it is signed by " + signer + ", which isn't a trusted publisher");
	return readIndex(index, folder, std::move(signer));
}

CatalogIndex readUnsignedCatalog(std::string_view text, const std::filesystem::path& folder) {
	if(text.size() > maxCatalogBytes) fail("catalog", "is larger than " + std::to_string(maxCatalogBytes) + " bytes");
	const json index = parseUntrusted(text);
	expect::object(index, "catalog");
	std::string signer = index.contains("signature") ? verifySignature(index, "catalog") : std::string();
	return readIndex(index, folder, std::move(signer));
}

std::string readListedFile(const CatalogIndex& index, std::string_view url, std::size_t size, std::string_view digest,
                           std::size_t most, const std::string& place) {
	std::string bytes = readBounded(listedFile(index, url, place), most, place);
	if(bytes.size() != size) fail(place, "its size isn't the catalogue's (the package was changed)");
	if(!sameDigest(digestOf(bytes), digest)) fail(place, "its digest isn't the catalogue's (the package was changed)");
	return bytes;
}

json fetchManifest(const CatalogIndex& index, const std::string& name, const CatalogRelease& release,
                   const std::vector<PublicKey>& trusted) {
	const std::string place = name + " " + release.release;
	const std::string bytes = readListedFile(index, release.url, release.size, release.digest, maxManifestBytes, place);
	// A feature's code joins the app's own: from a catalogue, only a trusted publisher's
	if(release.publisher.empty()) fail(place, "it is unsigned: a feature is installed only from a trusted publisher");
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

std::string fetchPackage(const CatalogIndex& index, const std::string& id, const CatalogPackageRelease& release) {
	const std::string place = id + " " + release.release;
	std::string bytes = readListedFile(index, release.url, release.size, release.digest, maxPackageBytes, place);
	const Package package = readPackage(bytes); // its own checks, and its signature if it has one
	if(package.release != release.release || package.base.app != release.baseApp)
		fail(place, "the package is " + package.base.app + " " + package.release + ", not what the catalogue lists");
	if(release.baseRelease != package.base.release)
		fail(place, "it was made on " + package.base.app + " " + package.base.release +
		                ", not on the release the "
		                "catalogue lists");
	if(release.publisher.empty()) return bytes;
	const std::string signer = package.signature.is_object() ? package.signature.value("publisher", "") : "";
	const auto key = parsePublicKey(signer);
	const auto expected = parsePublicKey(release.publisher);
	if(!key || !expected || !sameKey(*key, *expected))
		fail(place, "it isn't signed by the publisher the catalogue names");
	return bytes;
}

} // namespace affresco::format
