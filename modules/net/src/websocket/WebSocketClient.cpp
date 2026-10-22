/// @file
/// @brief The client side of WebSocket connections: connecting through libcurl (CONNECT_ONLY), the opening handshake,
/// then a loop that reads frames, answers pings and sends the queued messages, masked.

#include "websocket/WebSocketClient.hpp"

#include "Random.hpp"

#include "http/Curl.hpp"
#include "http/Delivery.hpp"
#include "http/Http.hpp"
#include "websocket/WebSocketFrames.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>

namespace affresco::net {

namespace {

using Clock = std::chrono::steady_clock; ///< The clock of deadlines.

/// The longest a connection may take to open (TCP, TLS and the handshake).
constexpr std::chrono::milliseconds openTimeout{30000};
/// The longest the loop sleeps without looking at its queue.
constexpr std::chrono::milliseconds longestWait{250};
/// The longest the closing handshake may hold the thread when the owner closes.
constexpr std::chrono::milliseconds closeTimeout{250};

/// @brief Posts a connection's end (from any thread): closed is called once, then nothing more.
/// @param executor The main thread.
/// @param delivery The delivery.
/// @param failure Why, or nullopt for a clean close.
void postClosed(Executor& executor, const std::shared_ptr<SocketDelivery>& delivery, std::optional<network::Failure> failure) {
	// NOLINTNEXTLINE(readability-redundant-lambda-parameter-list): `] mutable` without () is C++23
	executor.post([delivery, failure = std::move(failure)]() mutable {
		if(delivery->cancelled) return;
		const auto closed = std::move(delivery->handlers.closed);
		delivery->cancel();
		if(closed) closed(std::move(failure));
	});
}

/// @brief libcurl's progress callback while connecting: aborts once the owner closes.
/// @param user The stop token.
/// @return Non-zero to abort.
// NOLINTNEXTLINE(readability-non-const-parameter): curl's CURLOPT_XFERINFOFUNCTION type
int onConnectProgress(void* user, curl_off_t /*downloadTotal*/, curl_off_t /*downloaded*/, curl_off_t /*uploadTotal*/,
                      curl_off_t /*uploaded*/) {
	return static_cast<const std::stop_token*>(user)->stop_requested() ? 1 : 0;
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

	bool send(std::string_view /*message*/) override { return false; }

private:
	std::shared_ptr<SocketDelivery> delivery_; ///< See the constructor.
};

/// @brief A connection to a WebSocket server, on its own thread.
class ClientSocket final : public network::WebSocket {
public:
	/// @brief Starts connecting.
	/// @param executor The main thread.
	/// @param options The client's options.
	/// @param url The URL.
	/// @param headers Extra headers of the upgrade request.
	/// @param handlers Told what happens.
	ClientSocket(Executor& executor, ClientOptions options, Url url, network::Headers headers,
	             network::SocketHandlers handlers)
	    : executor_(executor), options_(std::move(options)), url_(std::move(url)), headers_(std::move(headers)),
	      delivery_(std::make_shared<SocketDelivery>(std::move(handlers))), multi_(curl_multi_init()),
	      thread_([this](const std::stop_token& stop) { run(stop); }) {}
	/// @brief Closes the connection (a close frame, best effort); no handler is called afterwards.
	~ClientSocket() override {
		delivery_->cancel();
		closed_ = true;
		thread_.request_stop();
		if(multi_) curl_multi_wakeup(multi_.get());
		if(thread_.joinable()) thread_.join();
	}
	ClientSocket(const ClientSocket&) = delete;
	ClientSocket& operator=(const ClientSocket&) = delete;
	ClientSocket(ClientSocket&&) = delete;
	ClientSocket& operator=(ClientSocket&&) = delete;

	bool send(std::string_view message) override {
		if(closed_ || message.size() > network::defaultMaxBodyBytes || !isUtf8(message)) return false;
		{
			const std::scoped_lock lock(mutex_);
			if(queuedBytes_ + message.size() > options_.maxSendQueueBytes) return false;
			queue_.emplace_back(message);
			queuedBytes_ += message.size();
		}
		curl_multi_wakeup(multi_.get());
		return true;
	}

private:
	/// @brief The thread: connects, shakes hands, then runs the loop until the end.
	/// @param stop Asks it to end.
	void run(const std::stop_token& stop);
	/// @brief Connects through libcurl.
	/// @param stop Aborts the connection.
	/// @return A failure, or nullopt.
	std::optional<network::Failure> connect(const std::stop_token& stop);
	/// @brief Sends the upgrade request and reads the answer.
	/// @param stop Asks it to end.
	/// @return A failure, or nullopt.
	std::optional<network::Failure> handshake(const std::stop_token& stop);
	/// @brief Checks the server's answer to the upgrade request.
	/// @param block Its header block.
	/// @param key The key sent.
	/// @return A failure, or nullopt.
	static std::optional<network::Failure> checkAnswer(const HeaderBlock& block, const std::string& key);
	/// @brief Reads, answers and sends until the connection ends.
	/// @param stop Asks it to end.
	/// @return Why it ended: nullopt for a clean close (or the owner closing).
	std::optional<network::Failure> loop(const std::stop_token& stop);
	/// @brief Reads what has arrived.
	/// @return A failure, or nullopt.
	std::optional<network::Failure> receive();
	/// @brief Handles one event the reader found.
	/// @param event The event.
	/// @param end Receives the connection's end, if this was it.
	/// @return Whether the connection goes on.
	bool handle(FrameEvent event, std::optional<network::Failure>& end);
	/// @brief Passes a message to the main thread, unless too many are waiting there.
	/// @param message The message, taken only if passed.
	/// @return Whether it was passed.
	bool deliver(std::string& message);
	/// @brief Answers the server's close frame and notes why the connection ended.
	/// @param code The server's close code, 0 if it gave none.
	/// @param end Receives a failure unless the close was normal.
	void answerClose(std::uint16_t code, std::optional<network::Failure>& end);
	/// @brief The upgrade request.
	/// @param key Its Sec-WebSocket-Key.
	[[nodiscard]] std::string upgradeRequest(const std::string& key) const;
	/// @brief Sends what the socket takes, encoding the queued messages once the bytes before them are out.
	/// @return false on an error.
	bool flush();
	/// @brief Moves the queued messages into the output, encoded.
	/// @return Whether there were any.
	bool encodeQueue();
	/// @brief Sends bytes before a deadline.
	/// @param bytes The bytes.
	/// @param deadline When to give up.
	/// @return Whether they all went.
	bool sendAll(std::string_view bytes, Clock::time_point deadline);
	/// @brief Waits for the socket, a wakeup or a timeout.
	/// @param write Whether to wait for room to write too.
	/// @param timeout The longest wait.
	void wait(bool write, std::chrono::milliseconds timeout);
	/// @brief A masking key from the system's generator: RFC 6455 section 10.3 asks that the server can't predict the
	/// next one, which a seeded engine's keys, all seen on the wire, would let it do.
	static std::array<std::uint8_t, 4> mask();

	Executor& executor_;                       ///< See the constructor.
	ClientOptions options_;                    ///< See the constructor.
	Url url_;                                  ///< See the constructor.
	network::Headers headers_;                 ///< See the constructor.
	std::shared_ptr<SocketDelivery> delivery_; ///< Where everything goes.
	curl::Multi multi_;                        ///< Only for curl_multi_poll and its wakeup.
	curl::Easy easy_;                          ///< The connection (thread only).
	curl_socket_t socket_ = CURL_SOCKET_BAD;   ///< Its socket (thread only).
	FrameReader reader_{false, network::defaultMaxBodyBytes}; ///< Reads the server's frames (thread only).
	std::string output_;                       ///< Bytes encoded and not yet sent (thread only).
	std::mutex mutex_;                         ///< Guards queue_ and queuedBytes_.
	std::deque<std::string> queue_;            ///< Messages sent and not yet encoded.
	std::size_t queuedBytes_ = 0;              ///< Their size.
	std::atomic<bool> closed_{false};          ///< The connection ended, or the owner closed it: send() refuses.
	std::jthread thread_;                      ///< Last: it starts once everything else is ready.
};

void ClientSocket::run(const std::stop_token& stop) {
	std::optional<network::Failure> end = multi_ ? connect(stop) : network::Failure{"network", "Out of memory.", 0, {}};
	if(!end) end = handshake(stop);
	if(!end && !stop.stop_requested()) {
		executor_.post([delivery = delivery_] {
			if(delivery->cancelled || !delivery->handlers.opened) return;
			const auto opened = delivery->handlers.opened; // a copy: the handler may destroy the connection
			opened();
		});
		end = loop(stop);
	}
	closed_ = true;
	if(!stop.stop_requested()) postClosed(executor_, delivery_, std::move(end));
}

std::optional<network::Failure> ClientSocket::connect(const std::stop_token& stop) {
	easy_.reset(curl_easy_init());
	CURL* easy = easy_.get();
	if(!easy) return network::Failure{"network", "Out of memory.", 0, {}};
	curl::applyCommonOptions(easy, options_);
	const std::string address = (url_.secure() ? "https://" : "http://") + url_.authority() + "/";
	curl_easy_setopt(easy, CURLOPT_URL, address.c_str());
	curl_easy_setopt(easy, CURLOPT_CONNECT_ONLY, 1L);
	curl_easy_setopt(easy, CURLOPT_HTTPPROXYTUNNEL, 1L); // through a proxy, a tunnel (only used if there is one)
	curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(openTimeout.count()));
	curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, onConnectProgress);
	curl_easy_setopt(easy, CURLOPT_XFERINFODATA, const_cast<void*>(static_cast<const void*>(&stop)));
	const CURLcode code = curl_easy_perform(easy);
	if(code == CURLE_OPERATION_TIMEDOUT) return network::Failure{"timeout", "The connection took too long.", 0, {}};
	if(code != CURLE_OK) return network::Failure{"network", curl_easy_strerror(code), 0, {}};
	if(curl_easy_getinfo(easy, CURLINFO_ACTIVESOCKET, &socket_) != CURLE_OK || socket_ == CURL_SOCKET_BAD)
		return network::Failure{"network", "The connection has no socket.", 0, {}};
	return std::nullopt;
}

std::optional<network::Failure> ClientSocket::handshake(const std::stop_token& stop) {
	std::string nonce(16, '\0');
	fillRandom(std::as_writable_bytes(std::span(nonce)));
	const std::string key = base64(nonce);
	const auto deadline = Clock::now() + openTimeout;
	if(!sendAll(upgradeRequest(key), deadline))
		return network::Failure{"network", "The upgrade request couldn't be sent.", 0, {}};
	std::string answer;
	std::size_t end = std::string::npos;
	while((end = answer.find("\r\n\r\n")) == std::string::npos) {
		if(stop.stop_requested()) return std::nullopt;
		if(answer.size() > maxResponseHeaderBytes) return network::Failure{"tooLarge", "The handshake is too large.", 0, {}};
		if(Clock::now() >= deadline) return network::Failure{"timeout", "The handshake took too long.", 0, {}};
		std::array<char, 4096> buffer{};
		std::size_t received = 0;
		const CURLcode code = curl_easy_recv(easy_.get(), buffer.data(), buffer.size(), &received);
		if(code == CURLE_AGAIN) {
			wait(false, longestWait);
			continue;
		}
		if(code != CURLE_OK || received == 0) return network::Failure{"network", "The server closed the connection.", 0, {}};
		answer.append(buffer.data(), received);
	}
	auto block = parseHeaderBlock(std::string_view{answer}.substr(0, end + 4));
	if(!block) return network::Failure{"protocol", "The handshake's answer is malformed.", 0, {}};
	if(auto failure = checkAnswer(*block, key)) return failure;
	reader_.feed(std::string_view{answer}.substr(end + 4)); // frames sent right after the answer
	return std::nullopt;
}

std::string ClientSocket::upgradeRequest(const std::string& key) const {
	std::string request = "GET " + url_.target + " HTTP/1.1\r\nHost: " + url_.authority() +
	                      "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key +
	                      "\r\nSec-WebSocket-Version: 13\r\n";
	for(const auto& [name, value] : headers_) request += name + ": " + value + "\r\n";
	return request + "\r\n";
}

std::optional<network::Failure> ClientSocket::checkAnswer(const HeaderBlock& block, const std::string& key) {
	const int status = parseStatusLine(block.firstLine).value_or(0);
	if(status != 101) return network::Failure{"status", "The server answered " + std::to_string(status) + ".", status, {}};
	const auto upgrade = findHeader(block.headers, "upgrade");
	const auto connection = findHeader(block.headers, "connection");
	const auto accept = findHeader(block.headers, "sec-websocket-accept");
	if(!upgrade || !equalsIgnoringCase(*upgrade, "websocket") || !connection || !hasToken(*connection, "upgrade") ||
	   !accept || *accept != acceptKey(key))
		return network::Failure{"protocol", "The server's answer isn't a WebSocket handshake.", 0, {}};
	return std::nullopt;
}

std::optional<network::Failure> ClientSocket::loop(const std::stop_token& stop) {
	while(!stop.stop_requested()) {
		if(auto failure = receive()) return failure;
		std::optional<network::Failure> end;
		while(auto event = reader_.next())
			if(!handle(std::move(*event), end)) return end;
		if(!flush()) return network::Failure{"network", "The connection was lost.", 0, {}};
		wait(!output_.empty(), longestWait);
	}
	// The owner closed: say goodbye, briefly
	output_ += encodeFrame(Opcode::Close, closePayload(1000, ""), mask());
	static_cast<void>(sendAll(output_, Clock::now() + closeTimeout));
	return std::nullopt;
}

std::optional<network::Failure> ClientSocket::receive() {
	for(;;) {
		std::array<char, 16384> buffer{};
		std::size_t received = 0;
		const CURLcode code = curl_easy_recv(easy_.get(), buffer.data(), buffer.size(), &received);
		if(code == CURLE_AGAIN) return std::nullopt;
		if(code != CURLE_OK) return network::Failure{"network", curl_easy_strerror(code), 0, {}};
		if(received == 0) return network::Failure{"network", "The server closed the connection.", 0, {}};
		reader_.feed({buffer.data(), received});
		if(reader_.buffered() > network::defaultMaxBodyBytes + 16) return std::nullopt; // read the frames first
	}
}

bool ClientSocket::handle(FrameEvent event, std::optional<network::Failure>& end) {
	switch(event.kind) {
	case FrameEvent::Kind::Message:
		if(deliver(event.data)) return true;
		event = {FrameEvent::Kind::Error, "Messages arrive faster than they are handled.", 1008};
		break;
	case FrameEvent::Kind::Ping:
		// A server that pings without reading would grow the output; RFC 6455 section 5.5.3 lets an endpoint skip
		// pongs it hasn't been able to send
		if(output_.size() < options_.maxSendQueueBytes) output_ += encodeFrame(Opcode::Pong, event.data, mask());
		return true;
	case FrameEvent::Kind::Pong: return true;
	case FrameEvent::Kind::Close:
		answerClose(event.code, end);
		return false;
	case FrameEvent::Kind::Error: break;
	}
	output_ += encodeFrame(Opcode::Close, closePayload(event.code, event.data), mask());
	static_cast<void>(sendAll(output_, Clock::now() + closeTimeout));
	end = network::Failure{event.code == 1009 ? "tooLarge" : "protocol", event.data, 0, {}};
	return false;
}

bool ClientSocket::deliver(std::string& message) {
	if(delivery_->undelivered > options_.maxUndelivered) return false;
	++delivery_->undelivered;
	// NOLINTNEXTLINE(readability-redundant-lambda-parameter-list): `] mutable` without () is C++23
	executor_.post([delivery = delivery_, message = std::move(message)]() mutable {
		--delivery->undelivered;
		if(delivery->cancelled || !delivery->handlers.message) return;
		const auto handleMessage = delivery->handlers.message; // a copy: the handler may destroy the connection
		handleMessage(std::move(message));
	});
	return true;
}

void ClientSocket::answerClose(std::uint16_t code, std::optional<network::Failure>& end) {
	output_ += encodeFrame(Opcode::Close, code == 0 ? std::string{} : closePayload(code, ""), mask());
	static_cast<void>(sendAll(output_, Clock::now() + closeTimeout));
	if(code != 0 && code != 1000)
		end = network::Failure{"network", "The server closed the connection (" + std::to_string(code) + ").", 0, {}};
}

bool ClientSocket::flush() {
	for(;;) {
		// Messages wait in the queue, which send() bounds, until the bytes before them are out: taking them sooner
		// would let a server that stops reading grow the output without limit
		if(output_.empty() && !encodeQueue()) return true;
		std::size_t sent = 0;
		const CURLcode code = curl_easy_send(easy_.get(), output_.data(), output_.size(), &sent);
		if(code == CURLE_AGAIN) return true;
		if(code != CURLE_OK) return false;
		output_.erase(0, sent);
	}
}

bool ClientSocket::encodeQueue() {
	std::deque<std::string> messages;
	{
		const std::scoped_lock lock(mutex_);
		messages.swap(queue_);
		queuedBytes_ = 0;
	}
	for(const auto& message : messages) output_ += encodeFrame(Opcode::Text, message, mask());
	return !messages.empty();
}

bool ClientSocket::sendAll(std::string_view bytes, Clock::time_point deadline) {
	while(!bytes.empty()) {
		std::size_t sent = 0;
		const CURLcode code = curl_easy_send(easy_.get(), bytes.data(), bytes.size(), &sent);
		if(code == CURLE_OK) {
			bytes.remove_prefix(sent);
			continue;
		}
		const auto now = Clock::now();
		if(code != CURLE_AGAIN || now >= deadline) return false;
		wait(true, std::min(longestWait, std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)));
	}
	return true;
}

void ClientSocket::wait(bool write, std::chrono::milliseconds timeout) {
	curl_waitfd socket{socket_, static_cast<short>(CURL_WAIT_POLLIN | (write ? CURL_WAIT_POLLOUT : 0)), 0};
	int ready = 0;
	curl_multi_poll(multi_.get(), &socket, 1, static_cast<int>(timeout.count()), &ready);
}

std::array<std::uint8_t, 4> ClientSocket::mask() {
	std::array<std::uint8_t, 4> key{};
	fillRandom(std::as_writable_bytes(std::span(key)));
	return key;
}

} // namespace

std::unique_ptr<network::WebSocket> openWebSocket(Executor& executor, const ClientOptions& options, Url url,
                                                  network::Headers headers, network::SocketHandlers handlers) {
	return std::make_unique<ClientSocket>(executor, options, std::move(url), std::move(headers), std::move(handlers));
}

std::unique_ptr<network::WebSocket> refusedWebSocket(Executor& executor, network::SocketHandlers handlers,
                                                     network::Failure failure) {
	auto delivery = std::make_shared<SocketDelivery>(std::move(handlers));
	postClosed(executor, delivery, std::move(failure));
	return std::make_unique<RefusedSocket>(std::move(delivery));
}

} // namespace affresco::net
