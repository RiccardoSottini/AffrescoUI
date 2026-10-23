/// @file
/// @brief Fuzzes the URL parser and the origin allowlist (modules/net/include/affresco/net/Url.hpp): every URL a screen's
/// connection, a sign-in provider or a remote peer names reaches parseUrl, and the allowlist decides what may be opened.
///
/// The input is cut at its first line feed: before it, an allowlist entry; after it, a URL. A URL that parses has a
/// known scheme, a lower-case host of the allowed characters, a port and a target starting with '/'; its origin parses
/// again to the same scheme, host and port. Path segments and query parts encode to unreserved characters only.

#include "Fuzz.hpp"

#include <affresco/net/Url.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

/// @brief Tells whether a character may be left as it is in an encoded segment or query part.
/// @param character The character.
/// @return Whether it is a letter, a digit, one of "-._~" or the '%' of an escape.
bool unreservedOrEscape(char character) noexcept {
	const bool letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
	const bool digit = character >= '0' && character <= '9';
	return letter || digit || character == '-' || character == '.' || character == '_' || character == '~' ||
	       character == '%';
}

/// @brief Checks a parsed URL's target: it starts with '/' and has no space or control character.
/// @param target The target.
void checkTarget(std::string_view target) {
	if(!target.starts_with('/')) affresco::fuzz::broken("a URL's target starts with '/'");
	for(const char character : target)
		if(static_cast<unsigned char>(character) <= ' ' || character == 0x7f)
			affresco::fuzz::broken("a URL's target has no space or control character");
}

/// @brief Checks a URL that parsed.
/// @param url The URL.
void checkUrl(const affresco::net::Url& url) {
	if(url.scheme != "https" && url.scheme != "http" && url.scheme != "wss" && url.scheme != "ws")
		affresco::fuzz::broken("a URL's scheme is one of https, http, wss, ws");
	if(url.host.empty()) affresco::fuzz::broken("a URL has a host");
	for(const char character : url.host)
		if(character >= 'A' && character <= 'Z') affresco::fuzz::broken("a URL's host is in lower case");
	if(url.port == 0) affresco::fuzz::broken("a URL's port is from 1 to 65535");
	checkTarget(url.target);
	const auto again = affresco::net::parseUrl(url.origin());
	if(!again || again->scheme != url.scheme || again->host != url.host || again->port != url.port)
		affresco::fuzz::broken("a URL's origin parses to the same scheme, host and port");
}

/// @brief Checks that a value encodes to unreserved characters and escapes only.
/// @param encoded The encoded value.
void checkEncoded(std::string_view encoded) {
	for(const char character : encoded)
		if(!unreservedOrEscape(character)) affresco::fuzz::broken("an encoded value is unreserved characters and escapes");
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace net = affresco::net;
	const std::string_view input = affresco::fuzz::text(data, size);
	const std::size_t feed = input.find('\n');
	const std::string_view entry = feed == std::string_view::npos ? std::string_view() : input.substr(0, feed);
	const std::string_view text = feed == std::string_view::npos ? input : input.substr(feed + 1);

	const auto url = net::parseUrl(text);
	if(url) checkUrl(*url);
	const std::string segment = net::encodeSegment(text);
	checkEncoded(segment);
	if(segment == "." || segment == "..") affresco::fuzz::broken("a segment is never \".\" or \"..\"");
	checkEncoded(net::encodeQueryPart(text));

	net::OriginAllowlist allowlist;
	const bool allowed = !allowlist.allow(entry).has_value();
	const bool admitted = !allowlist.check(text).has_value();
	if(admitted && !url) affresco::fuzz::broken("the allowlist admits only URLs that parse");
	if(admitted && !allowed) affresco::fuzz::broken("an empty allowlist admits nothing");
	if(admitted && !url->secure() && !net::isLoopback(url->host))
		affresco::fuzz::broken("the allowlist admits plain http and ws only on the loopback");
	return 0;
}
