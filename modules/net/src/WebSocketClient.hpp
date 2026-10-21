#pragma once

/// @file
/// @brief The client side of WebSocket connections: libcurl connects (TCP, TLS, the proxy), and the handshake and
/// the frames are RFC 6455 as WebSocketFrames.hpp encodes them, on one thread per connection.

#include "affresco/core/Executor.hpp"
#include "affresco/core/Network.hpp"
#include "affresco/net/Client.hpp"
#include "affresco/net/Url.hpp"

#include <memory>

namespace affresco::net {

/// @brief Opens a connection on its own thread.
/// @param executor The main thread, where every handler runs.
/// @param options The client's options (the proxy, the CA file, the queue and delivery limits).
/// @param url A ws:// or wss:// URL, already checked against the allowlist.
/// @param headers Extra headers of the upgrade request, already checked.
/// @param handlers Told what happens.
/// @return The connection.
[[nodiscard]] std::unique_ptr<network::WebSocket> openWebSocket(Executor& executor, const ClientOptions& options, Url url,
                                                                network::Headers headers,
                                                                network::SocketHandlers handlers);

/// @brief A connection refused before it started: its handlers are told, through the executor, that it closed.
/// @param executor The main thread.
/// @param handlers Told the failure.
/// @param failure Why.
/// @return The connection, closed: send() refuses everything.
[[nodiscard]] std::unique_ptr<network::WebSocket> refusedWebSocket(Executor& executor, network::SocketHandlers handlers,
                                                                   network::Failure failure);

} // namespace affresco::net
