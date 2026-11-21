/// @file
/// @brief Unit tests for the signatures of features and catalogues (RFC 0004, D12): base64url, canonical JSON (RFC
/// 8785), Ed25519 with Monocypher, and SHA-512 digests. Every key is made when the tests run; none is kept.

#include "FeatureSignatures.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

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
	CHECK_FALSE(affresco::format::fromBase64url(text, 3));       // a fixed length
	CHECK_FALSE(affresco::format::fromBase64url("-_8AEB", 4));   // stray bits
	CHECK_FALSE(affresco::format::fromBase64url("+/8AEA", 4));   // base64's alphabet, not base64url's
	CHECK_FALSE(affresco::format::fromBase64url("-_8AEA==", 4)); // no padding
}

TEST_CASE("signatures: canonical JSON is RFC 8785's") {
	// Members sorted by UTF-16 code units: the emoji (a surrogate pair, D83D) before U+FB33
	const json keys = json::parse(R"({"\u20ac": 1, "\r": 2, "\ufb33": 3, "1": 4, "\ud83d\ude00": 5, "\u0080": 6,
		"\u00f6": 7})");
	CHECK(affresco::format::canonicalJson(keys) ==
	      "{\"\\r\":2,\"1\":4,\"\u0080\":6,\"\u00f6\":7,\"\u20ac\":1,\"\U0001F600\":5,\"\uFB33\":3}");
	// Numbers as ECMAScript writes them
	const json numbers = json::parse("[333333333.33333329, 1E30, 4.50, 2e-3, 0.000001, 1e-7, -0, 100, 1e21]");
	CHECK(affresco::format::canonicalJson(numbers) == "[333333333.3333333,1e+30,4.5,0.002,0.000001,1e-7,0,100,1e+21]");
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

namespace {

/// @brief The encodings of Ed25519's points of small order, with y below the field's prime (libsodium's list, less
/// its non-canonical entries): each is taken with x's sign bit clear and set.
/// @return Their 32 bytes each.
std::vector<std::array<std::uint8_t, 32>> smallOrderPoints() {
	const std::array<std::uint8_t, 32> zero{}; // y = 0: order 4
	std::array<std::uint8_t, 32> one{};        // y = 1: the identity
	one[0] = 0x01;
	std::array<std::uint8_t, 32> minusOne{}; // y = p - 1: order 2
	minusOne.fill(0xff);
	minusOne[0] = 0xec;
	minusOne[31] = 0x7f;
	// the two y of the points of order 8
	const std::array<std::uint8_t, 32> eightA = {0x26, 0xe8, 0x95, 0x8f, 0xc2, 0xb2, 0x27, 0xb0, 0x45, 0xc3, 0xf4,
	                                             0x89, 0xf2, 0xef, 0x98, 0xf0, 0xd5, 0xdf, 0xac, 0x05, 0xd3, 0xc6,
	                                             0x33, 0x39, 0xb1, 0x38, 0x02, 0x88, 0x6d, 0x53, 0xfc, 0x05};
	const std::array<std::uint8_t, 32> eightB = {0xc7, 0x17, 0x6a, 0x70, 0x3d, 0x4d, 0xd8, 0x4f, 0xba, 0x3c, 0x0b,
	                                             0x76, 0x0d, 0x10, 0x67, 0x0f, 0x2a, 0x20, 0x53, 0xfa, 0x2c, 0x39,
	                                             0xcc, 0xc6, 0x4e, 0xc7, 0xfd, 0x77, 0x92, 0xac, 0x03, 0x7a};
	std::vector<std::array<std::uint8_t, 32>> points;
	for(std::array<std::uint8_t, 32> point : {zero, one, minusOne, eightA, eightB}) {
		points.push_back(point);
		point[31] |= 0x80U;
		points.push_back(point);
	}
	return points;
}

/// @brief A document signed as a given key and signature claim it, whatever they are.
/// @param key The public key's bytes.
/// @param signature The signature's bytes.
/// @return The document.
json claimed(std::span<const std::uint8_t> key, std::span<const std::uint8_t> signature) {
	return {{"name", "wordCount"},
	        {"signature",
	         {{"publisher", "ed25519:" + affresco::format::toBase64url(key)},
	          {"value", affresco::format::toBase64url(signature)}}}};
}

} // namespace

TEST_CASE("signatures: a small-order key or R, and a non-canonical encoding, are refused before the check") {
	using affresco::format::isWeakPoint;
	const std::array<std::uint8_t, 64> zeros{};
	// The all-zero key with the all-zero signature verifies over any content with a plain check: refused here
	CHECK(refusal(claimed(std::span(zeros).first<32>(), zeros)).find("weak point") != std::string::npos);

	for(const std::array<std::uint8_t, 32>& point : smallOrderPoints()) {
		CAPTURE(affresco::format::toBase64url(point));
		CHECK(isWeakPoint(point));
		CHECK(refusal(claimed(point, zeros)).find("weak point") != std::string::npos);
		// The same point as a valid key's signature's R
		const SigningKey key = SigningKey::generate();
		json document = key.sign({{"name", "wordCount"}});
		const std::optional<std::vector<std::uint8_t>> decoded =
		    affresco::format::fromBase64url(document["signature"]["value"].get<std::string>(), 64);
		REQUIRE(decoded.has_value());
		std::vector<std::uint8_t> value = decoded.value_or(std::vector<std::uint8_t>{});
		std::ranges::copy(point, value.begin());
		document["signature"]["value"] = affresco::format::toBase64url(value);
		CHECK(refusal(document).find("weak or non-canonical") != std::string::npos);
	}

	// y at or above the prime: p and p + 1 (other names for y = 0 and 1), and 2^255 - 1
	for(const std::uint8_t low : {std::uint8_t{0xed}, std::uint8_t{0xee}, std::uint8_t{0xff}}) {
		std::array<std::uint8_t, 32> point{};
		point.fill(0xff);
		point[0] = low;
		point[31] = 0x7f;
		CHECK(isWeakPoint(point));
	}

	// S at the group's order is refused, though S - L would be the same signature
	const SigningKey key = SigningKey::generate();
	json document = key.sign({{"name", "wordCount"}});
	const std::optional<std::vector<std::uint8_t>> decoded =
	    affresco::format::fromBase64url(document["signature"]["value"].get<std::string>(), 64);
	REQUIRE(decoded.has_value());
	std::vector<std::uint8_t> value = decoded.value_or(std::vector<std::uint8_t>{});
	const std::array<std::uint8_t, 32> order = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
	                                            0xa2, 0xde, 0xf9, 0xde, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
	std::ranges::copy(order, value.begin() + 32);
	json high = document;
	high["signature"]["value"] = affresco::format::toBase64url(value);
	CHECK(refusal(high).find("weak or non-canonical") != std::string::npos);

	// Every key that signs is still taken, and its signatures verify
	for(int i = 0; i < 32; ++i) {
		const SigningKey fresh = SigningKey::generate();
		CHECK(refusal(fresh.sign({{"name", "wordCount"}, {"i", i}})).empty());
	}
}
