/// @file
/// @brief WebSocket connections in a browser (cmake/AffrescoUIWeb.cmake): the browser's WebSocket in place of
/// WebSocketClient.cpp, on the main thread, with the same limits (each message at most defaultMaxBodyBytes and valid
/// UTF-8, the send queue and the undelivered messages bounded).
///
/// A browser sends no headers of a page's own with the upgrade request. An "Authorization: Bearer" header (a remote
/// peer's token) becomes the first message, {"type": "auth", "token": ...}, which a peer server takes in its place:
/// the token never goes in the URL. Any other header refuses the connection.

#include "WebSocketClient.hpp"

#include "Delivery.hpp"
#include "Http.hpp"
#include "WebSocketFrames.hpp"

#include <emscripten.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>

// clang-format off

/// @brief Opens a browser WebSocket; what happens goes to the affresco_socket_* functions below, under its handle.
/// @param handle The connection's number (never 0).
/// @param url The ws:// or wss:// URL.
/// @param maxBytes The largest message it takes: a larger one closes it (1009).
/// @param timeout The milliseconds it may take to open.
EM_JS(void, affresco_socket_open, (unsigned handle, const char* url, double maxBytes, double timeout), {
	const sockets = globalThis.affrescoSockets ??= new Map();
	let socket;
	try {
		socket = new WebSocket(UTF8ToString(url));
	} catch (error) {
		setTimeout(() => _affresco_socket_closed(handle, 1006, 0), 0);
		return;
	}
	const state = { socket, timer: 0, tooLarge: false };
	sockets.set(handle, state);
	const live = () => sockets.get(handle) === state;
	state.timer = setTimeout(() => {
		if (!live() || socket.readyState !== WebSocket.CONNECTING) return;
		sockets.delete(handle);
		socket.close();
		_affresco_socket_closed(handle, 1006, 1);
	}, timeout);
	socket.onopen = () => {
		clearTimeout(state.timer);
		if (live()) _affresco_socket_opened(handle);
	};
	socket.onmessage = event => {
		if (!live()) return;
		if (typeof event.data !== "string" || lengthBytesUTF8(event.data) > maxBytes) {
			state.tooLarge = typeof event.data === "string";
			socket.close(state.tooLarge ? 1009 : 1003);
			return;
		}
		const pointer = stringToNewUTF8(event.data);
		const kept = _affresco_socket_message(handle, pointer);
		_free(pointer);
		if (!kept) {
			state.tooLarge = true;
			socket.close(1009);
		}
	};
	socket.onclose = event => {
		clearTimeout(state.timer);
		if (!live()) return;
		sockets.delete(handle);
		_affresco_socket_closed(handle, state.tooLarge ? 1009 : event.code, 0);
	};
});

/// @brief Sends a text message on an open connection.
/// @param handle The connection.
/// @param message The message.
/// @param maxQueued The most bytes the browser may hold unsent, this message included.
/// @return 0 if it was refused (closed, or too much is queued).
EM_JS(int, affresco_socket_send, (unsigned handle, const char* message, double maxQueued), {
	const state = globalThis.affrescoSockets && globalThis.affrescoSockets.get(handle);
	if (!state || state.socket.readyState !== WebSocket.OPEN) return 0;
	const text = UTF8ToString(message);
	if (state.socket.bufferedAmount + lengthBytesUTF8(text) > maxQueued) return 0;
	state.socket.send(text);
	return 1;
});

/// @brief Closes a connection: nothing more is told about it.
/// @param handle The connection.
EM_JS(void, affresco_socket_close, (unsigned handle), {
	const sockets = globalThis.affrescoSockets;
	const state = sockets && sockets.get(handle);
	if (!state) return;
	sockets.delete(handle);
	clearTimeout(state.timer);
	state.socket.close(1000);
});

EM_JS_DEPS(affresco_socket, "$UTF8ToString,$stringToNewUTF8,$lengthBytesUTF8,free");

// clang-format on

namespace affresco::net {

namespace {

/// The longest a connection may take to open.
constexpr double openTimeoutMilliseconds = 30000;

/// @brief What the browser's callbacks reach: a connection's state.
class SocketEvents {
public:
	SocketEvents() = default;
	SocketEvents(const SocketEvents&) = delete;
	SocketEvents& operator=(const SocketEvents&) = delete;
	SocketEvents(SocketEvents&&) = delete;
	SocketEvents& operator=(SocketEvents&&) = delete;
	virtual ~SocketEvents() = default;

	/// @brief The connection opened.
	virtual void opened() = 0;
	/// @brief A message came.
	/// @param message The message (UTF-8, bounded).
	/// @return false if too many wait to be handled: the connection closes.
	virtual bool message(std::string message) = 0;
	/// @brief The connection closed.
	/// @param code The close code (1000 is clean; 1009 a message over the limit).
	/// @param timedOut Whether it closed because it didn't open in time.
	virtual void closed(int code, bool timedOut) = 0;
};

/// @brief The open connections, by handle.
/// @return The map.
std::unordered_map<std::uint32_t, SocketEvents*>& sockets() {
	static std::unordered_map<std::uint32_t, SocketEvents*> map;
	return map;
}

/// @brief Posts a connection's end: closed is called once, then nothing more.
/// @param executor The main thread.
/// @param delivery The delivery.
/// @param failure Why, or nullopt for a clean close.
void postClosed(Executor& executor, const std::shared_ptr<SocketDelivery>& delivery,
                std::optional<network::Failure> failure) {
	executor.post([delivery, failure = std::move(failure)]() mutable {
		if(delivery->cancelled) return;
		auto closed = std::move(delivery->handlers.closed);
		delivery->cancel();
		if(closed) closed(std::move(failure));
	});
}

/// @brief A connection that was refused before it started.
class RefusedSocket final : public network::WebSocket {
public:
	/// @brief Holds the delivery its end was posted to.
	/// @param delivery The delivery.
	explicit RefusedSocket(std::shared_ptr<SocketDelivery> delivery) noexcept : delivery_(std::move(delivery)) {}
	/// @brief Stops the end from being delivered, if it hasn't been.
	~RefusedSocket() override { delivery_->cancel(); }
	RefusedSocket(const RefusedSocket&) = delete;
	RefusedSocket& operator=(const RefusedSocket&) = delete;
	RefusedSocket(RefusedSocket&&) = delete;
	RefusedSocket& operator=(RefusedSocket&&) = delete;

	bool send(std::string_view) override { return false; }

private:
	std::shared_ptr<SocketDelivery> delivery_; ///< See the constructor.
};

/// @brief A connection through the browser's WebSocket.
class BrowserSocket final : public network::WebSocket, SocketEvents {
public:
	/// @brief Starts connecting.
	/// @param executor The main thread.
	/// @param options The client's options.
	/// @param url The URL.
	/// @param token The token to send first, or empty.
	/// @param handlers Told what happens.
	BrowserSocket(Executor& executor, const ClientOptions& options, const Url& url, std::string token,
	              network::SocketHandlers handlers)
	    : executor_(executor), options_(options), token_(std::move(token)),
	      delivery_(std::make_shared<SocketDelivery>(std::move(handlers))) {
		static std::uint32_t lastHandle = 0;
		if(++lastHandle == 0) ++lastHandle; // 0 is no handle
		handle_ = lastHandle;
		sockets()[handle_] = this;
		const std::string address = url.scheme + "://" + url.authority() + url.target;
		affresco_socket_open(handle_, address.c_str(), static_cast<double>(network::defaultMaxBodyBytes),
		                     openTimeoutMilliseconds);
	}
	/// @brief Closes the connection; no handler is called afterwards.
	~BrowserSocket() override {
		delivery_->cancel();
		sockets().erase(handle_);
		affresco_socket_close(handle_);
	}
	BrowserSocket(const BrowserSocket&) = delete;
	BrowserSocket& operator=(const BrowserSocket&) = delete;
	BrowserSocket(BrowserSocket&&) = delete;
	BrowserSocket& operator=(BrowserSocket&&) = delete;

	bool send(std::string_view message) override {
		if(closed_ || message.size() > network::defaultMaxBodyBytes || !isUtf8(message)) return false;
		if(!open_) {
			if(queuedBytes_ + message.size() > options_.maxSendQueueBytes) return false;
			queue_.emplace_back(message);
			queuedBytes_ += message.size();
			return true;
		}
		const std::string text(message);
		return affresco_socket_send(handle_, text.c_str(), static_cast<double>(options_.maxSendQueueBytes)) != 0;
	}

private:
	void opened() override {
		open_ = true;
		if(!token_.empty()) {
			const std::string auth = nlohmann::json{{"type", "auth"}, {"token", token_}}.dump();
			token_.clear();
			static_cast<void>(
			    affresco_socket_send(handle_, auth.c_str(), static_cast<double>(options_.maxSendQueueBytes)));
		}
		for(const auto& message : queue_)
			static_cast<void>(
			    affresco_socket_send(handle_, message.c_str(), static_cast<double>(options_.maxSendQueueBytes)));
		queue_.clear();
		queuedBytes_ = 0;
		executor_.post([delivery = delivery_] {
			if(delivery->cancelled || !delivery->handlers.opened) return;
			auto opened = delivery->handlers.opened; // a copy: the handler may destroy the connection
			opened();
		});
	}

	bool message(std::string message) override {
		if(!isUtf8(message)) return false;
		if(delivery_->undelivered >= options_.maxUndelivered) return false;
		++delivery_->undelivered;
		executor_.post([delivery = delivery_, message = std::move(message)]() mutable {
			--delivery->undelivered;
			if(delivery->cancelled || !delivery->handlers.message) return;
			auto handle = delivery->handlers.message; // a copy: the handler may destroy the connection
			handle(std::move(message));
		});
		return true;
	}

	void closed(int code, bool timedOut) override {
		closed_ = true;
		sockets().erase(handle_);
		std::optional<network::Failure> failure;
		if(timedOut) failure = network::Failure{"timeout", "The connection didn't open in time.", 0, {}};
		else if(code == 1009) failure = network::Failure{"tooLarge", "A message is larger than allowed.", 0, {}};
		else if(code == 1003) failure = network::Failure{"protocol", "The server sent a binary message.", 0, {}};
		else if(!open_) failure = network::Failure{"network", "The connection couldn't open.", 0, {}};
		else if(code != 1000)
			failure = network::Failure{"network", "The connection closed (" + std::to_string(code) + ").", 0, {}};
		postClosed(executor_, delivery_, std::move(failure));
	}

	Executor& executor_;                       ///< See the constructor.
	ClientOptions options_;                    ///< See the constructor.
	std::string token_;                        ///< Sent first once open, then forgotten.
	std::shared_ptr<SocketDelivery> delivery_; ///< Where everything goes.
	std::uint32_t handle_ = 0;                 ///< The browser's connection.
	std::deque<std::string> queue_;            ///< Messages sent before it opened.
	std::size_t queuedBytes_ = 0;              ///< Their size.
	bool open_ = false;                        ///< It opened.
	bool closed_ = false;                      ///< It ended: send() refuses.
};

/// @brief The connection of a handle, if it is still open.
/// @param handle The handle.
/// @return It, or nullptr.
SocketEvents* socketOf(std::uint32_t handle) {
	auto it = sockets().find(handle);
	return it == sockets().end() ? nullptr : it->second;
}

} // namespace

} // namespace affresco::net

/// @brief A browser WebSocket opened.
/// @param handle The connection.
extern "C" EMSCRIPTEN_KEEPALIVE void affresco_socket_opened(std::uint32_t handle) {
	if(auto* socket = affresco::net::socketOf(handle)) socket->opened();
}

/// @brief A browser WebSocket received a text message.
/// @param handle The connection.
/// @param message The message.
/// @return 0 if it can't be taken: the connection closes.
extern "C" EMSCRIPTEN_KEEPALIVE int affresco_socket_message(std::uint32_t handle, const char* message) {
	auto* socket = affresco::net::socketOf(handle);
	return socket && socket->message(message) ? 1 : 0;
}

/// @brief A browser WebSocket closed (or couldn't open).
/// @param handle The connection.
/// @param code Its close code.
/// @param timedOut Non-zero if it didn't open in time.
extern "C" EMSCRIPTEN_KEEPALIVE void affresco_socket_closed(std::uint32_t handle, int code, int timedOut) {
	if(auto* socket = affresco::net::socketOf(handle)) socket->closed(code, timedOut != 0);
}

namespace affresco::net {

std::unique_ptr<network::WebSocket> openWebSocket(Executor& executor, const ClientOptions& options, Url url,
                                                  network::Headers headers, network::SocketHandlers handlers) {
	std::string token;
	for(const auto& [name, value] : headers) {
		constexpr std::string_view bearer = "Bearer ";
		if(equalsIgnoringCase(name, "authorization") && value.starts_with(bearer) && token.empty()) {
			token = value.substr(bearer.size());
			continue;
		}
		return refusedWebSocket(executor, std::move(handlers),
		                        {"refused", "A browser can't send the header '" + name + "' with a WebSocket.", 0, {}});
	}
	return std::make_unique<BrowserSocket>(executor, options, url, std::move(token), std::move(handlers));
}

std::unique_ptr<network::WebSocket> refusedWebSocket(Executor& executor, network::SocketHandlers handlers,
                                                     network::Failure failure) {
	auto delivery = std::make_shared<SocketDelivery>(std::move(handlers));
	postClosed(executor, delivery, std::move(failure));
	return std::make_unique<RefusedSocket>(std::move(delivery));
}

} // namespace affresco::net
