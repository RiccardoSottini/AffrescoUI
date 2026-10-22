/// @file
/// @brief One remote peer's connection on the server side (RFC 0003).

#include "peers/PeerConnection.hpp"

#include "peers/PeerHandshake.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>

namespace affresco::net {

namespace {

/// The longest the reader waits to write while the connection opens (the 101 answer, a pong, a refusal).
constexpr std::chrono::milliseconds openingWriteTimeout{1000};
/// The longest the writer waits for room for one queued write: a peer that reads nothing for longer is closed.
constexpr std::chrono::milliseconds writeTimeout{10000};
/// The longest one receive() waits when there is no deadline (close() ends it sooner).
constexpr std::chrono::milliseconds idleSlice{60000};

} // namespace

PeerConnection::PeerConnection(std::shared_ptr<const Shared> shared, std::unique_ptr<tcp::Connection> connection,
                               Opened opened)
    : shared_(std::move(shared)), connection_(std::move(connection)), opened_(std::move(opened)),
      frames_(true, shared_->options.maxMessageBytes) {
	writer_ = std::thread([this] { writeLoop(); });
	reader_ = std::thread([this] { readLoop(); });
}

PeerConnection::~PeerConnection() {
	*alive_ = false;
	{
		std::lock_guard lock(mutex_);
		stopping_ = true;
	}
	wake_.notify_all();
	connection_->close();
	writer_.join();
	reader_.join();
	--*shared_->open;
}

void PeerConnection::send(std::string_view message) { queue(encodeFrame(Opcode::Text, message)); }

void PeerConnection::readLoop() {
	auto author = open(Clock::now() + shared_->options.authTimeout);
	const bool accepted = author.has_value();
	opened_(*this, std::move(author));
	if(!accepted) return;
	bool reading = true;
	while(reading) {
		while(auto event = frames_.next()) {
			if(!(reading = handle(std::move(*event)))) break;
		}
		if(reading && receive(std::nullopt) != tcp::Received::Data) reading = false;
		feedFrames();
	}
	post([](PeerConnection& connection) { connection.onClosed(); });
}

std::optional<std::string> PeerConnection::open(Clock::time_point deadline) {
	std::optional<std::size_t> end;
	while(!(end = peer::headEnd(input_))) {
		if(input_.size() > peer::maxHandshakeBytes) return refuse(431, "the request headers are too large");
		const tcp::Received got = receive(deadline);
		if(got == tcp::Received::TimedOut) return refuse(408, "no upgrade request came in time");
		if(got != tcp::Received::Data) return std::nullopt; // gone before asking anything
	}
	const peer::Upgrade upgrade = peer::parseUpgrade(std::string_view(input_).substr(0, *end));
	if(upgrade.status != 0) return refuse(upgrade.status, upgrade.reason);
	if(upgrade.hasOrigin && !peer::originAllowed(upgrade.origin, shared_->options.allowedOrigins))
		return refuse(403, "this origin isn't allowed");
	std::optional<std::string> author;
	if(!upgrade.token.empty() && !(author = verify(upgrade.token))) return refuse(401, "the token was refused");
	if(!connection_->send(peer::acceptResponse(upgrade.key), openingWriteTimeout)) return std::nullopt;
	input_.erase(0, *end);
	feedFrames();
	return author ? author : authenticate(deadline);
}

std::optional<std::string> PeerConnection::authenticate(Clock::time_point deadline) {
	std::optional<std::string> author;
	for(;;) {
		while(auto event = frames_.next())
			if(authEvent(*event, author)) return author;
		const tcp::Received got = receive(deadline);
		if(got == tcp::Received::TimedOut) return closeWith(1008, "no token came in time");
		if(got != tcp::Received::Data) return std::nullopt;
		feedFrames();
	}
}

bool PeerConnection::authEvent(const FrameEvent& event, std::optional<std::string>& author) {
	switch(event.kind) {
	case FrameEvent::Kind::Message: {
		const auto token = peer::authToken(event.data);
		if(!token) {
			closeWith(1008, R"(the first message must be {"type": "auth", "token": "..."})");
		} else if(!(author = verify(*token))) {
			closeWith(1008, "the token was refused");
		}
		return true;
	}
	case FrameEvent::Kind::Ping:
		static_cast<void>(connection_->send(encodeFrame(Opcode::Pong, event.data), openingWriteTimeout));
		return false;
	case FrameEvent::Kind::Pong: return false;
	case FrameEvent::Kind::Close:
		static_cast<void>(connection_->send(encodeFrame(Opcode::Close, closePayload(1000, "")), openingWriteTimeout));
		connection_->close();
		return true;
	case FrameEvent::Kind::Error: closeWith(event.code, event.data); return true;
	}
	return true;
}

std::optional<std::string> PeerConnection::verify(const std::string& token) {
	try {
		auto author = shared_->verifier->verify(token);
		if(author && !author->empty()) return author;
	} catch(...) { // a verifier that throws refuses the peer
	}
	return std::nullopt;
}

std::optional<std::string> PeerConnection::refuse(int status, const std::string& reason) {
	log(LogLevel::Warning, "A remote peer from " + connection_->peer() + " was refused: " + reason);
	static_cast<void>(connection_->send(peer::refusalResponse(status, reason), openingWriteTimeout));
	connection_->close();
	return std::nullopt;
}

std::optional<std::string> PeerConnection::closeWith(std::uint16_t code, const std::string& reason) {
	log(LogLevel::Warning, "A remote peer from " + connection_->peer() + " was refused: " + reason);
	static_cast<void>(connection_->send(encodeFrame(Opcode::Close, closePayload(code, reason)), openingWriteTimeout));
	connection_->close();
	return std::nullopt;
}

tcp::Received PeerConnection::receive(std::optional<Clock::time_point> deadline) {
	for(;;) {
		std::chrono::milliseconds wait = idleSlice;
		if(deadline) {
			wait = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - Clock::now());
			if(wait.count() <= 0) return tcp::Received::TimedOut;
		}
		const tcp::Received got = connection_->receive(input_, wait);
		if(got != tcp::Received::TimedOut || deadline) return got;
	}
}

void PeerConnection::feedFrames() {
	frames_.feed(input_);
	input_.clear();
}

bool PeerConnection::handle(FrameEvent event) {
	switch(event.kind) {
	case FrameEvent::Kind::Message:
		post([message = std::move(event.data)](PeerConnection& connection) { connection.onMessage(message); });
		return true;
	case FrameEvent::Kind::Ping: queue(encodeFrame(Opcode::Pong, event.data)); return true;
	case FrameEvent::Kind::Pong: return true;
	case FrameEvent::Kind::Close: queue(encodeFrame(Opcode::Close, closePayload(1000, "")), true); return false;
	case FrameEvent::Kind::Error:
		log(LogLevel::Warning, "A remote peer from " + connection_->peer() + " was closed: " + event.data);
		queue(encodeFrame(Opcode::Close, closePayload(event.code, event.data)), true);
		return false;
	}
	return false;
}

void PeerConnection::queue(std::string bytes, bool last) {
	bool overflow = false;
	{
		std::lock_guard lock(mutex_);
		if(finishing_ || stopping_) return;
		if(queuedBytes_ + bytes.size() > shared_->options.maxQueuedBytes) {
			overflow = true;
		} else {
			queuedBytes_ += bytes.size();
			output_.push_back(std::move(bytes));
			finishing_ = last;
		}
	}
	if(overflow) {
		log(LogLevel::Warning, "A remote peer from " + connection_->peer() + " was closed: it reads too slowly");
		connection_->close(); // the reader ends, and posts onClosed
		return;
	}
	wake_.notify_one();
}

void PeerConnection::writeLoop() {
	for(;;) {
		std::unique_lock lock(mutex_);
		wake_.wait(lock, [this] { return stopping_ || finishing_ || !output_.empty(); });
		if(stopping_) return;
		if(output_.empty()) { // finishing, and all sent
			lock.unlock();
			connection_->close();
			return;
		}
		std::string bytes = std::move(output_.front());
		output_.pop_front();
		queuedBytes_ -= bytes.size();
		lock.unlock();
		if(!connection_->send(bytes, writeTimeout)) {
			connection_->close();
			return;
		}
	}
}

void PeerConnection::post(std::function<void(PeerConnection&)> task) {
	shared_->executor.post([this, alive = alive_, task = std::move(task)] {
		if(*alive) task(*this);
	});
}

} // namespace affresco::net
