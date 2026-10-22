#pragma once

/// @file
/// @brief The protocol of OAuth 2 and OpenID Connect, without its transport: random values, PKCE (RFC 7636), the
/// authorization URL, form bodies, and the token endpoint's answers read and checked. Every input is bounded.

#include "affresco/core/Identity.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace affresco::net::oauth {

inline constexpr std::size_t maxTokenBytes = 16384;        ///< The largest access, refresh or id token.
inline constexpr std::size_t maxResponseBytes = 65536;     ///< The largest answer of a provider's endpoint.
inline constexpr std::size_t maxAccountBytes = 256;        ///< The longest account name shown.
inline constexpr long long maxLifetime = 31536000;         ///< The longest "expires_in" taken: a year, in seconds.

/// @brief Random bytes from the system's cryptographic generator, as base64url without padding.
/// @param bytes How many bytes (32 for a PKCE verifier, 16 for a state or a nonce).
/// @return The text.
[[nodiscard]] std::string randomText(std::size_t bytes);

/// @brief Writes bytes as base64url (RFC 4648 §5), without padding.
/// @param bytes The bytes.
/// @return The text.
[[nodiscard]] std::string base64Url(std::string_view bytes);

/// @brief Reads base64url (padding optional).
/// @param text The text.
/// @return The bytes, or nullopt if it isn't base64url.
[[nodiscard]] std::optional<std::string> decodeBase64Url(std::string_view text);

/// @brief The PKCE challenge of a verifier: base64url(SHA-256(verifier)), for the method "S256".
/// @param verifier The verifier.
/// @return The challenge.
[[nodiscard]] std::string challenge(std::string_view verifier);

/// @brief Compares two texts in a time that depends only on their lengths, never on where they differ.
/// @param a One.
/// @param b The other.
/// @return Whether they are equal.
[[nodiscard]] bool equalInConstantTime(std::string_view a, std::string_view b) noexcept;

/// @brief Writes name/value pairs as an application/x-www-form-urlencoded body or query.
/// @param fields The pairs.
/// @return The text.
[[nodiscard]] std::string form(const std::vector<std::pair<std::string, std::string>>& fields);

/// @brief The value of a field of a query ("a=1&b=2"), decoded.
/// @param query The query, without "?".
/// @param name The field's name.
/// @return Its value, or nullopt if it is missing or appears twice.
[[nodiscard]] std::optional<std::string> queryField(std::string_view query, std::string_view name);

/// @brief The values a browser sign-in sends with the authorization request and checks on its return.
struct Pending {
	std::string verifier;    ///< The PKCE verifier (kept on this side).
	std::string state;       ///< Matched against the redirect's "state".
	std::string nonce;       ///< Matched against the id token's "nonce".
	std::string redirectUri; ///< "http://127.0.0.1:<port>/callback", matched exactly by the token endpoint.
};

/// @brief The authorization URL a browser opens.
/// @param provider The provider.
/// @param pending The values of this sign-in.
/// @return The URL.
[[nodiscard]] std::string authorizationUrl(const identity::Provider& provider, const Pending& pending);

/// @brief Tokens the token endpoint gave.
struct Tokens {
	std::string access;  ///< The access token (sent as Bearer).
	std::string refresh; ///< The refresh token, or empty.
	std::string account; ///< Who signed in (from the id token), or empty.
	std::chrono::system_clock::time_point expires; ///< When the access token expires.
};

/// @brief Reads and checks a token endpoint's answer: a Bearer access token, its lifetime, and, if the provider names
/// an issuer, an id token whose "iss", "aud", "exp" and "nonce" match (OpenID Connect Core 3.1.3.7; its signature is
/// not checked, since it came straight from the token endpoint over TLS).
/// @param body The answer's body.
/// @param provider The provider.
/// @param nonce The nonce sent, or empty for a refresh or a device sign-in.
/// @param now The time now.
/// @return The tokens, or an error for people (never quoting a token).
[[nodiscard]] std::variant<Tokens, std::string> readTokens(std::string_view body, const identity::Provider& provider,
                                                           std::string_view nonce,
                                                           std::chrono::system_clock::time_point now);

/// @brief A device sign-in's codes (RFC 8628 §3.2).
struct DeviceCodes {
	std::string deviceCode;      ///< Sent while polling; never shown.
	std::string userCode;        ///< Shown, for the person to type.
	std::string verificationUrl; ///< Where to type it.
	std::chrono::seconds interval{5}; ///< Between polls.
	std::chrono::seconds lifetime{600}; ///< How long the codes last.
};

/// @brief Reads a device authorization endpoint's answer.
/// @param body The answer's body.
/// @return The codes, or an error for people.
[[nodiscard]] std::variant<DeviceCodes, std::string> readDeviceCodes(std::string_view body);

/// @brief The "error" of a token endpoint's error answer (RFC 6749 §5.2), e.g. "authorization_pending".
/// @param body The answer's body.
/// @return It, or empty.
[[nodiscard]] std::string errorCode(std::string_view body);

} // namespace affresco::net::oauth
