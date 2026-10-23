/// @file
/// @brief Client: checking each request on the calling thread, then handing it to the worker (HTTP) or to a
/// connection thread (WebSocket).

#include "affresco/net/Client.hpp"

#include "http/Curl.hpp"
#include "http/Delivery.hpp"
#include "http/Http.hpp"
#include "websocket/WebSocketClient.hpp"
#include "http/Worker.hpp"

#include <algorithm>
#include <array>

namespace affresco::net {

namespace {

/// The most headers a caller may add.
constexpr std::size_t maxRequestHeaders = 100;
/// The most bytes the headers a caller adds may be.
constexpr std::size_t maxRequestHeaderBytes = std::size_t{64} << 10;
/// The longest timeout a request may ask for.
constexpr std::chrono::milliseconds longestTimeout{std::chrono::hours{1}};

/// @brief An HTTP request handed to the worker. Destroying it cancels the request.
class HttpTransfer final : public network::Transfer {
public:
	/// @brief Holds the request.
	/// @param worker The worker (held weakly: the Client may go first).
	/// @param id The request's number.
	/// @param delivery Its delivery.
	HttpTransfer(std::weak_ptr<Worker> worker, std::uint64_t id, std::shared_ptr<RequestDelivery> delivery) noexcept
	    : worker_(std::move(worker)), id_(id), delivery_(std::move(delivery)) {}
	/// @brief Cancels the request: no handler is called afterwards.
	~HttpTransfer() override {
		delivery_->cancel();
		if(const auto worker = worker_.lock()) worker->cancel(id_);
	}
	HttpTransfer(const HttpTransfer&) = delete;
	HttpTransfer& operator=(const HttpTransfer&) = delete;
	HttpTransfer(HttpTransfer&&) = delete;
	HttpTransfer& operator=(HttpTransfer&&) = delete;

private:
	std::weak_ptr<Worker> worker_;              ///< See the constructor.
	std::uint64_t id_;                          ///< See the constructor.
	std::shared_ptr<RequestDelivery> delivery_; ///< See the constructor.
};

/// @brief A request that failed before it started: its failure is posted, like any other.
class FailedTransfer final : public network::Transfer {
public:
	/// @brief Holds the delivery the failure was posted to.
	/// @param delivery The delivery.
	explicit FailedTransfer(std::shared_ptr<RequestDelivery> delivery) noexcept : delivery_(std::move(delivery)) {}
	/// @brief Stops the failure from being delivered, if it hasn't been.
	~FailedTransfer() override { delivery_->cancel(); }
	FailedTransfer(const FailedTransfer&) = delete;
	FailedTransfer& operator=(const FailedTransfer&) = delete;
	FailedTransfer(FailedTransfer&&) = delete;
	FailedTransfer& operator=(FailedTransfer&&) = delete;

private:
	std::shared_ptr<RequestDelivery> delivery_; ///< See the constructor.
};

/// @brief Whether a character may be in an HTTP token (a header name).
/// @param c The character.
bool isTokenCharacter(char c) noexcept {
	if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;
	return std::string_view{"!#$%&'*+-.^_`|~"}.contains(c);
}

/// @brief Whether a header is one the transport sets, which a caller may not.
/// @param name The name.
// NOLINTNEXTLINE(bugprone-exception-escape): substr(0, n) never throws
bool isTransportHeader(std::string_view name) noexcept {
	static constexpr std::array<std::string_view, 8> names{"host", "content-length", "transfer-encoding", "connection",
	                                                       "upgrade", "expect", "te", "trailer"};
	if(std::ranges::any_of(names, [name](std::string_view forbidden) { return equalsIgnoringCase(name, forbidden); }))
		return true;
	const auto startsWith = [name](std::string_view prefix) {
		return name.size() >= prefix.size() && equalsIgnoringCase(name.substr(0, prefix.size()), prefix);
	};
	return startsWith("proxy-") || startsWith("sec-websocket-");
}

/// @brief Checks a request's method, timeout and body before it is sent.
/// @param request The request (its retries are clamped).
/// @return An error, or nullopt.
std::optional<std::string> checkRequest(network::Request& request) {
	static constexpr std::array<std::string_view, 5> methods{"GET", "POST", "PUT", "PATCH", "DELETE"};
	if(std::ranges::find(methods, request.method) == methods.end())
		return "'" + request.method.substr(0, 20) + "' isn't a method a request may use";
	if(request.timeout.count() < 1 || request.timeout > longestTimeout)
		return "A request's timeout is from 1 ms to an hour";
	if(request.maxBodyBytes == 0) return "A request's body limit can't be 0";
	request.retries = std::clamp(request.retries, 0, 5);
	request.retryAfter = std::clamp(request.retryAfter, std::chrono::milliseconds{1}, longestTimeout);
	return checkHeaders(request.headers);
}

} // namespace

std::optional<std::string> checkHeaders(const network::Headers& headers) {
	if(headers.size() > maxRequestHeaders) return "A request may have at most 100 headers";
	std::size_t bytes = 0;
	for(const auto& [name, value] : headers) {
		bytes += name.size() + value.size() + 4;
		if(name.empty() || !std::ranges::all_of(name, isTokenCharacter))
			return "'" + name.substr(0, 40) + "' isn't a header name";
		if(value.find_first_of(std::string_view{"\r\n\0", 3}) != std::string::npos)
			return "The header '" + name + "' has a line break in its value";
		if(isTransportHeader(name)) return "The header '" + name + "' is set by the transport, not by a request";
	}
	if(bytes > maxRequestHeaderBytes) return "A request's headers may be at most 64 KiB";
	return std::nullopt;
}

Client::Client(Executor& executor, ClientOptions options) : executor_(executor), options_(std::move(options)) {
	static_cast<void>(curl::start()); // if it failed, every request fails with "network"
	worker_ = std::make_shared<Worker>(executor_, options_);
}

Client::~Client() = default;

std::optional<std::string> Client::allow(std::string_view origin) { return allowlist_.allow(origin); }

std::optional<std::string> Client::check(std::string_view url) const { return allowlist_.check(url); }

std::unique_ptr<network::Transfer> Client::start(network::Request request, network::Handlers handlers) {
	auto delivery = std::make_shared<RequestDelivery>(std::move(handlers));
	const auto refuse = [&](std::string message) -> std::unique_ptr<network::Transfer> {
		postFailure(executor_, delivery, {"refused", std::move(message), 0, {}});
		return std::make_unique<FailedTransfer>(delivery);
	};
	auto url = parseUrl(request.url);
	if(!url || (url->scheme != "https" && url->scheme != "http"))
		return refuse("'" + request.url.substr(0, 200) + "' isn't an http or https URL");
	if(auto error = allowlist_.check(request.url)) return refuse(std::move(*error));
	if(auto error = checkRequest(request)) return refuse(std::move(*error));
	if(worker_->jobs() >= options_.maxWaiting) return refuse("Too many requests are waiting");
	const std::uint64_t id = worker_->submit(std::move(request), std::move(*url), delivery);
	return std::make_unique<HttpTransfer>(worker_, id, std::move(delivery));
}

std::unique_ptr<network::WebSocket> Client::connect(std::string url, network::Headers headers,
                                                    network::SocketHandlers handlers) {
	auto parsed = parseUrl(url);
	std::optional<std::string> error;
	if(!parsed || (parsed->scheme != "wss" && parsed->scheme != "ws"))
		error = "'" + url.substr(0, 200) + "' isn't a ws or wss URL";
	else if(auto refused = allowlist_.check(url))
		error = std::move(refused);
	else if(auto badHeader = checkHeaders(headers))
		error = std::move(badHeader);
	if(error) return refusedWebSocket(executor_, std::move(handlers), {"refused", std::move(*error), 0, {}});
	return openWebSocket(executor_, options_, std::move(*parsed), std::move(headers), std::move(handlers));
}

} // namespace affresco::net
