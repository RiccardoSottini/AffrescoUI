#pragma once

/// @file
/// @brief A connection's "auth" (RFC 0003): the identity provider it signs in to, read and checked, and the bearer
/// token added to its requests. Sign-in itself is the host's Identity (net::SignIn); a screen sees only "auth.<name>".

#include "ConnectionSpec.hpp"

#include "affresco/core/Identity.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

/// @brief Reads a connection's "auth" object: {flow, signInUrl, deviceUrl, tokenUrl, clientId, scopes, issuer, note}.
/// Every URL is literal (https://, or http:// on the loopback); never a client secret.
/// @param value The object.
/// @param at Its place, for errors.
/// @return The provider.
/// @throws DocumentError If it is wrong.
[[nodiscard]] identity::Provider readProvider(const json& value, const std::string& at);

/// @brief Links the connections whose "auth" names another connection to that one's sign-in.
/// @param connections The section's connections; each one's authName is set.
/// @param section The section, for the names.
/// @param at Its place, for errors.
/// @throws DocumentError If a name isn't a connection with its own sign-in.
void linkSignIns(ConnectionMap& connections, const json& section, const std::string& at);

/// @brief The providers the connections declare, by connection name, for Identity::declare.
/// @param connections The connections.
/// @return The providers.
[[nodiscard]] std::vector<std::pair<std::string, identity::Provider>> declaredProviders(const ConnectionMap& connections);

/// @brief Adds a connection's bearer token to a request's headers.
/// @param connection The connection.
/// @param identity The host's Identity, or null.
/// @param headers The request's headers.
/// @return nullopt if it may go (signed in, or no sign-in), else why not.
[[nodiscard]] std::optional<std::string> authorize(const ConnectionSpec& connection, const identity::Identity* identity,
                                                   network::Headers& headers);

} // namespace affresco::format
