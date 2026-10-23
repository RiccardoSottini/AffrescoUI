/// @file
/// @brief Fuzzes the parsing behind signatures (format/src/features/FeatureSignatures.hpp): base64url, keys, digests, key
/// files, canonical JSON and verifying a document.
///
/// Fixed-length parsing gives the length asked for or nothing, and reads back as the text it came from; canonical JSON
/// is a fixed point (its own canonical form); verifying gives the key the document names or a DocumentError.

#include "Fuzz.hpp"

#include "FeatureSignatures.hpp"

#include <cstddef>
#include <cstdint>

using affresco::json;

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	for(const std::size_t length : {std::size_t{4}, std::size_t{32}, std::size_t{64}})
		if(const auto bytes = affresco::format::fromBase64url(text, length)) {
			if(bytes->size() != length) affresco::fuzz::broken("base64url gives the length asked for");
			if(affresco::format::toBase64url(*bytes) != text) affresco::fuzz::broken("base64url is one text per value");
		}
	if(const auto key = affresco::format::parsePublicKey(text))
		if(std::string(affresco::format::publisherPrefix) + affresco::format::toBase64url(*key) != text)
			affresco::fuzz::broken("a key is one text");
	if(affresco::format::sameDigest(text, text) != (text.starts_with("sha512:") && text.size() == 7 + 86 &&
	                                               affresco::format::fromBase64url(text.substr(7), 64)))
		affresco::fuzz::broken("a digest is sha512: and 64 bytes");
	try {
		static_cast<void>(affresco::format::SigningKey::fromText(text));
	} catch(const affresco::DocumentError&) {
	}
	try {
		const json value = affresco::parseUntrusted(text);
		const std::string canonical = affresco::format::canonicalJson(value);
		if(affresco::format::canonicalJson(json::parse(canonical)) != canonical)
			affresco::fuzz::broken("canonical JSON is its own canonical form");
		static_cast<void>(affresco::format::verifySignature(value, "fuzz"));
	} catch(const json::parse_error&) {
	} catch(const affresco::DocumentError&) {
	}
	return 0;
}
