/// @file
/// @brief Signatures of feature packages and catalogues: base64url, canonical JSON (RFC 8785), Ed25519 and SHA-512
/// with Monocypher.

#include "FeatureSignatures.hpp"

#include "monocypher-ed25519.h"
#include "monocypher.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <random>
#include <system_error>

namespace affresco::format {

namespace {

/// The base64url alphabet.
constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/// @brief A base64url character's value.
/// @param c The character.
/// @return Its value (0 to 63), or -1 if it isn't one.
int sextet(char c) noexcept {
	if(c >= 'A' && c <= 'Z') return c - 'A';
	if(c >= 'a' && c <= 'z') return c - 'a' + 26;
	if(c >= '0' && c <= '9') return c - '0' + 52;
	if(c == '-') return 62;
	if(c == '_') return 63;
	return -1;
}

/// @brief Writes a string as RFC 8785 does: only `"`, `\` and the control characters are escaped, the short forms
/// where JSON has them, else `\u00xx` in lower case.
/// @param text The string (valid UTF-8: the parser checked it).
/// @param out Receives it, quoted.
void writeString(const std::string& text, std::string& out) {
	out += '"';
	for(const char c : text) {
		switch(c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if(static_cast<unsigned char>(c) < 0x20) {
				constexpr std::string_view hex = "0123456789abcdef";
				out += "\\u00";
				out += hex[static_cast<unsigned char>(c) >> 4];
				out += hex[static_cast<unsigned char>(c) & 0xF];
			} else {
				out += c;
			}
		}
	}
	out += '"';
}

/// @brief Writes a number as ECMAScript's Number.prototype.toString() does (RFC 8785, section 3.2.2.3): the shortest
/// digits that read back as the same double, in plain notation from 1e-6 to below 1e21, else with an exponent.
/// @param number The number.
/// @param out Receives it.
void writeNumber(double number, std::string& out) {
	if(number == 0) { // -0 too
		out += '0';
		return;
	}
	if(number < 0) {
		out += '-';
		number = -number;
	}
	char buffer[40];
	const auto [end, error] = std::to_chars(buffer, buffer + sizeof buffer, number, std::chars_format::scientific);
	if(error != std::errc()) throw DocumentError("a number can't be written");
	// "d.ddde+XX": the digits, then the exponent
	const std::string_view text(buffer, end);
	const std::size_t e = text.find('e');
	std::string digits(1, text[0]);
	if(e > 1) digits += text.substr(2, e - 2);
	int exponent = 0;
	const std::string_view power = text.substr(e + 1);
	std::from_chars(power.data() + (power[0] == '+' ? 1 : 0), power.data() + power.size(), exponent);
	const int k = static_cast<int>(digits.size());
	const int n = exponent + 1; // the value is 0.digits × 10^n
	if(k <= n && n <= 21) {
		out += digits;
		out.append(static_cast<std::size_t>(n - k), '0');
	} else if(0 < n && n <= 21) {
		out += digits.substr(0, static_cast<std::size_t>(n));
		out += '.';
		out += digits.substr(static_cast<std::size_t>(n));
	} else if(-6 < n && n <= 0) {
		out += "0.";
		out.append(static_cast<std::size_t>(-n), '0');
		out += digits;
	} else {
		out += digits[0];
		if(k > 1) out += '.' + digits.substr(1);
		out += n - 1 < 0 ? "e-" : "e+";
		out += std::to_string(std::abs(n - 1));
	}
}

/// @brief A string's UTF-16 code units, which RFC 8785 sorts names by (UTF-8's byte order differs for characters
/// beyond U+FFFF).
/// @param text Valid UTF-8.
/// @return Its code units.
std::u16string utf16(const std::string& text) {
	std::u16string units;
	for(std::size_t i = 0; i < text.size();) {
		const auto byte = static_cast<unsigned char>(text[i]);
		const std::size_t length = byte < 0x80 ? 1 : byte < 0xE0 ? 2 : byte < 0xF0 ? 3 : 4;
		char32_t point = length == 1 ? byte : length == 2 ? byte & 0x1Fu : length == 3 ? byte & 0x0Fu : byte & 0x07u;
		for(std::size_t j = 1; j < length && i + j < text.size(); ++j)
			point = (point << 6) | (static_cast<unsigned char>(text[i + j]) & 0x3Fu);
		if(point >= 0x10000) {
			point -= 0x10000;
			units += static_cast<char16_t>(0xD800 + (point >> 10));
			units += static_cast<char16_t>(0xDC00 + (point & 0x3FF));
		} else {
			units += static_cast<char16_t>(point);
		}
		i += length;
	}
	return units;
}

/// @brief Writes a value in canonical form.
/// @param value The value (its depth is bounded: it was parsed with parseUntrusted()).
/// @param out Receives it.
void writeCanonical(const json& value, std::string& out) {
	switch(value.type()) {
	case json::value_t::object: {
		std::vector<std::pair<std::u16string, const json::object_t::value_type*>> members;
		for(const auto& member : value.get_ref<const json::object_t&>()) members.emplace_back(utf16(member.first), &member);
		std::ranges::sort(members, {}, &decltype(members)::value_type::first);
		out += '{';
		for(std::size_t i = 0; i < members.size(); ++i) {
			if(i > 0) out += ',';
			writeString(members[i].second->first, out);
			out += ':';
			writeCanonical(members[i].second->second, out);
		}
		out += '}';
		return;
	}
	case json::value_t::array:
		out += '[';
		for(std::size_t i = 0; i < value.size(); ++i) {
			if(i > 0) out += ',';
			writeCanonical(value[i], out);
		}
		out += ']';
		return;
	case json::value_t::string: writeString(value.get_ref<const std::string&>(), out); return;
	case json::value_t::boolean: out += value.get<bool>() ? "true" : "false"; return;
	case json::value_t::number_integer:
	case json::value_t::number_unsigned:
	case json::value_t::number_float: writeNumber(value.get<double>(), out); return;
	default: out += "null"; return;
	}
}

/// @brief What a signature covers: the canonical form of a document without its "signature".
/// @param document The document, an object.
/// @return The bytes.
std::string signedBytes(json document) {
	document.erase("signature");
	return canonicalJson(document);
}

/// @brief A byte view of text, for Monocypher.
/// @param text The text.
/// @return Its bytes.
const std::uint8_t* bytesOf(std::string_view text) noexcept {
	return reinterpret_cast<const std::uint8_t*>(text.data()); // NOLINT(*-reinterpret-cast): char to byte
}

} // namespace

std::string toBase64url(std::span<const std::uint8_t> bytes) {
	std::string text;
	text.reserve((bytes.size() * 4 + 2) / 3);
	std::uint32_t buffer = 0;
	int bits = 0;
	for(const std::uint8_t byte : bytes) {
		buffer = (buffer << 8) | byte;
		bits += 8;
		while(bits >= 6) {
			bits -= 6;
			text += alphabet[(buffer >> bits) & 0x3F];
		}
	}
	if(bits > 0) text += alphabet[(buffer << (6 - bits)) & 0x3F];
	return text;
}

std::optional<std::vector<std::uint8_t>> fromBase64url(std::string_view text, std::size_t size) {
	if(text.size() != (size * 4 + 2) / 3) return std::nullopt; // the length is fixed: no padding, nothing else
	std::vector<std::uint8_t> bytes;
	bytes.reserve(size);
	std::uint32_t buffer = 0;
	int bits = 0;
	for(const char c : text) {
		const int value = sextet(c);
		if(value < 0) return std::nullopt;
		buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
		bits += 6;
		if(bits >= 8) {
			bits -= 8;
			bytes.push_back(static_cast<std::uint8_t>(buffer >> bits));
		}
	}
	if((buffer & ((1u << bits) - 1)) != 0) return std::nullopt; // stray bits: another text for the same bytes
	return bytes;
}

std::string canonicalJson(const json& value) {
	std::string out;
	writeCanonical(value, out);
	return out;
}

std::optional<PublicKey> parsePublicKey(std::string_view text) {
	if(!text.starts_with(publisherPrefix)) return std::nullopt;
	const auto bytes = fromBase64url(text.substr(publisherPrefix.size()), 32);
	if(!bytes) return std::nullopt;
	PublicKey key{};
	std::ranges::copy(*bytes, key.begin());
	return key;
}

bool sameKey(const PublicKey& a, const PublicKey& b) noexcept { return crypto_verify32(a.data(), b.data()) == 0; }

std::string digestOf(std::string_view bytes) {
	std::array<std::uint8_t, 64> hash{};
	crypto_sha512(hash.data(), bytesOf(bytes), bytes.size());
	return std::string(digestPrefix) + toBase64url(hash);
}

bool sameDigest(std::string_view a, std::string_view b) noexcept {
	if(!a.starts_with(digestPrefix) || !b.starts_with(digestPrefix)) return false;
	try {
		const auto first = fromBase64url(a.substr(digestPrefix.size()), 64);
		const auto second = fromBase64url(b.substr(digestPrefix.size()), 64);
		return first && second && crypto_verify64(first->data(), second->data()) == 0;
	} catch(...) {
		return false;
	}
}

SigningKey::SigningKey(const std::array<std::uint8_t, 32>& seed) noexcept : seed_(seed) {}

SigningKey SigningKey::generate() {
	// std::random_device is the system's source: arc4random on macOS and the BSDs, getrandom() or /dev/urandom on
	// Linux, rand_s() on Windows
	std::random_device source;
	std::array<std::uint8_t, 32> seed{};
	for(std::size_t i = 0; i < seed.size(); i += 4) {
		const std::uint32_t word = source();
		for(std::size_t j = 0; j < 4; ++j) seed[i + j] = static_cast<std::uint8_t>(word >> (8 * j));
	}
	SigningKey key(seed);
	crypto_wipe(seed.data(), seed.size());
	return key;
}

SigningKey SigningKey::fromText(std::string_view text) {
	while(!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.remove_suffix(1);
	if(!text.starts_with(secretKeyPrefix)) throw DocumentError("the key file doesn't start with \"ed25519-secret:\"");
	auto bytes = fromBase64url(text.substr(secretKeyPrefix.size()), 32);
	if(!bytes) throw DocumentError("the key file doesn't hold a 32-byte key");
	std::array<std::uint8_t, 32> seed{};
	std::ranges::copy(*bytes, seed.begin());
	crypto_wipe(bytes->data(), bytes->size());
	SigningKey key(seed);
	crypto_wipe(seed.data(), seed.size());
	return key;
}

void wipe(std::string& text) noexcept {
	crypto_wipe(text.data(), text.size());
	text.clear();
}

SigningKey::~SigningKey() { crypto_wipe(seed_.data(), seed_.size()); }

SigningKey::SigningKey(SigningKey&& other) noexcept : seed_(other.seed_) {
	crypto_wipe(other.seed_.data(), other.seed_.size());
}

std::string SigningKey::toText() const { return std::string(secretKeyPrefix) + toBase64url(seed_) + "\n"; }

std::string SigningKey::publisher() const {
	std::array<std::uint8_t, 32> seed = seed_; // the key pair wipes the seed it is given
	std::array<std::uint8_t, 64> secret{};
	PublicKey key{};
	crypto_ed25519_key_pair(secret.data(), key.data(), seed.data());
	crypto_wipe(secret.data(), secret.size());
	return std::string(publisherPrefix) + toBase64url(key);
}

json SigningKey::sign(json document) const {
	if(!document.is_object()) throw DocumentError("only an object can be signed");
	const std::string message = signedBytes(document);
	std::array<std::uint8_t, 32> seed = seed_;
	std::array<std::uint8_t, 64> secret{};
	PublicKey key{};
	crypto_ed25519_key_pair(secret.data(), key.data(), seed.data());
	std::array<std::uint8_t, 64> signature{};
	crypto_ed25519_sign(signature.data(), secret.data(), bytesOf(message), message.size());
	crypto_wipe(secret.data(), secret.size());
	document["signature"] = {{"publisher", std::string(publisherPrefix) + toBase64url(key)},
	                         {"value", toBase64url(signature)}};
	return document;
}

std::string verifySignature(const json& document, const std::string& place) {
	const auto signature = document.is_object() ? document.find("signature") : document.end();
	if(signature == document.end() || !signature->is_object()) throw DocumentError(place + ": it isn't signed");
	const json publisher = signature->value("publisher", json());
	const json value = signature->value("value", json());
	const auto key = publisher.is_string() ? parsePublicKey(publisher.get_ref<const std::string&>()) : std::nullopt;
	if(!key) throw DocumentError(place + ".signature.publisher: expected \"ed25519:<public key>\"");
	const auto bytes = value.is_string() ? fromBase64url(value.get_ref<const std::string&>(), 64) : std::nullopt;
	if(!bytes) throw DocumentError(place + ".signature.value: expected a 64-byte Ed25519 signature");
	const std::string message = signedBytes(document);
	if(crypto_ed25519_check(bytes->data(), key->data(), bytesOf(message), message.size()) != 0)
		throw DocumentError(place + ": the signature doesn't match its content (it was changed after signing)");
	return publisher.get<std::string>();
}

} // namespace affresco::format
