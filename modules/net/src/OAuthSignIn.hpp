#pragma once

/// @file
/// @brief What SignIn's sources share: one provider's entry (its state, tokens and what is in progress).

#include "affresco/net/SignIn.hpp"

#include "LoopbackRedirect.hpp"
#include "OAuth.hpp"
#include "OAuthTimer.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::net {

/// @brief One provider's sign-in. The tokens live only here and in the SecretStore (the refresh token).
struct SignIn::Entry {
	std::string name;                                      ///< The provider's name.
	identity::Provider provider;                           ///< As declared.
	std::string state = "signedOut";                       ///< "signedOut", "signingIn", "signedIn" or "error".
	std::string account;                                   ///< Who signed in, or empty.
	std::string error;                                     ///< Why the last sign-in failed, or empty.
	std::string userCode;                                  ///< A device sign-in's code to type, or empty.
	std::string verificationUrl;                           ///< Where to type it, or empty.
	std::string accessToken;                               ///< Sent as Bearer; never shown.
	std::string refreshToken;                              ///< Exchanged for new tokens; never shown.
	std::chrono::system_clock::time_point expires{};       ///< When accessToken expires.
	bool refusedOnce = false;                              ///< A server refused it, and it refreshed for that.
	oauth::Pending pending;                                ///< A browser sign-in's values.
	std::string deviceCode;                                ///< A device sign-in's code, sent while polling.
	std::chrono::seconds interval{5};                      ///< Between a device sign-in's polls.
	std::chrono::steady_clock::time_point deadline{};      ///< When a device sign-in's codes expire.
	std::unique_ptr<network::Transfer> transfer;           ///< The request in progress, or null.
	std::unique_ptr<LoopbackRedirect> redirect;            ///< The browser's redirect listener, or null.
	OAuthTimer::Id timer = 0;                              ///< The poll or refresh due, or 0.
	std::vector<std::function<void(std::optional<std::string>)>> waiting; ///< signIn() callbacks not yet called.
};

} // namespace affresco::net
