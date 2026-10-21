#pragma once

/// @file
/// @brief Remote peers (RFC 0003): the live protocol over WebSocket, one JSON object per text frame. A PeerServer
/// accepts peers that bring a token a TokenVerifier accepts; connectPeer() is the other end.

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"
#include "affresco/net/Client.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::net {

/// @brief Decides who a remote peer is, from the token it brings. The verifier names the author (e.g. "team/ada")
/// from claims it checked; a peer never picks its own name.
///
/// verify() runs on a connection's own thread, one call at a time per connection but possibly at the same time for
/// different connections: it must be thread-safe. It may block (e.g. to fetch keys), within the server's authTimeout.
class TokenVerifier {
public:
	virtual ~TokenVerifier() = default;

	/// @brief Checks a token.
	/// @param token The token, as the peer sent it (never logged, never put in a document).
	/// @return The author the peer's changes are made as, or nullopt to refuse the peer. An exception refuses it too.
	[[nodiscard]] virtual std::optional<std::string> verify(std::string_view token) = 0;
};

/// @brief What a PeerServer accepts. The defaults suit a few peers on the loopback, behind a TLS proxy.
struct PeerServerOptions {
	/// Origins browsers may connect from, exactly as they send them (e.g. "https://studio.example.com"). A connection
	/// with an Origin header that isn't listed is refused; one without (a program, not a browser) is not checked.
	std::vector<std::string> allowedOrigins;
	std::size_t maxPeers = 32;                  ///< Connections open at once, authenticated or not; more are refused.
	double connectionsPerSecond = 5.0;          ///< New connections a second from one address; more are refused.
	std::size_t connectionBurst = 10;           ///< New connections one address may open at once.
	std::chrono::milliseconds authTimeout{5000}; ///< From the connection to its token (header or first message).
	std::size_t maxMessageBytes = std::size_t{8} << 20; ///< The longest message; a longer one closes the peer (1009).
	std::size_t maxQueuedBytes = std::size_t{64} << 20; ///< Bytes a peer may have waiting to be sent; more close it.
	/// Listen on an address other than the loopback. Off by default: put a TLS proxy (e.g. Caddy) in front and keep the
	/// server on 127.0.0.1, as there is no TLS server code in the library.
	bool allowRemote = false;
};

/// @brief Accepts remote peers over WebSocket on a TCP address (RFC 0003).
///
/// Each connection must: be an HTTP/1.1 WebSocket upgrade with bounded headers; come from an allowed Origin if it
/// has one; and bring a token, as `Authorization: Bearer <token>` on the upgrade or as a first message
/// `{"type": "auth", "token": "..."}`, within authTimeout. A peer that fails is closed with a reason (an HTTP status,
/// or a close frame) and the refusal is logged without its token. Connections are capped (maxPeers) and rate-limited
/// per address. Threads: an acceptor, plus a reader and a writer per connection; onConnection runs through the executor.
class PeerServer {
public:
	/// Receives each authenticated peer and the author its verifier named, through the executor.
	using Connected = std::function<void(std::unique_ptr<Channel> channel, std::string author)>;

	/// @brief Listens.
	/// @param executor Delivers connections and messages on the main thread; outlives the server.
	/// @param address "host:port", e.g. "127.0.0.1:7420" (port 0 picks one, see port()).
	/// @param verifier Checks tokens; must not be null.
	/// @param onConnection Receives each authenticated peer.
	/// @param options Limits and origins.
	/// @throws std::runtime_error If the address is malformed, isn't on the loopback without allowRemote, or can't be
	/// listened on.
	PeerServer(Executor& executor, std::string_view address, std::shared_ptr<TokenVerifier> verifier,
	           Connected onConnection, PeerServerOptions options = {});
	/// @brief Stops listening and closes the connections still authenticating (accepted peers belong to their owner).
	~PeerServer();
	PeerServer(const PeerServer&) = delete;
	PeerServer& operator=(const PeerServer&) = delete;
	PeerServer(PeerServer&&) = delete;
	PeerServer& operator=(PeerServer&&) = delete;

	/// @brief The port it listens on.
	/// @return The port.
	[[nodiscard]] std::uint16_t port() const noexcept;

private:
	class State; ///< The listener, the connections authenticating and the limits (RemotePeers.cpp).

	std::shared_ptr<State> state_; ///< Shared with the acceptor thread and the posted tasks.
};

/// @brief Connects to a PeerServer as a peer: `wss://` anywhere, or `ws://` on the loopback (e.g. through a local
/// proxy). The token goes as `Authorization: Bearer`; TLS is verified (ClientOptions::caFile adds authorities).
/// @param executor Delivers messages on the main thread; outlives the channel.
/// @param url The server's URL, e.g. "wss://host.example.com/live".
/// @param token The token its verifier accepts.
/// @param options Proxy, authorities and limits.
/// @return The channel (messages sent before the handshake wait for it; a failure closes it), or nullptr if the URL
/// isn't a wss URL or a ws URL on the loopback.
[[nodiscard]] std::unique_ptr<Channel> connectPeer(Executor& executor, std::string_view url, std::string_view token,
                                                   const ClientOptions& options = {});

} // namespace affresco::net
