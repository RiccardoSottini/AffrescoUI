#pragma once

/// @file
/// @brief SignIn: the net module's Identity. OAuth 2 and OpenID Connect sign-in for public clients: the authorization
/// code with PKCE and a loopback redirect (RFC 7636, RFC 8252), the device flow (RFC 8628), and refresh. The tokens
/// stay in this object and its SecretStore; the "auth" variable shows only states, accounts and expiry.

#include "affresco/core/Executor.hpp"
#include "affresco/core/Identity.hpp"
#include "affresco/core/Network.hpp"
#include "affresco/core/SecretStore.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::net {

class OAuthTimer;

/// @brief How a SignIn keeps its tokens and opens the browser.
struct SignInOptions {
	/// Where refresh tokens are kept between runs: systemSecretStore() for an application, a MemorySecretStore for a
	/// test; null keeps them in memory.
	std::shared_ptr<SecretStore> store{};
	/// Opens a URL in the person's browser; empty opens the system's default one.
	std::function<void(const std::string& url)> openBrowser{};
	/// The application's name, part of each secret's key ("affrescoui/<application>/<provider>").
	std::string application = "affrescoui";
};

/// @brief Signs in to a document's identity providers (see Identity).
///
/// No client secret is ever used or kept: the providers are public clients. The browser flow's redirect is bound to
/// 127.0.0.1 on a port the system picks and matched exactly; the state and the nonce are random, checked in constant
/// time; every answer is bounded. A failed refresh signs out: the kept secret is erased. The token endpoints must be
/// origins the host's Network allows.
class SignIn final : public identity::Identity {
public:
	/// @brief Takes the transport.
	/// @param executor The main thread's Executor; outlives this object.
	/// @param network The transport; outlives this object.
	/// @param options See SignInOptions.
	SignIn(Executor& executor, network::Network& network, SignInOptions options = {});
	/// @brief Cancels every sign-in in progress (no callback runs afterwards); keeps the kept secrets.
	~SignIn() override;
	SignIn(const SignIn&) = delete;
	SignIn& operator=(const SignIn&) = delete;
	SignIn(SignIn&&) = delete;
	SignIn& operator=(SignIn&&) = delete;

	void declare(const std::vector<std::pair<std::string, identity::Provider>>& providers) override;
	void signIn(const std::string& name, std::function<void(std::optional<std::string> error)> done) override;
	void signOut(const std::string& name) override;
	[[nodiscard]] std::optional<std::string> bearer(const std::string& name) const override;
	void refused(const std::string& name) override;
	[[nodiscard]] json status() const override;
	void onStatus(std::function<void()> callback) override;

private:
	/// @brief One provider's sign-in (OAuthSignIn.cpp).
	struct Entry;

	/// @brief Starts the browser flow.
	/// @param entry The provider's entry.
	void startBrowser(Entry& entry);
	/// @brief Starts the device flow.
	/// @param entry The provider's entry.
	void startDevice(Entry& entry);
	/// @brief Polls the token endpoint for a device sign-in.
	/// @param entry The provider's entry.
	void pollDevice(Entry& entry);
	/// @brief Exchanges a refresh token for new tokens.
	/// @param entry The provider's entry.
	void refresh(Entry& entry);
	/// @brief Posts a form to an endpoint of the provider.
	/// @param entry The provider's entry (it owns the request).
	/// @param url The endpoint.
	/// @param fields The form.
	/// @param done Called with the answer's body, or nullopt, the failure's body and an error.
	void post(Entry& entry, const std::string& url, const std::vector<std::pair<std::string, std::string>>& fields,
	          std::function<void(std::optional<std::string> body, std::string failure, std::string error)> done);
	/// @brief Takes a token endpoint's answer.
	/// @param entry The provider's entry.
	/// @param body The answer.
	/// @param nonce The nonce sent, or empty.
	void accept(Entry& entry, const std::string& body, const std::string& nonce);
	/// @brief Ends a sign-in or a refresh that failed: tokens forgotten, the secret erased.
	/// @param entry The provider's entry.
	/// @param error Why, for people.
	void fail(Entry& entry, const std::string& error);
	/// @brief Stops whatever the entry is doing (a request, a redirect listener, a timer).
	/// @param entry The provider's entry.
	void stop(Entry& entry);
	/// @brief Tells the waiting callbacks, then whoever watches the status.
	/// @param entry The provider's entry.
	/// @param error nullopt for signed in, else why not.
	void finish(Entry& entry, const std::optional<std::string>& error);
	/// @brief Keeps the refresh token and account in the SecretStore.
	/// @param entry The provider's entry.
	void keep(const Entry& entry);
	/// @brief The key of an entry's secret.
	/// @param name The provider's name.
	/// @return "affrescoui/<application>/<name>".
	[[nodiscard]] std::string key(const std::string& name) const;
	/// @brief Calls the status callback.
	void changed() const;

	Executor& executor_;                                         ///< See the constructor.
	network::Network& network_;                                  ///< See the constructor.
	SignInOptions options_;                                      ///< See the constructor.
	std::function<void()> onStatus_;                             ///< See onStatus().
	std::unique_ptr<OAuthTimer> timer_;                          ///< Polls and refreshes.
	std::map<std::string, std::unique_ptr<Entry>> entries_;      ///< By provider name.
};

/// @brief The system's secret store: the Keychain on macOS, the Credential Manager on Windows, the Secret Service
/// (libsecret, when installed) on Linux; else a MemorySecretStore.
/// @param service The service its secrets are kept under, e.g. the application's name.
/// @return The store.
[[nodiscard]] std::shared_ptr<SecretStore> systemSecretStore(const std::string& service);

/// @brief Opens a URL in the system's default browser, without a shell.
/// @param url An https:// (or loopback http://) URL.
void openInBrowser(const std::string& url);

} // namespace affresco::net
