/// @file
/// @brief Fuzzes the parsers of what an HTTP server answers (modules/net/src/Http.hpp): status lines, header lines,
/// header blocks (the WebSocket handshake) and Retry-After values, all read from any server a screen may reach.
///
/// The input is a response head. A parsed status is 100..999; parsed header names are lower-case tokens; a block
/// holds at most the header limit and no value with a line feed; a retry delay is within 0..1 hour.

#include "../../modules/net/src/Http.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

/// @brief Checks a parsed header name against the rules.
/// @param name The name.
void checkName(std::string_view name) {
	if(name.empty() || name.find_first_of(":\r\n \t") != std::string_view::npos)
		affresco::fuzz::broken("a header name is a token");
	for(const char character : name)
		if(std::isupper(static_cast<unsigned char>(character)) != 0) affresco::fuzz::broken("header names are lower case");
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace net = affresco::net;
	const auto input = affresco::fuzz::text(data, size);
	if(const auto status = net::parseStatusLine(input); status && (*status < 100 || *status > 999))
		affresco::fuzz::broken("a status is three digits, from 100");
	if(const auto header = net::parseHeaderLine(input)) checkName(header->first);
	if(const auto delay = net::parseRetryAfter(input); delay && (delay->count() < 0 || *delay > std::chrono::hours{1}))
		affresco::fuzz::broken("a retry delay is within an hour");
	if(const auto block = net::parseHeaderBlock(input)) {
		if(block->headers.size() > net::maxResponseHeaders) affresco::fuzz::broken("a block holds at most the limit");
		for(const auto& [name, value] : block->headers) {
			checkName(name);
			if(value.find('\n') != std::string_view::npos) affresco::fuzz::broken("a header value is one line");
		}
	}
	return 0;
}
