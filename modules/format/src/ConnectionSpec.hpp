#pragma once

/// @file
/// @brief The "connections" section read into plain values (RFC 0003): each connection's URL, headers, timeouts and
/// retries, and its operations (HTTP) or messages (WebSocket). Shared by the section's check, the Connections backend
/// and the revision rules, so all three read a declaration the same way.

#include "affresco/core/Json.hpp"
#include "affresco/core/Network.hpp"
#include "affresco/core/Services.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>

namespace affresco::format {

inline constexpr std::size_t maxConnections = 64;          ///< Connections one screen declares.
inline constexpr std::size_t maxOperations = 256;          ///< Operations and messages one connection declares.
inline constexpr int maxConnectionTimeout = 600000;        ///< The longest "timeout": ten minutes.
inline constexpr int defaultConnectionTimeout = 30000;     ///< "timeout" when the connection has none.
inline constexpr int maxRetries = 5;                       ///< The most "retry.times".
inline constexpr int defaultReconnect = 2000;              ///< "reconnect": true, in milliseconds.

/// @brief An operation (HTTP) or a message (WebSocket) of a connection.
struct Operation {
	std::string name;                           ///< Its name in the connection.
	bool message = false;                       ///< A WebSocket message: `value` is sent as one text frame.
	std::string method = "GET";                 ///< The HTTP method.
	std::string path;                           ///< Appended to the URL; `{name}` takes a `with` value, one segment.
	json query;                                 ///< Name -> value, evaluated with `with`; or null.
	json value;                                 ///< The body, or the message; evaluated with `with`; null for none.
	bool form = false;                          ///< The body is sent form-encoded, not as JSON.
	network::Headers headers;                   ///< Its own headers (after the connection's).
	services::Effect effect = services::Effect::Read; ///< What it does on the server.
	network::Stream stream = network::Stream::None;   ///< A stream, used with "subscribe".
	std::optional<json::json_pointer> result;   ///< Where in the response body the result is, or the whole body.
	std::optional<int> timeout;                 ///< Its own timeout, in milliseconds.
	json mock;                                  ///< `{status, body}` answered while a proposal is previewed, or null.

	/// @brief What the services see of it: its name, effect, whether it streams, and a line such as
	/// "PUT /projects/{id}". Revisions may use every declared operation, never add one.
	/// @return The method.
	[[nodiscard]] services::MethodSpec spec() const;
};

/// @brief A declared connection.
struct ConnectionSpec {
	std::string name;                      ///< Its name in the section.
	std::string url;                       ///< https://, wss://, or http:// and ws:// (the host allows only the loopback).
	bool socket = false;                   ///< A WebSocket (ws:// or wss://).
	network::Headers headers;              ///< Sent with every request (never Authorization, Cookie or the like).
	int timeout = defaultConnectionTimeout; ///< Per request, in milliseconds.
	int retries = 0;                       ///< Retries of read operations.
	int retryAfter = 500;                  ///< The first wait before a retry, in milliseconds.
	int reconnect = 0;                     ///< WebSocket: the wait before connecting again after a drop (0: never).
	std::map<std::string, Operation, std::less<>> operations; ///< Its operations or messages, by name.
};

/// @brief The connections of a section.
using ConnectionMap = std::map<std::string, ConnectionSpec, std::less<>>;

/// @brief Reads a "connections" section. Every value that leaves the machine (a URL, a header) is literal, every
/// number is bounded, and no header may carry a credential: sign-in sets those (`auth`, accepted as declared).
/// @param section The section (null: none).
/// @param at Its place, for errors ("connections").
/// @return The connections.
/// @throws DocumentError If the section is wrong.
[[nodiscard]] ConnectionMap parseConnections(const json& section, const std::string& at = "connections");

} // namespace affresco::format
