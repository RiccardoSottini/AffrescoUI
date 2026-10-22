/// @file
/// @brief Fuzzes the OAuth 2 and OpenID Connect readers (modules/net/src/OAuth.hpp): a token endpoint's answer, a
/// device authorization endpoint's answer, an error answer, a redirect's query and base64url, all of which come from
/// the network.
///
/// The first byte picks the reader and whether the provider names an issuer (then an id token is read and checked).
/// Tokens that are read have an access token and expire after now; an error never quotes the access token; device codes
/// poll at a sane interval; base64url read back writes the same bytes.

#include "../../modules/net/src/OAuth.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace {

/// @brief The provider the answers are read for.
/// @param issuer Whether it names an issuer (an id token is then required and checked).
/// @return It.
affresco::identity::Provider provider(bool issuer) {
	affresco::identity::Provider result;
	result.tokenUrl = "https://sign-in.example.com/token";
	result.clientId = "affrescoui-fuzz";
	result.scopes = {"openid"};
	if(issuer) result.issuer = "https://sign-in.example.com";
	return result;
}

/// @brief Reads a token endpoint's answer and checks what it gives.
/// @param body The answer.
/// @param issuer Whether the provider names an issuer.
void readTokens(std::string_view body, bool issuer) {
	namespace oauth = affresco::net::oauth;
	const auto now = std::chrono::system_clock::time_point(std::chrono::seconds(1'800'000'000));
	const auto read = [&](std::string_view answer) {
		return oauth::readTokens(answer, provider(issuer), issuer ? "n0nce" : "", now);
	};
	const auto result = read(body);
	if(const auto* tokens = std::get_if<oauth::Tokens>(&result)) {
		if(tokens->access.empty()) affresco::fuzz::broken("tokens have an access token");
		if(tokens->access.size() > oauth::maxTokenBytes || tokens->refresh.size() > oauth::maxTokenBytes ||
		   tokens->account.size() > oauth::maxAccountBytes)
			affresco::fuzz::broken("tokens are within their limits");
		if(tokens->expires <= now || tokens->expires > now + std::chrono::seconds(oauth::maxLifetime))
			affresco::fuzz::broken("a token expires within the longest lifetime");
	} else {
		const auto& error = std::get<std::string>(result);
		if(error.empty()) affresco::fuzz::broken("a refused answer says why");
		const std::size_t at = body.find("\"access_token\":\"");
		if(at != std::string_view::npos) {
			const std::string_view rest = body.substr(at + 16);
			const std::string_view token = rest.substr(0, rest.find('"'));
			if(token.size() >= 12 && error.contains(token)) {
				// The token may be words of the error's own text: then the error still has them for another token
				std::string other(body);
				for(std::size_t i = 0; i < token.size(); ++i) other[at + 16 + i] = token[i] == 'q' ? 'z' : 'q';
				const auto again = read(other);
				const auto* otherError = std::get_if<std::string>(&again);
				if(otherError && !otherError->contains(token))
					affresco::fuzz::broken("an error never quotes the access token");
			}
		}
	}
}

/// @brief Reads a device authorization endpoint's answer and checks what it gives.
/// @param body The answer.
void readDeviceCodes(std::string_view body) {
	namespace oauth = affresco::net::oauth;
	const auto result = oauth::readDeviceCodes(body);
	if(const auto* codes = std::get_if<oauth::DeviceCodes>(&result)) {
		if(codes->deviceCode.empty() || codes->userCode.empty()) affresco::fuzz::broken("device codes have both codes");
		using std::chrono::seconds;
		if(codes->interval < seconds(1) || codes->interval > seconds(60) || codes->lifetime < seconds(10) ||
		   codes->lifetime > seconds(3600))
			affresco::fuzz::broken("device codes poll every 1..60 s and last 10 s..1 h");
	} else if(std::get<std::string>(result).empty()) {
		affresco::fuzz::broken("refused device codes say why");
	}
}

/// @brief Reads base64url and a query, and checks that what reads writes back the same.
/// @param text The text.
void readEncodings(std::string_view text) {
	namespace oauth = affresco::net::oauth;
	if(const auto bytes = oauth::decodeBase64Url(text)) {
		const auto again = oauth::decodeBase64Url(oauth::base64Url(*bytes));
		if(!again || *again != *bytes) affresco::fuzz::broken("base64url read back writes the same bytes");
	}
	for(const char* name : {"state", "code", "error"})
		if(const auto value = oauth::queryField(text, name); value && value->size() > text.size())
			affresco::fuzz::broken("a field is never longer than its query");
	if(oauth::errorCode(text).size() > 64) affresco::fuzz::broken("an error code is short");
}

} // namespace

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0) return 0;
	const std::string_view body = affresco::fuzz::text(data + 1, size - 1);
	switch(data[0] % 4) {
	case 0: readTokens(body, false); break;
	case 1: readTokens(body, true); break;
	case 2: readDeviceCodes(body); break;
	default: readEncodings(body); break;
	}
	return 0;
}
