/// @file
/// @brief SocketChannel and SocketServer. What differs between POSIX and Winsock is in SocketPlatform.hpp; the channel
/// and the server above it are written once.

#include "affresco/channel/Sockets.hpp"

#include "transports/Lines.hpp"
#include "transports/SocketPlatform.hpp"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>

namespace affresco::channel {

bool peerIsThisUser(SocketHandle socket) { return detail::sameUser(socket); }

/* SocketServer::Listener */

/// @brief A listening socket, and the wake-up that lets the destructor stop a thread blocked accepting on it.
///
/// accept() runs on the server's acceptor thread; stop() on the main thread.
class SocketServer::Listener {
public:
	/// @brief Creates the socket file and listens on it.
	/// @param path The socket file.
	/// @throws std::runtime_error If it can't be created.
	explicit Listener(const std::filesystem::path& path) : socket_(detail::listenAt(path)) {
		if(!wait_.watch(socket_)) {
			const std::string reason = detail::lastError();
			detail::closeSocket(socket_);
			throw std::runtime_error("AffrescoUI: could not listen at " + path.string() + ": " + reason);
		}
	}
	/// @brief Closes the socket (the file stays: the server removes it).
	~Listener() { detail::closeSocket(socket_); }

	Listener(const Listener&) = delete;
	Listener& operator=(const Listener&) = delete;

	/// @brief Waits for the next connection.
	/// @return The connection, or nothing once stop() was called.
	[[nodiscard]] std::optional<SocketHandle> accept() {
		while(wait_.waitForConnection())
			if(const SocketHandle connection = detail::acceptConnection(socket_); connection != detail::noSocket)
				return connection;
		return std::nullopt;
	}

	/// @brief Ends accept(), now or at its next call.
	void stop() noexcept { wait_.stop(); }

private:
	SocketHandle socket_;     ///< The listening socket.
	detail::AcceptWait wait_; ///< Wakes accept() for a connection or for stop().
};

/* SocketChannel */

std::unique_ptr<SocketChannel> SocketChannel::connect(Executor& executor, const std::filesystem::path& path) {
	const SocketHandle connection = detail::connectTo(path);
	if(connection == detail::noSocket) throw std::runtime_error("AffrescoUI: nothing is listening at " + path.string());
	return std::make_unique<SocketChannel>(executor, connection);
}

SocketChannel::SocketChannel(Executor& executor, SocketHandle socket, std::size_t maxQueuedBytes)
    : socket_(socket), maxQueuedBytes_(maxQueuedBytes) {
	detail::ignoreBrokenPipes(socket_);
	reader_ = std::thread([this, &executor] {
		const auto read = [this](char* data, std::size_t size) { return detail::receive(socket_, data, size); };
		detail::readLines(read, {executor, *this, alive_}, &open_);
	});
	writer_ = std::thread([this] { writeQueued(); });
}

SocketChannel::~SocketChannel() {
	*alive_ = false;
	{
		// What was sent before the channel went away still goes out, unless the peer doesn't read it
		std::unique_lock lock(queueMutex_);
		stopping_ = true;
		queueChanged_.notify_all();
		queueChanged_.wait_for(lock, std::chrono::seconds(1), [this] { return writerDone_; });
	}
	detail::shutdownSocket(socket_); // ends the reader's read, and a write the peer never took
	if(reader_.joinable()) reader_.join();
	if(writer_.joinable()) writer_.join();
	detail::closeSocket(socket_);
}

void SocketChannel::send(std::string_view message) {
	if(!open_) return;
	{
		const std::scoped_lock lock(queueMutex_);
		if(queuedBytes_ == 0 || queuedBytes_ + message.size() <= maxQueuedBytes_) {
			queuedBytes_ += message.size();
			queue_.emplace_back(message);
			queueChanged_.notify_all();
			return;
		}
	}
	// The peer reads slower than the app writes to it: it is cut off, not buffered without end
	log(LogLevel::Warning, "AffrescoUI: a peer fell more than " + std::to_string(maxQueuedBytes_ >> 20) +
	                           " MiB behind in reading: closing its channel");
	open_ = false;
	detail::shutdownSocket(socket_); // the reader sees the end and reports onClosed
}

void SocketChannel::writeQueued() {
	const auto write = [this](const char* data, std::size_t size) { return detail::transmit(socket_, data, size); };
	std::unique_lock lock(queueMutex_);
	while(true) {
		queueChanged_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
		if(queue_.empty()) break; // stopping, with everything written
		const std::string message = std::move(queue_.front());
		queue_.pop_front();
		lock.unlock();
		const bool written = open_ && detail::writeAll(write, message);
		lock.lock();
		queuedBytes_ -= message.size();
		if(!written) { // the peer went away, or was cut off: nothing more will reach it
			open_ = false;
			queue_.clear();
			queuedBytes_ = 0;
			break;
		}
	}
	writerDone_ = true;
	queueChanged_.notify_all();
}

/* SocketServer */

SocketServer::SocketServer(Executor& executor, std::filesystem::path path, Connected onConnection, PeerCheck checkPeer)
    : executor_(executor), path_(std::move(path)), onConnection_(std::move(onConnection)),
      checkPeer_(std::move(checkPeer)) {
	// A socket file left by a server that crashed is replaced; one that still answers is in use
	if(const SocketHandle existing = detail::connectTo(path_); existing != detail::noSocket) {
		detail::closeSocket(existing);
		throw std::runtime_error("AffrescoUI: another app is already listening at " + path_.string());
	}
	detail::removeSocketFile(path_);
	listener_ = std::make_unique<Listener>(path_);

	acceptor_ = std::thread([this] {
		while(const std::optional<SocketHandle> connection = listener_->accept()) {
			if(!admits(*connection)) {
				log(LogLevel::Warning, "AffrescoUI: refused a connection to " + path_.string() +
				                           ": the program on the other end runs as another user");
				detail::closeSocket(*connection);
				continue;
			}
			executor_.post([this, socket = *connection, alive = alive_] {
				if(*alive) onConnection_(std::make_unique<SocketChannel>(executor_, socket));
				else detail::closeSocket(socket);
			});
		}
	});
}

bool SocketServer::admits(SocketHandle connection) const noexcept {
	try {
		return checkPeer_ && checkPeer_(connection);
	} catch(...) { // the server's thread must not end: a check that fails refuses
		return false;
	}
}

SocketServer::~SocketServer() {
	*alive_ = false;
	listener_->stop();
	if(acceptor_.joinable()) acceptor_.join();
	listener_.reset();
	detail::removeSocketFile(path_);
}

} // namespace affresco::channel
