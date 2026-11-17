/// @file
/// @brief Fuzzes readCatalog (format/src/features/FeatureCatalog.hpp): a catalogue's index, of features and packages,
/// is untrusted input (RFC 0004).
///
/// The input is signed with a key made at start-up and trusted, so it reaches the index's checks past the signature.
/// It gives an index or a DocumentError; an index's releases, a feature's or a package's, are newest first, its URLs
/// and keys are well formed, and no package's id is a feature's name.

#include "Fuzz.hpp"

#include "FeatureCatalog.hpp"
#include "FeatureManifest.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

using affresco::json;

namespace {

/// @brief Checks a feature's or a package's releases: at least one, newest first, each with a URL that stays in the
/// folder and a key that is a key, or none (a release may be unsigned: the index's signature covers its digest).
/// @param releases The releases.
template <typename Release> void checkReleases(const std::vector<Release>& releases) {
	if(releases.empty()) affresco::fuzz::broken("an entry lists a release");
	for(std::size_t i = 1; i < releases.size(); ++i) {
		const auto newer = affresco::format::parseRelease(releases[i - 1].release);
		const auto older = affresco::format::parseRelease(releases[i].release);
		if(!newer || !older || !(*older < *newer)) affresco::fuzz::broken("releases are newest first, each once");
	}
	for(const Release& release : releases)
		if((!release.publisher.empty() && !affresco::format::parsePublicKey(release.publisher)) ||
		   release.url.empty() || release.url.front() == '/' || ("/" + release.url + "/").contains("/../"))
			affresco::fuzz::broken("a release's key is a key and its URL stays in the folder");
}

} // namespace

/// @brief libFuzzer's entry point: signs the input if it is an object, and reads it as an index.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	static const affresco::format::SigningKey key = affresco::format::SigningKey::generate();
	static const std::vector<affresco::format::PublicKey> trusted = {*affresco::format::parsePublicKey(key.publisher())};
	if(size > affresco::format::maxCatalogBytes) return 0;
	std::string text(affresco::fuzz::text(data, size));
	try {
		const json value = affresco::parseUntrusted(text);
		if(value.is_object()) text = key.sign(value).dump();
	} catch(const json::parse_error&) {
	} catch(const affresco::DocumentError&) {
	}
	try {
		const affresco::format::CatalogIndex index = affresco::format::readCatalog(text, "/catalog", trusted);
		for(const auto& [name, feature] : index.features) checkReleases(feature.releases);
		for(const auto& [id, package] : index.packages) {
			checkReleases(package.releases);
			if(index.features.contains(id)) affresco::fuzz::broken("a package's id is no feature's name");
			for(const affresco::format::CatalogPackageRelease& release : package.releases)
				if(release.baseApp.empty()) affresco::fuzz::broken("a package's release names its base app");
		}
	} catch(const json::parse_error&) {
	} catch(const affresco::DocumentError&) {
	}
	return 0;
}
