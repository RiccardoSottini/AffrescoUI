/// @file
/// @brief Unit tests for the signatures of features and catalogues (RFC 0004, D12): base64url, canonical JSON (RFC
/// 8785), Ed25519 with Monocypher, and SHA-512 digests. Every key is made when the tests run; none is kept.

#include "FeatureSignatures.hpp"

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::format::SigningKey;

namespace {

/// @brief The reason verifying a document gives, or "" if it is valid.
/// @param document The document.
/// @return The reason.
std::string refusal(const json& document) {
	try {
		(void)affresco::format::verifySignature(document, "feature");
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
	return {};
}

} // namespace

TEST_CASE("signatures: base64url is strict") {
	const std::vector<std::uint8_t> bytes = {0xfb, 0xff, 0x00, 0x10};
	const std::string text = affresco::format::toBase64url(bytes);
	CHECK(text == "-_8AEA");
	CHECK(affresco::format::fromBase64url(text, 4) == bytes);
	CHECK_FALSE(affresco::format::fromBase64url(text, 3));        // a fixed length
	CHECK_FALSE(affresco::format::fromBase64url("-_8AEB", 4));    // stray bits
	CHECK_FALSE(affresco::format::fromBase64url("+/8AEA", 4));    // base64's alphabet, not base64url's
	CHECK_FALSE(affresco::format::fromBase64url("-_8AEA==", 4));  // no padding
}

TEST_CASE("signatures: canonical JSON is RFC 8785's") {
	// Members sorted by UTF-16 code units: the emoji (a surrogate pair, D83D) before U+FB33
	const json keys = json::parse(R"({"\u20ac": 1, "\r": 2, "\ufb33": 3, "1": 4, "\ud83d\ude00": 5, "\u0080": 6,
		"\u00f6": 7})");
	CHECK(affresco::format::canonicalJson(keys) ==
	      "{\"\\r\":2,\"1\":4,\"\u0080\":6,\"\u00f6\":7,\"\u20ac\":1,\"\U0001F600\":5,\"\uFB33\":3}");
	// Numbers as ECMAScript writes them
	const json numbers = json::parse("[333333333.33333329, 1E30, 4.50, 2e-3, 0.000001, 1e-7, -0, 100, 1e21]");
	CHECK(affresco::format::canonicalJson(numbers) ==
	      "[333333333.3333333,1e+30,4.5,0.002,0.000001,1e-7,0,100,1e+21]");
	// The fewest escapes
	CHECK(affresco::format::canonicalJson(json("\u20ac$\x0f\n/\"\\")) == "\"\u20ac$\\u000f\\n/\\\"\\\\\"");
	CHECK(affresco::format::canonicalJson(json::parse(R"({ "b": [true, null, {}], "a": "x" })")) ==
	      R"({"a":"x","b":[true,null,{}]})");
}

TEST_CASE("signatures: a signed document verifies, a changed one doesn't, whatever its layout") {
	const SigningKey key = SigningKey::generate();
	const json document = key.sign({{"name", "wordCount"}, {"release", "1.0.0"}, {"size", 3}});
	CHECK(document["signature"]["publisher"] == key.publisher());
	CHECK(affresco::format::verifySignature(document, "feature") == key.publisher());
	// Written another way, it is the same document
	CHECK(refusal(json::parse(document.dump(4))).empty());

	json changed = document;
	changed["release"] = "1.0.1";
	CHECK(refusal(changed).find("it was changed after signing") != std::string::npos);

	json unsigned_ = document;
	unsigned_.erase("signature");
	CHECK(refusal(unsigned_).find("isn't signed") != std::string::npos);

	// Another key's name on the signature doesn't make it that key's
	json renamed = document;
	renamed["signature"]["publisher"] = SigningKey::generate().publisher();
	CHECK(refusal(renamed).find("it was changed after signing") != std::string::npos);

	json malformed = document;
	malformed["signature"]["value"] = "AAAA";
	CHECK_FALSE(refusal(malformed).empty());
}

TEST_CASE("signatures: a key file reads back as the same key") {
	const SigningKey key = SigningKey::generate();
	const std::string text = key.toText();
	CHECK(text.starts_with("ed25519-secret:"));
	CHECK(SigningKey::fromText(text).publisher() == key.publisher());
	CHECK(SigningKey::fromText(text.substr(0, text.size() - 1)).publisher() == key.publisher()); // no newline
	CHECK_THROWS_AS((void)SigningKey::fromText(text + "x"), affresco::DocumentError);
	CHECK_THROWS_AS((void)SigningKey::fromText(key.publisher()), affresco::DocumentError);
	CHECK(SigningKey::generate().publisher() != key.publisher());
}

TEST_CASE("signatures: keys and digests are parsed at their length and compared whole") {
	const SigningKey key = SigningKey::generate();
	CHECK(affresco::format::parsePublicKey(key.publisher()));
	CHECK_FALSE(affresco::format::parsePublicKey(key.publisher() + "A"));
	CHECK_FALSE(affresco::format::parsePublicKey(key.publisher().substr(1)));
	const std::string digest = affresco::format::digestOf("{}");
	CHECK(digest.starts_with("sha512:"));
	CHECK(affresco::format::sameDigest(digest, affresco::format::digestOf("{}")));
	CHECK_FALSE(affresco::format::sameDigest(digest, affresco::format::digestOf("{ }")));
	CHECK_FALSE(affresco::format::sameDigest(digest, "sha512:"));
}
