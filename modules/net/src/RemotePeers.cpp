/// @file
/// @brief Remote peers (RFC 0003): the server that accepts them, with its caps, and the client that connects as one.

#include "affresco/net/RemotePeers.hpp"

#include "Curl.hpp"
#include "PeerConnection.hpp"
#include "PeerHandshake.hpp"
#include "Tcp.hpp"
#include "WebSocketClient.hpp"

#include "affresco/core/Log.hpp"
#include "affresco/net/Url.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace affresco::net {

namespace {

/// The longest the acceptor waits to send a refusal (429, 503) before it closes the connection anyway.
constexpr std::chrono::milliseconds turnAwayTimeout{50};
/// Addresses whose rate is tracked; past it, those back to a full burst are forgotten.
constexpr std::size_t maxTrackedAddresses = 4096;

/// @brief Refuses a connection before reading it, and logs why.
/// @param connection The connection.
/// @param status 429 or 503.
/// @param reason Why.
void turnAway(tcp::Connection& connection, int status, std::string_view reason) {
	log(LogLevel::Warning, "A remote peer from " + connection.peer() + " was refused: " + std::string(reason));
	static_cast<void>(connection.send(peer::refusalResponse(status, reason), turnAwayTimeout));
	connection.close();
}

/// @brief A Channel over a WebSocket client connection (connectPeer()).
class ClientPeer final : public Channel {
public:
	/// @brief Connects.
	/// @param executor The main thread.
	/// @param options How to connect.
	/// @param url The server's URL, checked.
	/// @param token The token, checked.
	ClientPeer(Executor& executor, const ClientOptions& options, Url url, std::string_view token) {
		network::SocketHandlers handlers;
		handlers.message = [this](std::string message) { onMessage(message); };
		handlers.closed = [this](std::optional<network::Failure>) { onClosed(); };
		socket_ = openWebSocket(executor, options, std::move(url), {{"Authorization", "Bearer " + std::string(token)}},
		                        std::move(handlers));
	}

	/// @brief Sends one message (it waits for the handshake; a full queue drops it, as the connection is failing).
	/// @param message The message.
	void send(std::string_view message) override { static_cast<void>(socket_->send(message)); }

private:
	std::unique_ptr<network::WebSocket> socket_; ///< Destroyed first: no handler runs after.
};

} // namespace

/// @brief The listener, the connections still opening, and the rate per address.
class PeerServer::State : public std::enable_shared_from_this<State> {
public:
	/// @brief Takes the server's parts (listen() starts it).
	/// @param shared What every connection shares.
	/// @param onConnection Receives each authenticated peer.
	/// @param listener The listening socket.
	State(std::shared_ptr<PeerConnection::Shared> shared, Connected onConnection,
	      std::unique_ptr<tcp::Listener> listener)
	    : shared_(std::move(shared)), onConnection_(std::move(onConnection)), listener_(std::move(listener)) {}
	State(const State&) = delete;
	State& operator=(const State&) = delete;
	State(State&&) = delete;
	State& operator=(State&&) = delete;
	/// @brief Stops (stop() must have run).
	~State() = default;

	/// @brief Starts the acceptor thread.
	void start() {
		acceptor_ = std::thread([this] { acceptLoop(); });
	}

	/// @brief Stops listening, waits for the acceptor, and closes the connections still opening.
	void stop() {
		listener_->close();
		if(acceptor_.joinable()) acceptor_.join();
		std::unordered_map<PeerConnection*, std::unique_ptr<PeerConnection>> pending;
		{
			std::lock_guard lock(mutex_);
			pending.swap(pending_);
		}
	}

	/// @brief The port listened on.
	[[nodiscard]] std::uint16_t port() const noexcept { return listener_->port(); }

private:
	/// @brief A token bucket for one address.
	struct Bucket {
		double tokens = 0;                       ///< Connections it may still open now.
		std::chrono::steady_clock::time_point at; ///< When tokens was last brought up to date.
	};

	/// @brief Accepts connections until stop().
	void acceptLoop() {
		const std::size_t maxPeers = shared_->options.maxPeers;
		while(auto connection = listener_->accept()) {
			if(!admit(connection->peer())) {
				turnAway(*connection, 429, "too many connections from this address");
				continue;
			}
			if(shared_->open->load() >= maxPeers) {
				turnAway(*connection, 503, "the server has as many peers as it takes");
				continue;
			}
			++*shared_->open;
			std::lock_guard lock(mutex_); // before the reader can report, so its report finds it
			auto peer = std::make_unique<PeerConnection>(
			    shared_, std::move(connection),
			    [weak = weak_from_this()](PeerConnection& opened, std::optional<std::string> author) {
				    const auto state = weak.lock();
				    if(!state) return;
				    state->shared_->executor.post([weak, &opened, author = std::move(author)]() mutable {
					    if(const auto alive = weak.lock()) alive->finish(&opened, std::move(author));
				    });
			    });
			PeerConnection* key = peer.get();
			pending_.emplace(key, std::move(peer));
		}
	}

	/// @brief Whether an address may open one more connection now (its bucket refills at connectionsPerSecond).
	/// @param address The address.
	[[nodiscard]] bool admit(const std::string& address) {
		const auto now = std::chrono::steady_clock::now();
		const double burst = static_cast<double>(std::max<std::size_t>(shared_->options.connectionBurst, 1));
		if(buckets_.size() >= maxTrackedAddresses) forgetFull(now, burst);
		Bucket& entry = buckets_.try_emplace(address, Bucket{burst, now}).first->second;
		const std::chrono::duration<double> elapsed = now - entry.at;
		entry.tokens = std::min(burst, entry.tokens + elapsed.count() * shared_->options.connectionsPerSecond);
		entry.at = now;
		if(entry.tokens < 1) return false;
		entry.tokens -= 1;
		return true;
	}

	/// @brief Forgets the addresses whose bucket is full again (or all, if none is), so the map stays bounded.
	/// @param now The time.
	/// @param burst A full bucket.
	void forgetFull(std::chrono::steady_clock::time_point now, double burst) {
		std::erase_if(buckets_, [&](const auto& entry) {
			const std::chrono::duration<double> elapsed = now - entry.second.at;
			return entry.second.tokens + elapsed.count() * shared_->options.connectionsPerSecond >= burst;
		});
		if(buckets_.size() >= maxTrackedAddresses) buckets_.clear();
	}

	/// @brief Ends a connection's opening, on the main thread: hands an authenticated peer over, drops a refused one.
	/// @param connection The connection.
	/// @param author Its author, or nullopt.
	void finish(PeerConnection* connection, std::optional<std::string> author) {
		std::unique_ptr<PeerConnection> owned;
		{
			std::lock_guard lock(mutex_);
			const auto found = pending_.find(connection);
			if(found == pending_.end()) return;
			owned = std::move(found->second);
			pending_.erase(found);
		}
		if(author && onConnection_) onConnection_(std::move(owned), std::move(*author));
	}

	std::shared_ptr<PeerConnection::Shared> shared_; ///< What every connection shares.
	Connected onConnection_;                         ///< See PeerServer.
	std::unique_ptr<tcp::Listener> listener_;        ///< The listening socket.
	std::mutex mutex_;                               ///< Guards pending_.
	std::unordered_map<PeerConnection*, std::unique_ptr<PeerConnection>> pending_; ///< Connections still opening.
	std::unordered_map<std::string, Bucket> buckets_; ///< The rate per address (acceptor thread only).
	std::thread acceptor_;                            ///< Runs acceptLoop().
};

PeerServer::PeerServer(Executor& executor, std::string_view address, std::shared_ptr<TokenVerifier> verifier,
                       Connected onConnection, PeerServerOptions options) {
	if(!verifier) throw std::runtime_error("A peer server needs a token verifier");
	auto split = tcp::splitAddress(address);
	if(!split) throw std::runtime_error("'" + std::string(address) + "' isn't a host:port address");
	auto& [host, port] = *split;
	if(host == "localhost") host = "127.0.0.1";
	if(!options.allowRemote && !isLoopback(host))
		throw std::runtime_error("A peer server listens on the loopback unless allowRemote is set (put a TLS proxy in "
		                         "front instead)");
	std::string error;
	auto listener = tcp::Listener::open(host, port, error);
	if(!listener) throw std::runtime_error("Can't listen on '" + std::string(address) + "': " + error);
	auto shared = std::make_shared<PeerConnection::Shared>(PeerConnection::Shared{
	    executor, std::move(verifier), std::move(options), std::make_shared<std::atomic<std::size_t>>(0)});
	state_ = std::make_shared<State>(std::move(shared), std::move(onConnection), std::move(listener));
	state_->start();
}

PeerServer::~PeerServer() { state_->stop(); }

std::uint16_t PeerServer::port() const noexcept { return state_->port(); }

std::unique_ptr<Channel> connectPeer(Executor& executor, std::string_view url, std::string_view token,
                                     const ClientOptions& options) {
	auto parsed = parseUrl(url);
	if(!parsed || !(parsed->scheme == "wss" || (parsed->scheme == "ws" && isLoopback(parsed->host)))) return nullptr;
	if(token.empty() || token.size() > peer::maxTokenBytes ||
	   !std::ranges::all_of(token, [](char c) { return c > ' ' && c < 0x7f; }))
		return nullptr;
	static_cast<void>(curl::start()); // if it failed, the connection closes at once
	return std::make_unique<ClientPeer>(executor, options, std::move(*parsed), token);
}

} // namespace affresco::net
