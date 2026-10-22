#pragma once

/// @file
/// @brief The server side of a remote peer's opening (RFC 0003): the WebSocket upgrade request, read with bounds, the
/// answers to it, the Origin check and the first "auth" message. Pure functions, fuzzed (tests/fuzz/peer_handshake.cpp).

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::net::peer {

/// The longest upgrade request (its request line and headers); a longer one is refused with 431.
inline constexpr std::size_t maxHandshakeBytes = std::size_t{16} << 10;
/// The most header lines an upgrade request may have.
inline constexpr std::size_t maxHeaderLines = 64;
/// The longest token a peer may send, in a header or a first message.
inline constexpr std::size_t maxTokenBytes = std::size_t{8} << 10;
/// The longest first message, {"type": "auth", "token": "..."}: also the longest message read before the peer has
/// proven who it is.
inline constexpr std::size_t maxAuthMessageBytes = maxTokenBytes + 64;

/// @brief What an upgrade request says, or why it is refused.
struct Upgrade {
	int status = 0;        ///< 0 if it may go on, else the HTTP status of the refusal (400, 426, 431...).
	std::string reason;    ///< Why it is refused (sent to the peer and logged; never holds the token).
	std::string key;       ///< Sec-WebSocket-Key.
	std::string origin;    ///< The Origin header, if hasOrigin.
	bool hasOrigin = false; ///< Whether there is an Origin header (a browser).
	std::string token;     ///< The bearer token of an Authorization header, or empty.
};

/// @brief Where the request head ends.
/// @param buffer The bytes received so far.
/// @return The offset just past the blank line that ends the head, or nullopt if it hasn't come yet.
[[nodiscard]] std::optional<std::size_t> headEnd(std::string_view buffer) noexcept;

/// @brief Reads an upgrade request: `GET <target> HTTP/1.1`, with Upgrade: websocket, Connection: upgrade,
/// Sec-WebSocket-Version: 13 and a Sec-WebSocket-Key; at most one Origin and one Authorization.
/// @param head The request head, up to and with its blank line (at most maxHandshakeBytes).
/// @return What it says, or the refusal.
[[nodiscard]] Upgrade parseUpgrade(std::string_view head);

/// @brief Whether an origin is on the allowlist (compared without regard to case).
/// @param origin The Origin header.
/// @param allowed The allowed origins.
/// @return Whether it is allowed.
[[nodiscard]] bool originAllowed(std::string_view origin, const std::vector<std::string>& allowed) noexcept;

/// @brief The answer that accepts an upgrade.
/// @param key The request's Sec-WebSocket-Key.
/// @return The response head.
[[nodiscard]] std::string acceptResponse(std::string_view key);

/// @brief The answer that refuses a connection before the upgrade.
/// @param status The HTTP status (400, 401, 403, 408, 426, 429, 431, 503).
/// @param reason Why, as plain text.
/// @return The whole response.
[[nodiscard]] std::string refusalResponse(int status, std::string_view reason);

/// @brief The token of a first message `{"type": "auth", "token": "..."}`.
/// @param message The message.
/// @return The token, or nullopt if the message isn't that (or the token is empty or too long).
[[nodiscard]] std::optional<std::string> authToken(std::string_view message);

} // namespace affresco::net::peer
