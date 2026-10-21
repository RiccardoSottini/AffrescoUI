#pragma once

/// @file
/// @brief Client: the net module's Network, over libcurl. HTTP(S) requests, server-sent events and newline-delimited
/// streams on one worker thread, WebSocket connections on a thread each, every outcome posted to the main thread.

#include "affresco/core/Executor.hpp"
#include "affresco/core/Network.hpp"
#include "affresco/net/Url.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::net {

class Worker;

/// @brief How a Client connects.
struct ClientOptions {
	/// The proxy, e.g. "http://proxy.example.com:3128": nullopt follows the environment (HTTPS_PROXY, NO_PROXY, as
	/// curl reads them), "" uses none.
	std::optional<std::string> proxy;
	std::string caFile;                ///< A PEM file of extra certificate authorities, or empty for the system's.
	std::size_t maxPerOrigin = 6;      ///< Requests in flight to one origin; more wait their turn.
	std::size_t maxWaiting = 256;      ///< Requests started and not finished, in all; more fail with "refused".
	std::size_t maxSendQueueBytes = std::size_t{16} << 20; ///< Bytes a WebSocket may hold unsent; send() refuses more.
	std::size_t maxUndelivered = 4096; ///< Stream events or messages posted and not yet handled; more fail "tooLarge".
};

/// @brief The Network of the net module.
///
/// Every request is checked on the calling thread before anything is sent: the method, the URL against the host's
/// allowlist (https and wss anywhere allowed, http and ws on the loopback only), and the headers (no line breaks, no
/// headers the transport sets). TLS 1.2 or later, with the certificate and the host name always verified. Redirects
/// are followed only within the request's origin (at most 5). Responses send no Accept-Encoding and are never
/// decompressed. Every byte received is bounded (see Network.hpp and SECURITY.md).
///
/// The Executor must outlive the Client. A Transfer or WebSocket may outlive it: it then does nothing.
class Client final : public network::Network {
public:
	/// @brief Starts the worker thread.
	/// @param executor Runs the handlers on the main thread.
	/// @param options How to connect.
	explicit Client(Executor& executor, ClientOptions options = {});
	/// @brief Cancels every request (no handler is called) and stops the worker.
	~Client() override;
	Client(const Client&) = delete;
	Client& operator=(const Client&) = delete;
	Client(Client&&) = delete;
	Client& operator=(Client&&) = delete;

	std::optional<std::string> allow(std::string_view origin) override;
	[[nodiscard]] std::optional<std::string> check(std::string_view url) const override;
	[[nodiscard]] std::unique_ptr<network::Transfer> start(network::Request request, network::Handlers handlers) override;
	[[nodiscard]] std::unique_ptr<network::WebSocket> connect(std::string url, network::Headers headers,
	                                                          network::SocketHandlers handlers) override;

private:
	Executor& executor_;             ///< See the constructor.
	ClientOptions options_;          ///< See the constructor.
	OriginAllowlist allowlist_;      ///< The origins the host allows.
	std::shared_ptr<Worker> worker_; ///< The HTTP worker; transfers hold it weakly.
};

/// @brief Checks the headers a caller adds: names that are HTTP tokens, values without CR, LF or NUL, at most 100 of
/// them and 64 KiB in all, and none of the headers the transport sets (Host, Content-Length, Transfer-Encoding,
/// Connection, Upgrade, Expect, TE, Trailer, Proxy-*, Sec-WebSocket-*).
/// @param headers The headers.
/// @return An error for people, or nullopt.
[[nodiscard]] std::optional<std::string> checkHeaders(const network::Headers& headers);

} // namespace affresco::net
