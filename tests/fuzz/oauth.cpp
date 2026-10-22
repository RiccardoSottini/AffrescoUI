/// @file
/// @brief Fuzzes what a sign-in reads from outside (modules/net/src/OAuth.hpp): the loopback redirect's query, as a
/// browser sends it to LoopbackRedirect, base64url, and the answers of a provider's token and device endpoints.
///
/// The input is a query and an answer body at once. A field read is never longer than the query; base64url read back
/// writes the same bytes; tokens are within their limits and expire within the longest lifetime; device codes poll
/// every 1..60 s and last 10 s..1 h.

#include "../../modules/net/src/OAuth.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <affresco/core/Identity.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <variant>

namespace {

namespace oauth = affresco::net::oauth;

/// @brief Reads the redirect's fields as LoopbackRedirect does.
/// @param query The query.
void readRedirect(std::string_view query) {
	for(const char* name : {"state", "code", "error"})
		if(const auto value = oauth::queryField(query, name); value && value->size() > query.size())
			affresco::fuzz::broken("a field is never longer than its query");
	if(const auto bytes = oauth::decodeBase64Url(query); bytes && oauth::decodeBase64Url(oauth::base64Url(*bytes)) != bytes)
		affresco::fuzz::broken("base64url reads back what it writes");
}

/// @brief Reads the input as a token endpoint's answer, with and without an issuer to check.
/// @param body The answer.
void readTokens(std::string_view body) {
	const auto now = std::chrono::system_clock::now();
	affresco::identity::Provider provider;
	for(const bool issuer : {false, true}) {
		provider.issuer = issuer ? "https://issuer.example" : "";
		const auto read = oauth::readTokens(body, provider, issuer ? "nonce" : "", now);
		const auto* tokens = std::get_if<oauth::Tokens>(&read);
		if(!tokens) continue;
		if(tokens->access.empty() || tokens->access.size() > oauth::maxTokenBytes ||
		   tokens->refresh.size() > oauth::maxTokenBytes || tokens->account.size() > oauth::maxAccountBytes)
			affresco::fuzz::broken("tokens are within their limits");
		if(tokens->expires <= now || tokens->expires > now + std::chrono::seconds(oauth::maxLifetime))
			affresco::fuzz::broken("a token expires within the longest lifetime");
	}
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const auto input = affresco::fuzz::text(data, size);
	readRedirect(input);
	readTokens(input);
	if(const auto read = oauth::readDeviceCodes(input); const auto* codes = std::get_if<oauth::DeviceCodes>(&read)) {
		using std::chrono::seconds;
		if(codes->interval < seconds(1) || codes->interval > seconds(60) || codes->lifetime < seconds(10) ||
		   codes->lifetime > seconds(3600))
			affresco::fuzz::broken("device codes poll every 1..60 s and last 10 s..1 h");
	}
	if(oauth::errorCode(input).size() > 64) affresco::fuzz::broken("an error code is short");
	return 0;
}
