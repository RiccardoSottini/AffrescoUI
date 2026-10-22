/// @file
/// @brief Fuzzes readCatalog (format/src/FeatureCatalog.hpp): a catalogue's index is untrusted input (RFC 0004).
///
/// The input is signed with a key made at start-up and trusted, so it reaches the index's checks past the signature.
/// It gives an index or a DocumentError; an index's releases are newest first, and its URLs and keys are well formed.

#include "Fuzz.hpp"

#include "FeatureCatalog.hpp"
#include "FeatureManifest.hpp"

#include <cstddef>
#include <cstdint>

using affresco::json;

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
		for(const auto& [name, feature] : index.features) {
			if(feature.releases.empty()) affresco::fuzz::broken("a feature lists a release");
			for(std::size_t i = 1; i < feature.releases.size(); ++i)
				if(!(*affresco::format::parseRelease(feature.releases[i].release) <
				     *affresco::format::parseRelease(feature.releases[i - 1].release)))
					affresco::fuzz::broken("releases are newest first, each once");
			for(const affresco::format::CatalogRelease& release : feature.releases)
				if(!affresco::format::parsePublicKey(release.publisher) || release.url.empty() ||
				   release.url.front() == '/' || release.url.find("..") != std::string::npos)
					affresco::fuzz::broken("a release's key is a key and its URL stays in the folder");
		}
	} catch(const json::parse_error&) {
	} catch(const affresco::DocumentError&) {
	}
	return 0;
}
