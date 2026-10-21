#pragma once

/// @file
/// @brief The Network interface: HTTP(S) requests, server-sent events and WebSocket connections, as the format module
/// sees them (RFC 0003). The net module implements it; a host or a test may pass its own.

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::network {

/// The most a response body, a server-sent event or a WebSocket message may be, by default: more fails with
/// "tooLarge". Large data is a data model (RFC 0002), not a response.
inline constexpr std::size_t defaultMaxBodyBytes = std::size_t{8} << 20;

/// @brief Header names and values, in order. Names compare without case.
using Headers = std::vector<std::pair<std::string, std::string>>;

/// @brief How a response body is read.
enum class Stream {
	None,   ///< Read whole, then delivered once to Handlers::done.
	Events, ///< Server-sent events (text/event-stream): each event to Handlers::event.
	Lines,  ///< One event per line (e.g. newline-delimited JSON): each line to Handlers::event, as its data.
};

/// @brief One HTTP request.
struct Request {
	std::string method = "GET"; ///< GET, POST, PUT, PATCH or DELETE.
	std::string url;            ///< https://, or http:// to the loopback only. The host must allow its origin.
	Headers headers;            ///< Extra headers (never Host, Content-Length or Transfer-Encoding: the client sets them).
	std::string body;           ///< Sent as it is (with a Content-Type header of the caller's).
	/// The longest the whole request may take, or for a stream, the longest until its response starts.
	std::chrono::milliseconds timeout{30000};
	int retries = 0; ///< Retries after a network error, 429, 502, 503 or 504 (for requests that only read).
	std::chrono::milliseconds retryAfter{500}; ///< The first wait before a retry; doubled each time (Retry-After wins).
	Stream stream = Stream::None;              ///< How the body is read.
	std::size_t maxBodyBytes = defaultMaxBodyBytes; ///< The largest body, or for a stream, the largest event.
};

/// @brief A response with a status from 200 to 299 (redirects within the origin are followed).
struct Response {
	int status = 0;   ///< E.g. 200.
	Headers headers;  ///< Its headers, names in lower case.
	std::string body; ///< Its body (empty for a stream: the events went to Handlers::event).
};

/// @brief Why a request or a connection failed.
struct Failure {
	/// "network", "timeout", "cancelled", "status" (a status of 300 or more, not a followed redirect), "refused" (the
	/// host doesn't allow it, or too many wait), "tooLarge", "protocol" (the server broke the protocol).
	std::string code;
	std::string message; ///< For people, in English; never a header's value or a token.
	int status = 0;      ///< The status for "status", else 0.
	std::string body;    ///< The body for "status" (at most maxBodyBytes), else empty.
};

/// @brief One event of a stream.
struct StreamEvent {
	std::string type = "message"; ///< The event's "event:" field, or "message"; "line" for Stream::Lines.
	std::string data;             ///< Its data (the "data:" lines joined by '\n'), or the line.
	std::string id;               ///< Its "id:" field, or empty.
};

/// @brief What a request tells its caller, on the main thread (through the Executor). Each may be empty.
///
/// Exactly one of done and failed is called, once, unless the Transfer is destroyed first: then none is.
struct Handlers {
	std::function<void(Response)> done;         ///< The response, or a stream's end.
	std::function<void(Failure)> failed;        ///< The failure.
	std::function<void(StreamEvent)> event;     ///< Each event of a stream, in order, before done or failed.
};

/// @brief A request or a connection in progress. Destroying it cancels it: no handler is called afterwards.
class Transfer {
public:
	Transfer() = default;
	Transfer(const Transfer&) = delete;
	Transfer& operator=(const Transfer&) = delete;
	Transfer(Transfer&&) = delete;
	Transfer& operator=(Transfer&&) = delete;
	virtual ~Transfer() = default;
};

/// @brief What a WebSocket connection tells its caller, on the main thread. Each may be empty.
struct SocketHandlers {
	std::function<void()> opened;                     ///< The handshake succeeded: send() may be called.
	std::function<void(std::string)> message;         ///< Each text message, in order.
	std::function<void(std::optional<Failure>)> closed; ///< The end: nullopt for a clean close, else why. Called once.
};

/// @brief An open (or opening) WebSocket connection. Destroying it closes it: no handler is called afterwards.
class WebSocket : public Transfer {
public:
	/// @brief Queues a text message; returns at once. Messages sent before "opened" wait for it.
	/// @param message UTF-8 text, at most maxBodyBytes.
	/// @return false if the connection is closed, or the message too large or too much is queued.
	virtual bool send(std::string_view message) = 0;
};

/// @brief Opens HTTP requests and WebSocket connections to the origins the host allows.
///
/// Every function is called on the main thread. The handlers are called on the main thread too, never from inside
/// the call that started them.
class Network {
public:
	Network() = default;
	Network(const Network&) = delete;
	Network& operator=(const Network&) = delete;
	Network(Network&&) = delete;
	Network& operator=(Network&&) = delete;
	virtual ~Network() = default;

	/// @brief Lets requests reach an origin.
	/// @param origin "https://api.example.com" (any port given must match), or "https://*.example.com" for any
	/// subdomain (not the bare domain). Only the scheme, host and port count. http:// and ws:// are for the loopback.
	/// @return An error if it isn't an origin, or nullopt.
	virtual std::optional<std::string> allow(std::string_view origin) = 0;
	/// @brief Whether a URL may be reached: its origin is allowed, and it is https/wss or on the loopback.
	/// @param url The URL.
	/// @return An error for people ("'api.example.net' isn't a host this app may connect to"), or nullopt.
	[[nodiscard]] virtual std::optional<std::string> check(std::string_view url) const = 0;
	/// @brief Starts a request.
	/// @param request The request; its URL is checked first (a refused one fails with "refused").
	/// @param handlers Told the outcome.
	/// @return The request in progress: keep it until it ends, destroy it to cancel.
	[[nodiscard]] virtual std::unique_ptr<Transfer> start(Request request, Handlers handlers) = 0;
	/// @brief Opens a WebSocket connection (ws:// on the loopback, or wss://).
	/// @param url The URL; checked first (a refused one is closed with "refused").
	/// @param headers Extra headers of the upgrade request.
	/// @param handlers Told what happens.
	/// @return The connection: keep it while it is used, destroy it to close.
	[[nodiscard]] virtual std::unique_ptr<WebSocket> connect(std::string url, Headers headers,
	                                                         SocketHandlers handlers) = 0;
};

} // namespace affresco::network
