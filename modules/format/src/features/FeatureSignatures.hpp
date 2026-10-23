#pragma once

/// @file
/// @brief Signatures of feature packages and catalogues (RFC 0004, decision D12): Ed25519 over canonical JSON
/// (RFC 8785), with Monocypher, and their SHA-512 digests.
///
/// A signed document carries `"signature": { "publisher": "ed25519:<key>", "value": "<signature>" }`; what is signed
/// is the canonical form of the document without it. Keys and signatures are base64url without padding, read with
/// fixed lengths; keys and digests are compared in constant time.

#include "affresco/core/Json.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

inline constexpr std::string_view publisherPrefix = "ed25519:";     ///< A public key's prefix.
inline constexpr std::string_view secretKeyPrefix = "ed25519-secret:"; ///< A key file's prefix, before the seed.
inline constexpr std::string_view digestPrefix = "sha512:";         ///< A digest's prefix.

/// A public key: 32 bytes.
using PublicKey = std::array<std::uint8_t, 32>;

/// @brief Writes bytes as base64url, without padding (RFC 4648, section 5).
/// @param bytes The bytes.
/// @return The text.
[[nodiscard]] std::string toBase64url(std::span<const std::uint8_t> bytes);

/// @brief Reads base64url text of an exact length, strictly: no padding, no other characters, no stray bits.
/// @param text The text.
/// @param size The number of bytes it must hold.
/// @return The bytes, or std::nullopt if it isn't exactly that.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> fromBase64url(std::string_view text, std::size_t size);

/// @brief The canonical form of a JSON value (RFC 8785): members sorted by their names' UTF-16 code units, no white
/// space, numbers as ECMAScript prints them, strings with the fewest escapes.
/// @param value The value.
/// @return The canonical text.
[[nodiscard]] std::string canonicalJson(const json& value);

/// @brief Reads a public key, "ed25519:<43 characters>".
/// @param text The text.
/// @return The key, or std::nullopt if it isn't one.
[[nodiscard]] std::optional<PublicKey> parsePublicKey(std::string_view text);

/// @brief Tells whether two public keys are the same, in constant time.
/// @param a One.
/// @param b The other.
/// @return true if they are.
[[nodiscard]] bool sameKey(const PublicKey& a, const PublicKey& b) noexcept;

/// @brief The digest of some bytes, "sha512:<base64url>".
/// @param bytes The bytes.
/// @return The digest.
[[nodiscard]] std::string digestOf(std::string_view bytes);

/// @brief Tells whether two digests are the same, in constant time once both have the expected form.
/// @param a One, "sha512:<86 characters>".
/// @param b The other.
/// @return true if both are well formed and equal.
[[nodiscard]] bool sameDigest(std::string_view a, std::string_view b) noexcept;

/// @brief Wipes secret text, so it doesn't stay in freed memory.
/// @param text The text; empty afterwards.
void wipe(std::string& text) noexcept;

/// @brief A publisher's signing key: its 32-byte seed, wiped when it ends. Never written anywhere but the key file a
/// person asked for.
class SigningKey {
public:
	/// @brief Makes a new key from the operating system's random source.
	/// @return The key.
	[[nodiscard]] static SigningKey generate();

	/// @brief Reads a key file's text, "ed25519-secret:<43 characters>" (a trailing newline is allowed).
	/// @param text The text.
	/// @return The key.
	/// @throws DocumentError If it isn't one.
	[[nodiscard]] static SigningKey fromText(std::string_view text);

	/// @brief Wipes the key.
	~SigningKey();
	SigningKey(const SigningKey&) = delete;
	SigningKey& operator=(const SigningKey&) = delete;
	/// @brief Moves a key, wiping the one moved from.
	/// @param other The key.
	SigningKey(SigningKey&& other) noexcept;
	SigningKey& operator=(SigningKey&&) = delete;

	/// @brief The key file's text: "ed25519-secret:<seed>" and a newline.
	/// @return The text (the caller keeps it secret).
	[[nodiscard]] std::string toText() const;

	/// @brief Its public key.
	/// @return "ed25519:<key>".
	[[nodiscard]] std::string publisher() const;

	/// @brief Signs a document: adds its "signature" over the canonical form of the rest.
	/// @param document An object; an earlier "signature" is replaced.
	/// @return The signed document.
	[[nodiscard]] json sign(json document) const;

private:
	/// @brief Takes a seed.
	/// @param seed The seed.
	explicit SigningKey(const std::array<std::uint8_t, 32>& seed) noexcept;

	std::array<std::uint8_t, 32> seed_{}; ///< The seed: the secret.
};

/// @brief Checks a document's signature against the key it names.
/// @param document The signed document.
/// @param place Its place, for errors ("catalog", "wordCount").
/// @return The publisher's key, as the document names it ("ed25519:<key>").
/// @throws DocumentError If it has no signature, or the signature isn't the key's over the rest.
std::string verifySignature(const json& document, const std::string& place);

} // namespace affresco::format
