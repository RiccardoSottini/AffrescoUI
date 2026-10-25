#pragma once

/// @file
/// @brief The Identity interface: signing in to the identity providers a screen's connections name (OAuth 2 and
/// OpenID Connect), as the format module sees it. The net module implements it (net::SignIn); a test may pass its own.
///
/// A screen never sees a token: it sees each sign-in's state, account and expiry ("auth.<name>"), and the runtime asks
/// for the bearer token only to add it to a request's Authorization header.

#include "affresco/core/Json.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace affresco::identity {

/// @brief An identity provider, as a connection's "auth" declares it.
struct Provider {
	std::string flow = "browser";     ///< "browser" (authorization code with PKCE, RFC 7636) or "device" (RFC 8628).
	std::string signInUrl;            ///< The authorization endpoint (for "browser").
	std::string deviceUrl;            ///< The device authorization endpoint (for "device").
	std::string tokenUrl;             ///< The token endpoint.
	std::string clientId;             ///< The public client's id (never a secret: no client secret is ever used).
	std::vector<std::string> scopes;  ///< E.g. {"openid", "profile"}.
	std::string issuer;               ///< The id token's expected "iss", or empty to take the account from the response.
	std::string sentTo;               ///< Where its tokens go: the URL of the connection that declares it.

	/// @brief Whether two providers are the same.
	[[nodiscard]] bool operator==(const Provider&) const = default;
};

/// @brief Signs in to providers by name, keeps the tokens out of sight, and refreshes them.
///
/// Every member is called on the main thread; every callback runs there too.
class Identity {
public:
	Identity() = default;
	Identity(const Identity&) = delete;
	Identity& operator=(const Identity&) = delete;
	Identity(Identity&&) = delete;
	Identity& operator=(Identity&&) = delete;
	virtual ~Identity() = default;

	/// @brief Declares the providers a document names, replacing those declared before. A provider that changed signs
	/// out; one that stayed keeps its sign-in.
	/// @param providers By name (the connection's).
	virtual void declare(const std::vector<std::pair<std::string, Provider>>& providers) = 0;
	/// @brief Starts signing in (or resumes a kept sign-in). Returns at once.
	/// @param name The provider's name.
	/// @param done Called once on the main thread: nullopt when signed in, else an error for people.
	virtual void signIn(const std::string& name, std::function<void(std::optional<std::string> error)> done) = 0;
	/// @brief Signs out: forgets the tokens, here and in the SecretStore. A sign-in in progress stops.
	/// @param name The provider's name.
	virtual void signOut(const std::string& name) = 0;
	/// @brief The access token to send, for the transport only: never shown, stored in a document or logged.
	/// @param name The provider's name.
	/// @return It, or nullopt if not signed in (or it expired).
	[[nodiscard]] virtual std::optional<std::string> bearer(const std::string& name) const = 0;
	/// @brief Tells it a server refused the token (401): it refreshes once, else signs out.
	/// @param name The provider's name.
	virtual void refused(const std::string& name) = 0;
	/// @brief Every provider's state, as the "auth" variable shows it: {name: {state, account, expires, error, code,
	/// verificationUrl}}, the state "signedOut", "signingIn", "signedIn" or "error". Never a token.
	[[nodiscard]] virtual json status() const = 0;
	/// @brief Sets what runs, on the main thread, after a state changes.
	/// @param callback The callback, or empty.
	virtual void onStatus(std::function<void()> callback) = 0;
};

} // namespace affresco::identity
