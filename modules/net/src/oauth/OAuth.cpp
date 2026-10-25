/// @file
/// @brief The protocol of OAuth 2 and OpenID Connect (OAuth.hpp).

#include "oauth/OAuth.hpp"

#include "Random.hpp"
#include "affresco/core/Sha256.hpp"
#include "affresco/net/Url.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace affresco::net::oauth {

namespace {

using Json = nlohmann::json;

/// The base64url alphabet.
constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/// @brief The value of a hex digit.
/// @param c The digit.
/// @return 0 to 15, or -1.
int hexValue(char c) noexcept {
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/// @brief Decodes a form value: "+" is a space, "%XX" a byte.
/// @param text The value.
/// @return It, or nullopt if an escape is wrong.
std::optional<std::string> decodeFormValue(std::string_view text) {
	std::string out;
	for(std::size_t i = 0; i < text.size(); ++i) {
		if(text[i] == '+') out += ' ';
		else if(text[i] != '%') out += text[i];
		else if(i + 2 >= text.size()) return std::nullopt;
		else {
			const int high = hexValue(text[i + 1]);
			const int low = hexValue(text[i + 2]);
			if(high < 0 || low < 0) return std::nullopt;
			out += static_cast<char>(high * 16 + low);
			i += 2;
		}
	}
	return out;
}

/// @brief Parses a provider's JSON answer, bounded.
/// @param body The body.
/// @return The object, or nullopt if it isn't one.
std::optional<Json> parseObject(std::string_view body) {
	if(body.size() > maxResponseBytes) return std::nullopt;
	Json value = Json::parse(body, nullptr, false);
	if(value.is_discarded() || !value.is_object()) return std::nullopt;
	return value;
}

/// @brief A string field of an answer, bounded.
/// @param object The answer.
/// @param name The field.
/// @param limit The most bytes.
/// @return It, or empty if it is missing, not a string or too long.
std::string field(const Json& object, const char* name, std::size_t limit) {
	const auto found = object.find(name);
	if(found == object.end() || !found->is_string()) return {};
	const auto& text = found->get_ref<const std::string&>();
	return text.size() <= limit ? text : std::string();
}

/// @brief A number of seconds an answer gives, within bounds.
/// @param value The number.
/// @param least The fewest seconds.
/// @param most The most seconds.
/// @return It, clamped.
long long secondsIn(const Json& value, long long least, long long most) {
	// Any number may come, 1e300 too: clamped as a double, so it is never cast to an integer out of range
	return static_cast<long long>(
	    std::clamp(value.get<double>(), static_cast<double>(least), static_cast<double>(most)));
}

/// @brief The claims of an id token, a JWT (its signature isn't checked: the token came straight from the provider,
/// over TLS).
/// @param idToken The id token.
/// @return Them, or nullopt if it isn't a JWT or they aren't a JSON object.
std::optional<Json> readClaims(std::string_view idToken) {
	const std::size_t first = idToken.find('.');
	const std::size_t second = first == std::string_view::npos ? first : idToken.find('.', first + 1);
	if(second == std::string_view::npos) return std::nullopt;
	const auto payload = decodeBase64Url(idToken.substr(first + 1, second - first - 1));
	return payload ? parseObject(*payload) : std::nullopt;
}

/// @brief Whether claims name a client among their audience.
/// @param claims The claims.
/// @param clientId The client.
bool isForClient(const Json& claims, const std::string& clientId) {
	const auto aud = claims.find("aud");
	if(aud == claims.end()) return false;
	if(aud->is_string()) return *aud == clientId;
	return aud->is_array() && std::ranges::find(*aud, Json(clientId)) != aud->end();
}

/// @brief Whether claims have expired, allowing a minute for clocks that differ.
/// @param claims The claims.
/// @param now The time now.
bool hasExpired(const Json& claims, std::chrono::system_clock::time_point now) {
	const auto exp = claims.find("exp");
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
	return exp == claims.end() || !exp->is_number() || exp->get<double>() < static_cast<double>(seconds) - 60;
}

/// @brief The account claims name: a user name, an email or the subject, whichever comes first.
/// @param claims The claims.
/// @return It, or empty.
std::string accountOf(const Json& claims) {
	for(const char* name : {"preferred_username", "email", "sub"})
		if(std::string account = field(claims, name, maxAccountBytes); !account.empty()) return account;
	return {};
}

/// @brief Reads and checks an id token's claims (OpenID Connect Core 3.1.3.7, steps 2, 3, 9 and 11).
/// @param idToken The id token.
/// @param provider The provider.
/// @param nonce The nonce sent, or empty when none was (refresh, device).
/// @param now The time now.
/// @param account Receives the account it names (or empty).
/// @return An error, or nullopt.
std::optional<std::string> readIdToken(std::string_view idToken, const identity::Provider& provider,
                                       std::string_view nonce, std::chrono::system_clock::time_point now,
                                       std::string& account) {
	if(std::ranges::count(idToken, '.') < 2) return "the id token isn't a JWT";
	const auto claims = readClaims(idToken);
	if(!claims) return "the id token's claims aren't JSON";
	if(!equalInConstantTime(field(*claims, "iss", 2048), provider.issuer))
		return "the id token's issuer isn't the provider";
	if(!isForClient(*claims, provider.clientId)) return "the id token isn't for this application";
	if(hasExpired(*claims, now)) return "the id token expired";
	if(!nonce.empty() && !equalInConstantTime(field(*claims, "nonce", 256), nonce))
		return "the id token's nonce isn't this sign-in's";
	account = accountOf(*claims);
	return std::nullopt;
}

} // namespace

std::string randomText(std::size_t bytes) {
	std::string raw(bytes, '\0');
	fillRandom(std::as_writable_bytes(std::span(raw)));
	return base64Url(raw);
}

std::string base64Url(std::string_view bytes) {
	std::string out;
	out.reserve((bytes.size() * 4 + 2) / 3);
	std::uint32_t buffer = 0;
	int bits = 0;
	for(const char c : bytes) {
		buffer = (buffer << 8) | static_cast<unsigned char>(c);
		bits += 8;
		while(bits >= 6) {
			bits -= 6;
			out += alphabet[(buffer >> bits) & 63u];
		}
	}
	if(bits > 0) out += alphabet[(buffer << (6 - bits)) & 63u];
	return out;
}

std::optional<std::string> decodeBase64Url(std::string_view text) {
	while(text.ends_with('=')) text.remove_suffix(1);
	std::string out;
	std::uint32_t buffer = 0;
	int bits = 0;
	for(const char c : text) {
		const std::size_t value = alphabet.find(c);
		if(value == std::string_view::npos) return std::nullopt;
		buffer = (buffer << 6) | static_cast<std::uint32_t>(value);
		bits += 6;
		if(bits >= 8) {
			bits -= 8;
			out += static_cast<char>((buffer >> bits) & 0xffu);
		}
	}
	return out;
}

std::string challenge(std::string_view verifier) {
	const auto digest = sha256::digest(verifier);
	return base64Url(std::string_view(reinterpret_cast<const char*>(digest.data()), digest.size()));
}

bool equalInConstantTime(std::string_view a, std::string_view b) noexcept {
	unsigned difference = a.size() == b.size() ? 0u : 1u;
	const std::size_t length = std::max(a.size(), b.size());
	for(std::size_t i = 0; i < length; ++i) {
		const auto x = i < a.size() ? static_cast<unsigned char>(a[i]) : 0u;
		const auto y = i < b.size() ? static_cast<unsigned char>(b[i]) : 0u;
		difference |= x ^ y;
	}
	return difference == 0;
}

std::string form(const std::vector<std::pair<std::string, std::string>>& fields) {
	std::string out;
	for(const auto& [name, value] : fields) {
		if(!out.empty()) out += '&';
		out += encodeQueryPart(name) + "=" + encodeQueryPart(value);
	}
	return out;
}

std::optional<std::string> queryField(std::string_view query, std::string_view name) {
	std::optional<std::string> found;
	while(!query.empty()) {
		const std::size_t end = std::min(query.find('&'), query.size());
		const std::string_view pair = query.substr(0, end);
		query.remove_prefix(std::min(end + 1, query.size()));
		const std::size_t equals = pair.find('=');
		if(pair.substr(0, equals) != name) continue;
		if(found) return std::nullopt; // twice: neither is trusted
		found = decodeFormValue(equals == std::string_view::npos ? std::string_view() : pair.substr(equals + 1));
		if(!found) return std::nullopt;
	}
	return found;
}

std::string authorizationUrl(const identity::Provider& provider, const Pending& pending) {
	std::string scopes;
	for(const std::string& scope : provider.scopes) scopes += (scopes.empty() ? "" : " ") + scope;
	std::vector<std::pair<std::string, std::string>> fields = {
	    {"response_type", "code"},          {"client_id", provider.clientId},
	    {"redirect_uri", pending.redirectUri}, {"state", pending.state},
	    {"code_challenge", challenge(pending.verifier)}, {"code_challenge_method", "S256"}};
	if(!scopes.empty()) fields.emplace_back("scope", scopes);
	if(!provider.issuer.empty()) fields.emplace_back("nonce", pending.nonce);
	return provider.signInUrl + (provider.signInUrl.contains('?') ? "&" : "?") + form(fields);
}

std::variant<Tokens, std::string> readTokens(std::string_view body, const identity::Provider& provider,
                                             std::string_view nonce, std::chrono::system_clock::time_point now) {
	const auto answer = parseObject(body);
	if(!answer) return "the provider's answer isn't JSON";
	Tokens tokens;
	tokens.access = field(*answer, "access_token", maxTokenBytes);
	if(tokens.access.empty()) return "the provider gave no access token";
	const std::string type = field(*answer, "token_type", 16);
	if(type.size() != 6 || !std::ranges::equal(type, std::string_view("bearer"), [](char a, char b) {
		   return (a | 0x20) == b;
	   }))
		return "the provider's token isn't a Bearer token";
	tokens.refresh = field(*answer, "refresh_token", maxTokenBytes);
	long long lifetime = 3600;
	if(const auto in = answer->find("expires_in"); in != answer->end() && in->is_number())
		lifetime = secondsIn(*in, 1, maxLifetime);
	tokens.expires = now + std::chrono::seconds(lifetime);
	if(provider.issuer.empty()) return tokens;
	const std::string idToken = field(*answer, "id_token", maxTokenBytes);
	if(idToken.empty()) return nonce.empty() ? std::variant<Tokens, std::string>(tokens) : "the provider gave no id token";
	if(auto error = readIdToken(idToken, provider, nonce, now, tokens.account)) return *std::move(error);
	return tokens;
}

std::variant<DeviceCodes, std::string> readDeviceCodes(std::string_view body) {
	const auto answer = parseObject(body);
	if(!answer) return "the provider's answer isn't JSON";
	DeviceCodes codes;
	codes.deviceCode = field(*answer, "device_code", maxTokenBytes);
	codes.userCode = field(*answer, "user_code", 64);
	codes.verificationUrl = field(*answer, "verification_uri", 2048);
	const auto verification = parseUrl(codes.verificationUrl);
	if(codes.deviceCode.empty() || codes.userCode.empty() || !verification)
		return "the provider's device codes are incomplete";
	// The page the person types the code into: encrypted, unless it is on this machine (a test provider).
	if(!verification->secure() && !isLoopback(verification->host))
		return "the provider's verification page isn't https";
	if(const auto in = answer->find("interval"); in != answer->end() && in->is_number())
		codes.interval = std::chrono::seconds(secondsIn(*in, 1, 60));
	if(const auto in = answer->find("expires_in"); in != answer->end() && in->is_number())
		codes.lifetime = std::chrono::seconds(secondsIn(*in, 10, 3600));
	return codes;
}

std::string errorCode(std::string_view body) {
	const auto answer = parseObject(body);
	return answer ? field(*answer, "error", 64) : std::string();
}

} // namespace affresco::net::oauth
