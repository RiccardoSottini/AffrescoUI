/// @file
/// @brief Parsing URLs, the allowlist of origins, and percent-encoding.

#include "affresco/net/Url.hpp"

#include <algorithm>
#include <cctype>

namespace affresco::net {

namespace {

/// @brief The default port of a scheme.
/// @param scheme "https", "http", "wss" or "ws".
/// @return Its port, or 0 for another scheme.
std::uint16_t defaultPort(std::string_view scheme) noexcept {
	if(scheme == "https" || scheme == "wss") return 443;
	if(scheme == "http" || scheme == "ws") return 80;
	return 0;
}

/// @brief Text in lower case (ASCII only).
/// @param text The text.
/// @return It in lower case.
std::string lower(std::string_view text) {
	std::string result(text);
	std::ranges::transform(result, result.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
	return result;
}

/// @brief Whether a host name has only the characters a DNS name or an IPv4 address has.
/// @param host The host, in lower case.
bool isHostName(std::string_view host) noexcept {
	if(host.empty() || host.size() > 253 || host.front() == '.' || host.front() == '-') return false;
	return std::ranges::all_of(host, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.'; });
}

/// @brief Whether text is an IPv6 address's characters (hex digits, ':' and '.').
/// @param host The text between the brackets.
bool isIpv6(std::string_view host) noexcept {
	return host.contains(':') && std::ranges::all_of(host, [](char c) {
		       return std::isxdigit(static_cast<unsigned char>(c)) != 0 || c == ':' || c == '.';
	       });
}

/// @brief Parses a port.
/// @param text Its digits.
/// @return It, or nullopt if it isn't from 1 to 65535.
std::optional<std::uint16_t> parsePort(std::string_view text) noexcept {
	if(text.empty() || text.size() > 5) return std::nullopt;
	unsigned value = 0;
	for(const char c : text) {
		if(c < '0' || c > '9') return std::nullopt;
		value = value * 10 + static_cast<unsigned>(c - '0');
	}
	if(value == 0 || value > 65535) return std::nullopt;
	return static_cast<std::uint16_t>(value);
}

/// @brief Splits an authority into host and port, and checks both.
/// @param authority E.g. "api.example.com:8443" or "[::1]:7420".
/// @param url Receives the host and the port.
/// @return Whether it is well formed.
bool parseAuthority(std::string_view authority, Url& url) {
	std::string_view host = authority;
	std::optional<std::string_view> port;
	if(authority.starts_with('[')) {
		const auto close = authority.find(']');
		if(close == std::string_view::npos) return false;
		host = authority.substr(1, close - 1);
		const auto rest = authority.substr(close + 1);
		if(!rest.empty() && !rest.starts_with(':')) return false;
		if(!rest.empty()) port = rest.substr(1);
		if(!isIpv6(host)) return false;
	} else if(const auto colon = authority.rfind(':'); colon != std::string_view::npos) {
		host = authority.substr(0, colon);
		port = authority.substr(colon + 1);
	}
	url.host = lower(host);
	if(!authority.starts_with('[') && !isHostName(url.host)) return false;
	url.port = defaultPort(url.scheme);
	if(port) {
		const auto parsed = parsePort(*port);
		if(!parsed) return false;
		url.port = *parsed;
	}
	return true;
}

/// @brief Percent-encodes text, keeping the unreserved characters (letters, digits, "-._~").
/// @param value The text.
/// @return It, encoded.
std::string percentEncode(std::string_view value) {
	static constexpr std::string_view digits = "0123456789ABCDEF";
	std::string encoded;
	encoded.reserve(value.size());
	for(const char c : value) {
		const auto byte = static_cast<unsigned char>(c);
		if(std::isalnum(byte) != 0 || c == '-' || c == '.' || c == '_' || c == '~') encoded += c;
		else encoded += {'%', digits[byte >> 4], digits[byte & 0xf]};
	}
	return encoded;
}

/// @brief Whether a URL has a byte no URL may: a control, a space or a backslash (which some readers take for a slash).
/// @param text The URL.
bool hasForbiddenByte(std::string_view text) {
	return std::ranges::any_of(text,
	                           [](char c) { return static_cast<unsigned char>(c) <= 0x20 || c == 0x7f || c == '\\'; });
}

/// @brief The request target of a URL: its path and query, without the fragment, "/" if empty.
/// @param target What follows the authority.
std::string requestTarget(std::string_view target) {
	if(const auto fragment = target.find('#'); fragment != std::string_view::npos) target = target.substr(0, fragment);
	if(target.empty()) return "/";
	return target.starts_with('?') ? "/" + std::string(target) : std::string(target);
}

} // namespace

std::string Url::origin() const { return scheme + "://" + authority(); }

std::string Url::authority() const {
	std::string result = host.contains(':') ? "[" + host + "]" : host;
	if(port != defaultPort(scheme)) result += ":" + std::to_string(port);
	return result;
}

std::optional<Url> parseUrl(std::string_view text) {
	if(text.size() > 8192) return std::nullopt;
	if(hasForbiddenByte(text)) return std::nullopt;
	const auto separator = text.find("://");
	if(separator == std::string_view::npos) return std::nullopt;
	Url url;
	url.scheme = lower(text.substr(0, separator));
	if(defaultPort(url.scheme) == 0) return std::nullopt;
	const auto rest = text.substr(separator + 3);
	const auto end = rest.find_first_of("/?#");
	const auto authority = rest.substr(0, end);
	if(authority.empty() || authority.contains('@') || !parseAuthority(authority, url)) return std::nullopt;
	url.target = requestTarget(end == std::string_view::npos ? std::string_view{} : rest.substr(end));
	return url;
}

bool isLoopback(std::string_view host) noexcept {
	return host == "localhost" || host == "::1" || (host.starts_with("127.") && isHostName(host) &&
	                                                 std::ranges::all_of(host, [](char c) { return c == '.' || (c >= '0' && c <= '9'); }));
}

bool isSignInPage(std::string_view url) {
	const auto parsed = parseUrl(url);
	return parsed && (parsed->scheme == "https" || (parsed->scheme == "http" && isLoopback(parsed->host)));
}

std::optional<std::string> OriginAllowlist::allow(std::string_view origin) {
	std::string text(origin);
	bool subdomains = false;
	if(const auto star = text.find("://*."); star != std::string::npos) {
		subdomains = true;
		text.erase(star + 3, 2);
	}
	const auto url = parseUrl(text);
	if(!url || url->target != "/" || (text.find('/', text.find("://") + 3) != std::string::npos))
		return "'" + std::string(origin) + "' isn't an origin (a scheme, a host and a port, nothing more)";
	if(text.contains('*')) return "'" + std::string(origin) + "': a wildcard may only be the first label";
	entries_.push_back({url->secure(), subdomains, url->host, url->port});
	return std::nullopt;
}

bool OriginAllowlist::admits(const Url& url) const {
	if(!url.secure() && !isLoopback(url.host)) return false;
	return std::ranges::any_of(entries_, [&](const Entry& entry) {
		if(entry.secure != url.secure() || entry.port != url.port) return false;
		if(!entry.subdomains) return entry.host == url.host;
		return url.host.size() > entry.host.size() + 1 && url.host.ends_with("." + entry.host);
	});
}

std::optional<std::string> OriginAllowlist::check(std::string_view url) const {
	const auto parsed = parseUrl(url);
	if(!parsed)
		return "'" + std::string(url.substr(0, 200)) +
		       "' isn't a URL this app can open (https, wss, or http and ws on the loopback)";
	if(!parsed->secure() && !isLoopback(parsed->host))
		return "'" + parsed->host + "' needs TLS: http:// and ws:// are for the loopback only";
	if(!admits(*parsed)) return "'" + parsed->authority() + "' isn't a host this app may connect to";
	return std::nullopt;
}

std::string encodeSegment(std::string_view value) {
	if(value == "." || value == "..") return value == "." ? "%2E" : "%2E%2E";
	return percentEncode(value);
}

std::string encodeQueryPart(std::string_view value) { return percentEncode(value); }

} // namespace affresco::net
