#pragma once

/// @file
/// @brief A mock OAuth 2 / OpenID Connect provider on the loopback, for the sign-in tests: the authorization endpoint
/// (PKCE), the device flow, the token endpoint (codes, refresh) and a resource that wants the bearer token. Every
/// client id, code and token is generated per instance, so no test value is ever a fixed secret.

#include "NetServer.hpp"
#include "OAuth.hpp" // modules/net/src: form fields, the PKCE challenge, base64url

#include <cstdio>
#include <map>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>

namespace affresco::test {

/// @brief The mock provider. Its endpoints are origin() + "/authorize", "/device", "/token", "/userinfo" (the subject
/// of the current access token, else 401) and "/api/..." (the resource: 200 with the account for the current access
/// token, else 401).
class MockIdentity {
public:
	/// @brief Starts the provider.
	MockIdentity() : server_([this](net::tcp::Connection& connection) { serve(connection); }) {}

	/// @brief The provider's origin, e.g. "http://127.0.0.1:50123".
	[[nodiscard]] std::string origin() const { return server_.origin(); }
	/// @brief The client id the application declares (generated).
	[[nodiscard]] const std::string& clientId() const noexcept { return clientId_; }
	/// @brief The account the id token names.
	[[nodiscard]] static std::string account() { return "tester"; }

	/// @brief The "auth" object of a connection that signs in here.
	/// @param flow "browser" or "device".
	/// @return The declaration.
	[[nodiscard]] json provider(const std::string& flow) const {
		return {{"flow", flow},
		        {flow == "browser" ? "signInUrl" : "deviceUrl", origin() + (flow == "browser" ? "/authorize" : "/device")},
		        {"tokenUrl", origin() + "/token"},
		        {"clientId", clientId_},
		        {"issuer", origin()},
		        {"scopes", json::array({"openid", "profile"})}};
	}

	/// @brief The subject ("sub") userinfo gives for the current access token.
	[[nodiscard]] static std::string subject() { return "subject-tester"; }
	/// @brief Signs in without a client, as if a flow had just finished.
	/// @return A new access token (the previous one no longer works).
	[[nodiscard]] std::string signIn() {
		const std::scoped_lock lock(mutex_);
		static_cast<void>(tokens(""));
		return access_;
	}
	/// @brief How many times userinfo was asked.
	[[nodiscard]] int userinfoAsked() const {
		const std::scoped_lock lock(mutex_);
		return userinfoAsked_;
	}

	/// @brief The person approves the device code shown: the next poll gets the tokens.
	void approve() {
		const std::scoped_lock lock(mutex_);
		approved_ = true;
	}
	/// @brief Makes the access tokens issued so far invalid (the resource answers 401), as if they expired early.
	void revokeAccess() {
		const std::scoped_lock lock(mutex_);
		access_.clear();
	}
	/// @brief Makes the refresh token invalid too: a refresh is refused.
	void revokeRefresh() {
		const std::scoped_lock lock(mutex_);
		refresh_.clear();
	}
	/// @brief Every secret it issued: codes, access and refresh tokens. None may ever show outside the sign-in.
	[[nodiscard]] std::set<std::string> secrets() const {
		const std::scoped_lock lock(mutex_);
		return issued_;
	}
	/// @brief How many token requests it answered with tokens.
	[[nodiscard]] int grants() const {
		const std::scoped_lock lock(mutex_);
		return grants_;
	}

	/// @brief What a browser does with an authorization URL: asks the provider, then follows its redirect to the
	/// application's loopback listener.
	/// @param url The authorization URL the application opened.
	/// @return The listener's answer (its status line first), or empty if a step failed.
	static std::string browse(const std::string& url) {
		const std::string location = header(get(url), "Location");
		return location.empty() ? std::string() : get(location);
	}

private:
	/// @brief A new random value: 32 hex digits.
	/// @return The value.
	static std::string generated() {
		std::random_device device;
		std::string text;
		for(int i = 0; i < 4; ++i) {
			char part[9];
			std::snprintf(part, sizeof part, "%08x", static_cast<unsigned>(device()));
			text += part;
		}
		return text;
	}

	/// @brief A value it issues, remembered in issued_.
	/// @param prefix What it is, e.g. "access".
	/// @return The value.
	std::string issue(const std::string& prefix) {
		std::string value = prefix + "-" + generated();
		issued_.insert(value);
		return value;
	}

	/// @brief An unsigned id token (the token endpoint is the source, so the client reads its claims).
	/// @param nonce The nonce, or empty.
	/// @return The JWT.
	std::string idToken(const std::string& nonce) const {
		json claims = {{"iss", origin()}, {"aud", clientId_}, {"exp", 4102444800}, {"preferred_username", account()}};
		if(!nonce.empty()) claims["nonce"] = nonce;
		return net::oauth::base64Url(R"({"alg":"none"})") + "." + net::oauth::base64Url(claims.dump()) + ".";
	}

	/// @brief A token answer, with new access and refresh tokens.
	/// @param nonce The nonce of the sign-in, or empty.
	/// @return The JSON body.
	std::string tokens(const std::string& nonce) {
		access_ = issue("access");
		refresh_ = issue("refresh");
		++grants_;
		return json{{"access_token", access_},  {"token_type", "Bearer"},       {"expires_in", 3600},
		            {"refresh_token", refresh_}, {"id_token", idToken(nonce)}}
		    .dump();
	}

	/// @brief Answers the token endpoint.
	/// @param body The form body.
	/// @return The response.
	std::string token(std::string_view body) {
		const auto field = [body](std::string_view name) {
			return net::oauth::queryField(body, name).value_or("");
		};
		const auto refuse = [](const char* error) {
			return httpResponse("400 Bad Request", json{{"error", error}}.dump());
		};
		if(field("client_id") != clientId_) return refuse("invalid_client");
		const std::string grant = field("grant_type");
		if(grant == "authorization_code") {
			const auto code = codes_.find(field("code"));
			if(code == codes_.end() || net::oauth::challenge(field("code_verifier")) != code->second.challenge ||
			   field("redirect_uri") != code->second.redirect)
				return refuse("invalid_grant");
			const std::string nonce = code->second.nonce;
			codes_.erase(code); // a code works once
			return httpResponse("200 OK", tokens(nonce));
		}
		if(grant == "refresh_token") {
			if(refresh_.empty() || field("refresh_token") != refresh_) return refuse("invalid_grant");
			return httpResponse("200 OK", tokens(""));
		}
		if(grant == "urn:ietf:params:oauth:grant-type:device_code") {
			if(field("device_code") != deviceCode_) return refuse("invalid_grant");
			if(!approved_) return refuse("authorization_pending");
			deviceCode_.clear();
			return httpResponse("200 OK", tokens(""));
		}
		return refuse("unsupported_grant_type");
	}

	/// @brief Answers the authorization endpoint: approves at once and redirects back with a code.
	/// @param query The query.
	/// @return The response.
	std::string authorize(std::string_view query) {
		const auto field = [query](std::string_view name) {
			return net::oauth::queryField(query, name).value_or("");
		};
		const std::string redirect = field("redirect_uri");
		if(field("client_id") != clientId_ || field("code_challenge_method") != "S256" ||
		   !redirect.starts_with("http://127.0.0.1:"))
			return httpResponse("400 Bad Request", "refused");
		const std::string code = issue("code");
		codes_[code] = {field("code_challenge"), redirect, field("nonce")};
		return httpResponse("302 Found", "",
		                    "Location: " + redirect + "?" +
		                        net::oauth::form({{"code", code}, {"state", field("state")}}) + "\r\n");
	}

	/// @brief Answers one connection.
	/// @param connection The connection.
	void serve(net::tcp::Connection& connection) {
		const ServedRequest request = readRequest(connection);
		const std::string line = request.head.substr(0, request.head.find("\r\n"));
		const std::size_t space = line.find(' ');
		const std::string target = line.substr(space + 1, line.rfind(' ') - space - 1);
		const std::string path = target.substr(0, target.find('?'));
		const std::string query = target.contains('?') ? target.substr(target.find('?') + 1) : "";
		std::string answer;
		{
			const std::scoped_lock lock(mutex_);
			if(path == "/authorize") answer = authorize(query);
			else if(path == "/token") answer = token(request.body);
			else if(path == "/device") {
				deviceCode_ = issue("device");
				answer = httpResponse("200 OK", json{{"device_code", deviceCode_}, {"user_code", "WDJB-MJHT"},
				                                     {"verification_uri", origin() + "/verify"}, {"interval", 1},
				                                     {"expires_in", 60}}
				                                    .dump());
			} else if(path == "/userinfo") {
				++userinfoAsked_;
				const bool allowed = !access_.empty() && header(request.head, "Authorization") == "Bearer " + access_;
				answer = allowed ? httpResponse("200 OK", json{{"sub", subject()}}.dump())
				                 : httpResponse("401 Unauthorized", R"({"error":"invalid_token"})");
			} else if(path.starts_with("/api/")) {
				const bool allowed = !access_.empty() && header(request.head, "Authorization") == "Bearer " + access_;
				answer = allowed ? httpResponse("200 OK", json{{"account", account()}, {"ok", true}}.dump())
				                 : httpResponse("401 Unauthorized", R"({"error":"invalid_token"})");
			} else answer = httpResponse("404 Not Found", "");
		}
		static_cast<void>(connection.send(answer, std::chrono::seconds{5}));
	}

	/// @brief A header's value in a response or request head.
	/// @param head The head.
	/// @param name The header's name, as sent.
	/// @return The value, or empty.
	static std::string header(const std::string& head, const std::string& name) {
		const std::size_t at = head.find("\r\n" + name + ": ");
		if(at == std::string::npos) return {};
		const std::size_t start = at + name.size() + 4;
		return head.substr(start, head.find("\r\n", start) - start);
	}

	/// @brief A GET of an http://127.0.0.1 URL, the way a browser does it.
	/// @param url The URL.
	/// @return The whole response, or empty.
	static std::string get(const std::string& url) {
		const std::string prefix = "http://127.0.0.1:";
		if(!url.starts_with(prefix)) return {};
		const std::size_t slash = url.find('/', prefix.size());
		const auto port = static_cast<std::uint16_t>(std::stoi(url.substr(prefix.size(), slash - prefix.size())));
		auto connection = net::tcp::Connection::open("127.0.0.1", port);
		if(!connection) return {};
		const std::string request =
		    "GET " + url.substr(slash) + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
		if(!connection->send(request, std::chrono::seconds{5})) return {};
		std::string response;
		while(connection->receive(response, std::chrono::seconds{5}) == net::tcp::Received::Data) {}
		return response;
	}

	/// @brief An authorization code waiting to be exchanged.
	struct Code {
		std::string challenge; ///< The PKCE challenge it was asked with.
		std::string redirect;  ///< The redirect URI it was asked with.
		std::string nonce;     ///< The nonce, for the id token.
	};

	mutable std::mutex mutex_;          ///< Guards what follows (the server's thread and the test's).
	std::string clientId_ = "client-" + generated(); ///< See clientId().
	std::map<std::string, Code> codes_; ///< Codes issued, not yet exchanged.
	std::string deviceCode_;            ///< The device code issued, until exchanged.
	bool approved_ = false;             ///< See approve().
	std::string access_;                ///< The valid access token.
	std::string refresh_;               ///< The valid refresh token.
	std::set<std::string> issued_;      ///< See secrets().
	int grants_ = 0;                    ///< See grants().
	int userinfoAsked_ = 0;             ///< See userinfoAsked().
	LocalServer server_;                ///< Last: it starts once the rest is ready.
};

} // namespace affresco::test
