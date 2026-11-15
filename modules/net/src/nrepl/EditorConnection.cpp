/// @file
/// @brief The editor server's wire (EditorConnection.hpp): a connection's reader and writer threads, and the listener
/// that accepts connections and owns them.

#include "nrepl/EditorConnection.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace affresco::net {

namespace {

constexpr std::chrono::milliseconds writeTimeout{10000}; ///< The longest one message may take to send.
constexpr std::chrono::milliseconds idleSlice{60000};    ///< How long one receive() waits (close() ends it sooner).

/// @brief The limits a connection decodes its messages within.
/// @param options The server's options.
/// @return Them.
bencode::Limits limitsOf(const EditorServerOptions& options) {
	return {.maxBytes = options.maxMessageBytes, .maxDepth = options.maxDepth, .maxKeys = options.maxKeys};
}

/// @brief The most messages a connection may send at once, before the rate holds: one second's worth.
/// @param options The server's options.
/// @return It, at least 1.
double burstOf(const EditorServerOptions& options) { return std::max(options.messagesPerSecond, 1.0); }

} // namespace

EditorConnection::EditorConnection(Executor& executor, std::unique_ptr<tcp::Connection> connection,
                                   const EditorServerOptions& options, Ended ended)
    : executor_(executor), connection_(std::move(connection)), options_(options), ended_(std::move(ended)),
      decoder_(limitsOf(options)), tokens_(burstOf(options)), refilled_(Clock::now()) {}

EditorConnection::~EditorConnection() {
	*alive_ = false;
	{
		const std::scoped_lock lock(mutex_);
		stopping_ = true;
	}
	wake_.notify_all();
	room_.notify_all();
	connection_->close();
	if(writer_.joinable()) writer_.join();
	if(reader_.joinable()) reader_.join();
	tellClosed();
}

void EditorConnection::start(std::unique_ptr<EditorPeer> peer) {
	peer_ = std::move(peer);
	writer_ = std::thread([this] { writeLoop(); });
	reader_ = std::thread([this] { readLoop(); });
}

void EditorConnection::send(const bencode::Value& message) {
	if(!closing_) queue(bencode::encode(message));
}

void EditorConnection::close(std::string_view why) {
	if(closing_) return;
	closing_ = true;
	log(LogLevel::Info, "An editor connection from " + connection_->peer() + " closed: " + std::string(why));
	{
		const std::scoped_lock lock(mutex_);
		finishing_ = true;
	}
	wake_.notify_one();
}

void EditorConnection::readLoop() {
	std::string why;
	while(why.empty()) {
		{
			std::unique_lock lock(mutex_);
			room_.wait(lock, [this] { return stopping_ || heldBytes_ < options_.maxQueuedBytes; });
			if(stopping_) break;
		}
		const tcp::Received got = connection_->receive(input_, idleSlice);
		if(got == tcp::Received::TimedOut) continue;
		if(got != tcp::Received::Data) break;
		why = takeMessages();
	}
	if(!why.empty()) log(LogLevel::Warning, "An editor connection from " + connection_->peer() + " was closed: " + why);
	connection_->close();
	post([](EditorConnection& connection) { connection.finish(); });
}

std::string EditorConnection::takeMessages() {
	std::size_t start = 0; // the bytes taken by whole messages, erased once at the end
	std::string why;
	while(start < input_.size()) {
		// An editor sends dictionaries only: anything else (an HTTP request's "GET", stray text) ends at its first byte
		if(input_[start] != 'd') {
			why = "a message isn't a dictionary (an nREPL message starts with 'd')";
			break;
		}
		bencode::Decoded decoded = decoder_.read(std::string_view(input_).substr(start));
		if(decoded.status == bencode::Read::NeedMore) break;
		if(decoded.status == bencode::Read::Malformed) {
			why = std::move(decoded.why);
			break;
		}
		if(!admit()) {
			why =
			    "it sends more than " + std::to_string(std::llround(options_.messagesPerSecond)) + " messages a second";
			break;
		}
		start += decoded.used;
		{
			const std::scoped_lock lock(mutex_);
			heldBytes_ += decoded.used;
		}
		post([message = std::move(decoded.value), bytes = decoded.used](EditorConnection& connection) {
			{
				const std::scoped_lock lock(connection.mutex_);
				connection.heldBytes_ -= bytes;
			}
			connection.room_.notify_one();
			connection.deliver(message);
		});
	}
	input_.erase(0, start);
	return why;
}

bool EditorConnection::admit() {
	const auto now = Clock::now();
	const std::chrono::duration<double> elapsed = now - refilled_;
	tokens_ = std::min(burstOf(options_), tokens_ + elapsed.count() * options_.messagesPerSecond);
	refilled_ = now;
	if(tokens_ < 1) return false;
	tokens_ -= 1;
	return true;
}

void EditorConnection::queue(std::string bytes) {
	bool overflow = false;
	{
		const std::scoped_lock lock(mutex_);
		if(finishing_ || stopping_) return;
		if(queuedBytes_ + bytes.size() > options_.maxQueuedBytes) {
			overflow = true;
		} else {
			queuedBytes_ += bytes.size();
			output_.push_back(std::move(bytes));
		}
	}
	if(overflow) {
		closing_ = true;
		log(LogLevel::Warning, "An editor connection from " + connection_->peer() + " was closed: it reads too slowly");
		connection_->close(); // the reader ends, and posts finish()
		return;
	}
	wake_.notify_one();
}

void EditorConnection::writeLoop() {
	for(;;) {
		std::unique_lock lock(mutex_);
		wake_.wait(lock, [this] { return stopping_ || finishing_ || !output_.empty(); });
		if(stopping_) return;
		if(output_.empty()) { // close() asked, and everything before it is sent
			lock.unlock();
			connection_->close();
			return;
		}
		const std::string bytes = std::move(output_.front());
		output_.pop_front();
		queuedBytes_ -= bytes.size();
		lock.unlock();
		if(!connection_->send(bytes, writeTimeout)) {
			lock.lock();
			stopping_ = true; // the socket is gone: queue nothing more
			lock.unlock();
			connection_->close();
			return;
		}
	}
}

void EditorConnection::deliver(const bencode::Value& message) {
	if(!closing_ && peer_) peer_->receive(message);
}

void EditorConnection::finish() {
	tellClosed();
	const Ended ended = ended_; // a copy: the owner may destroy this connection, and ended_ with it
	if(ended) ended(*this);
}

void EditorConnection::tellClosed() {
	if(peerClosed_ || !peer_) return;
	peerClosed_ = true;
	peer_->closed();
}

void EditorConnection::post(std::function<void(EditorConnection&)> task) {
	executor_.post([this, alive = alive_, task = std::move(task)] {
		if(*alive) task(*this);
	});
}

EditorListener::EditorListener(Executor& executor, const EditorServerOptions& options, MakePeer makePeer)
    : executor_(executor), options_(options), makePeer_(std::move(makePeer)) {
	std::string error;
	listener_ = tcp::Listener::open("127.0.0.1", options.port, error);
	if(!listener_)
		throw std::runtime_error("Can't listen for an editor on 127.0.0.1:" + std::to_string(options.port) + ": " +
		                         error);
	acceptor_ = std::thread([this] { acceptLoop(); });
}

EditorListener::~EditorListener() {
	listener_->close();
	acceptor_.join();
	*alive_ = false;
	std::unordered_map<EditorConnection*, std::unique_ptr<EditorConnection>> open;
	{
		const std::scoped_lock lock(mutex_);
		open.swap(connections_);
	}
	// `open` goes here: each connection closes, joins its threads and tells its peer, outside the lock
}

std::size_t EditorListener::connections() {
	const std::scoped_lock lock(mutex_);
	return connections_.size();
}

void EditorListener::acceptLoop() {
	while(auto accepted = listener_->accept()) {
		const std::scoped_lock lock(mutex_);
		if(connections_.size() >= options_.maxConnections) {
			log(LogLevel::Warning, "An editor connection from " + accepted->peer() + " was refused: " +
			                           std::to_string(options_.maxConnections) + " are open already");
			continue; // `accepted` goes, and its socket closes
		}
		auto connection = std::make_unique<EditorConnection>(executor_, std::move(accepted), options_,
		                                                     [this](EditorConnection& ended) { remove(ended); });
		EditorConnection* key = connection.get();
		connections_.emplace(key, std::move(connection));
		executor_.post([this, alive = alive_, key] {
			if(*alive) begin(key);
		});
	}
}

void EditorListener::begin(EditorConnection* connection) {
	{
		const std::scoped_lock lock(mutex_);
		if(!connections_.contains(connection)) return;
	}
	// Only the main thread removes a connection, so it stays while the peer is made outside the lock
	std::unique_ptr<EditorPeer> peer;
	try {
		peer = makePeer_(*connection);
	} catch(const std::exception& error) {
		log(LogLevel::Error,
		    std::string("An editor connection was closed: its peer couldn't be made: ") + error.what());
		remove(*connection);
		return;
	}
	connection->start(std::move(peer));
}

void EditorListener::remove(EditorConnection& connection) {
	std::unique_ptr<EditorConnection> owned;
	{
		const std::scoped_lock lock(mutex_);
		const auto found = connections_.find(&connection);
		if(found == connections_.end()) return;
		owned = std::move(found->second);
		connections_.erase(found);
	}
	// `owned` goes here, outside the lock
}

} // namespace affresco::net
