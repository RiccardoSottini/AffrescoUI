#pragma once

/// @file
/// @brief URLs as the net module checks them: parsing, origins, the host's allowlist, and encoding arguments into a
/// path segment or a query.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::net {

/// @brief The parts of an absolute URL that matter here.
struct Url {
	std::string scheme;   ///< "https", "http", "wss" or "ws", in lower case.
	std::string host;     ///< In lower case; an IPv6 address without its brackets.
	std::uint16_t port{}; ///< The port, or the scheme's default.
	std::string target;   ///< The path and query, starting with '/' ("/" when there is none).

	/// @brief The origin: scheme, host and port, e.g. "https://api.example.com:443".
	[[nodiscard]] std::string origin() const;
	/// @brief Whether the scheme encrypts (https, wss).
	[[nodiscard]] bool secure() const noexcept { return scheme == "https" || scheme == "wss"; }
	/// @brief The host as it goes into a Host header or a URL: an IPv6 address in brackets, the port if not the default.
	[[nodiscard]] std::string authority() const;
};

/// @brief Parses an absolute URL strictly: no user information ('@' before the host), no spaces or control
/// characters, a port from 1 to 65535, a host of letters, digits, '-', '.' (or an IPv6 address in brackets).
/// @param text The URL.
/// @return It, or nullopt if it isn't one of the schemes above or isn't well formed.
[[nodiscard]] std::optional<Url> parseUrl(std::string_view text);

/// @brief Whether a host is the loopback: "localhost", "127.0.0.1" (any 127.x.y.z) or "::1".
/// @param host The host, in lower case, without brackets.
[[nodiscard]] bool isLoopback(std::string_view host) noexcept;

/// @brief The origins an app may reach, set by the host in C++.
///
/// An entry is an origin ("https://api.example.com") or a wildcard over subdomains ("https://*.example.com", which
/// doesn't match the bare domain). http:// and ws:// count only on the loopback. https and wss are the same origin
/// here, and so are http and ws: allowing "https://api.example.com" lets a screen open "wss://api.example.com".
class OriginAllowlist {
public:
	/// @brief Adds an entry.
	/// @param origin See the class.
	/// @return An error if it isn't an origin (a path, user information, a wildcard that isn't the first label), or
	/// nullopt.
	std::optional<std::string> allow(std::string_view origin);
	/// @brief Whether a URL may be reached.
	/// @param url The URL.
	/// @return An error for people, or nullopt.
	[[nodiscard]] std::optional<std::string> check(std::string_view url) const;
	/// @brief Whether a parsed URL may be reached.
	/// @param url The URL.
	/// @return Whether its origin is allowed and it is encrypted or on the loopback.
	[[nodiscard]] bool admits(const Url& url) const;

private:
	/// @brief One entry.
	struct Entry {
		bool secure = true;     ///< https/wss, or http/ws.
		bool subdomains = false; ///< A wildcard: the host must end in "." + host.
		std::string host;       ///< In lower case.
		std::uint16_t port{};   ///< Must match.
	};
	std::vector<Entry> entries_; ///< In the order added.
};

/// @brief Percent-encodes a value into one path segment: everything but letters, digits and "-._~" is encoded, so
/// '/', '?', '#' and ".." can't leave the segment ("../admin" becomes "..%2Fadmin"; "." and ".." alone are encoded
/// too).
/// @param value The value.
/// @return The segment.
[[nodiscard]] std::string encodeSegment(std::string_view value);

/// @brief Percent-encodes a query name or value (a space is "%20").
/// @param value The value.
/// @return It, encoded.
[[nodiscard]] std::string encodeQueryPart(std::string_view value);

} // namespace affresco::net
