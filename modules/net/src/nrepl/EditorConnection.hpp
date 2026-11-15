#pragma once

/// @file
/// @brief The editor server's wire: one connection's threads (EditorConnection) and the listener that accepts them
/// (EditorListener). Neither gives a connection any meaning: each message goes to the connection's EditorPeer.
///
/// Threads and ownership: the main thread owns the listener and every connection, and is the only one that calls a
/// peer. The listener's acceptor thread accepts and refuses; each connection's reader thread reads and decodes, and its
/// writer thread sends what is queued. They reach the main thread only through the Executor, and the main thread never
/// waits for them except when it destroys their owner (each wait is bounded by tcp's poll slice).

#include "EditorLink.hpp"
#include "Tcp.hpp"

#include "affresco/core/Executor.hpp"
#include "affresco/net/EditorServer.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace affresco::net {

/// @brief One editor's connection: a reader thread that decodes its messages, a writer thread that sends its answers,
/// and the EditorLink its peer answers through. Owned by the main thread.
///
/// Every message must start with `d` (a dictionary), decode within the server's limits and come within its rate;
/// otherwise the connection closes, and the log says why (never the bytes, which hold the person's code).
class EditorConnection final : public EditorLink {
public:
	/// Called on the main thread once the connection has ended and its peer heard closed(): the owner may destroy it.
	using Ended = std::function<void(EditorConnection& connection)>;

	/// @brief Takes an accepted connection; nothing runs until start().
	/// @param executor Runs the connection's deliveries on the main thread; outlives the connection.
	/// @param connection The socket.
	/// @param options The limits; outlive the connection.
	/// @param ended Called when it ends.
	EditorConnection(Executor& executor, std::unique_ptr<tcp::Connection> connection,
	                 const EditorServerOptions& options, Ended ended);
	/// @brief Closes the socket, waits for both threads, tells the peer closed() if it wasn't told, and destroys it.
	/// On the main thread: nothing the connection posted runs afterwards.
	~EditorConnection() override;
	EditorConnection(const EditorConnection&) = delete;
	EditorConnection& operator=(const EditorConnection&) = delete;
	EditorConnection(EditorConnection&&) = delete;
	EditorConnection& operator=(EditorConnection&&) = delete;

	/// @brief Starts reading and writing, on the main thread.
	/// @param peer What the messages go to.
	void start(std::unique_ptr<EditorPeer> peer);

	/// @brief Queues a message and returns at once (main thread); over maxQueuedBytes waiting, the connection closes.
	/// @param message The message.
	void send(const bencode::Value& message) override;
	/// @brief Closes once what was queued has been sent (main thread); the peer receives nothing more.
	/// @param why Why, for the log.
	void close(std::string_view why) override;

private:
	using Clock = std::chrono::steady_clock; ///< For the rate.

	/// @brief The reader thread: reads, decodes and posts each message, then posts the end. It stops reading while
	/// the main thread holds maxQueuedBytes of messages, so a main thread that is busy doesn't pile them up.
	void readLoop();
	/// @brief Takes every whole message out of the bytes received (reader thread).
	/// @return Why the connection must close, or "" to read on.
	std::string takeMessages();
	/// @brief Counts one more message against the rate (reader thread).
	/// @return Whether it is within messagesPerSecond.
	bool admit();
	/// @brief Queues bytes for the writer (main thread).
	/// @param bytes The bytes.
	void queue(std::string bytes);
	/// @brief The writer thread: sends what is queued, in order, then closes the socket if close() asked.
	void writeLoop();
	/// @brief Gives one message to the peer (main thread), unless the connection is closing.
	/// @param message The message.
	void deliver(const bencode::Value& message);
	/// @brief Tells the peer the connection closed, then the owner (main thread). Touches nothing after `ended_`.
	void finish();
	/// @brief Tells the peer closed(), once.
	void tellClosed();
	/// @brief Runs a task on the main thread, unless the connection is gone by then (from either thread).
	/// @param task The task.
	void post(std::function<void(EditorConnection&)> task);

	Executor& executor_;                          ///< See the constructor.
	std::unique_ptr<tcp::Connection> connection_; ///< The socket; any thread may close() it.
	const EditorServerOptions& options_;          ///< See the constructor.
	Ended ended_;                                 ///< See the constructor.
	std::unique_ptr<EditorPeer> peer_;            ///< What messages go to (main thread).
	bool closing_ = false;                        ///< close() was called, or the queue overflowed (main thread).
	bool peerClosed_ = false;                     ///< The peer heard closed() (main thread).

	std::string input_;          ///< Bytes received and not yet a whole message (reader thread).
	bencode::Reader decoder_;    ///< Decodes input_ as it grows (reader thread).
	double tokens_ = 0;          ///< Messages it may still send now (reader thread).
	Clock::time_point refilled_; ///< When tokens_ was last brought up to date (reader thread).

	std::mutex mutex_;               ///< Guards the queue and the flags below.
	std::condition_variable wake_;   ///< Wakes the writer.
	std::condition_variable room_;   ///< Wakes the reader: the main thread took messages it held.
	std::size_t heldBytes_ = 0;      ///< Bytes of messages posted to the main thread, not yet delivered.
	std::deque<std::string> output_; ///< Bytes to send, in order.
	std::size_t queuedBytes_ = 0;    ///< Their size.
	bool finishing_ = false;         ///< Close the socket once the queue is sent.
	bool stopping_ = false;          ///< The destructor runs, or the writer failed: queue nothing more.

	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
	std::thread writer_;                                         ///< Runs writeLoop().
	std::thread reader_;                                         ///< Runs readLoop(); started last.
};

/// @brief Listens on 127.0.0.1 and owns the connections it accepts, at most maxConnections at once, each with the peer
/// a factory makes: EditorServer gives it the nREPL protocol's, a test its own. Owned by the main thread.
class EditorListener {
public:
	/// Makes the peer of a new connection, on the main thread.
	using MakePeer = std::function<std::unique_ptr<EditorPeer>(EditorLink& link)>;

	/// @brief Listens and starts the acceptor thread.
	/// @param executor Runs deliveries on the main thread; outlives the listener.
	/// @param options The port and the limits; outlive the listener.
	/// @param makePeer Makes each connection's peer.
	/// @throws std::runtime_error If it can't listen.
	EditorListener(Executor& executor, const EditorServerOptions& options, MakePeer makePeer);
	/// @brief Stops listening, waits for the acceptor, and destroys every connection (main thread).
	~EditorListener();
	EditorListener(const EditorListener&) = delete;
	EditorListener& operator=(const EditorListener&) = delete;
	EditorListener(EditorListener&&) = delete;
	EditorListener& operator=(EditorListener&&) = delete;

	/// @brief The port listened on. @return It.
	[[nodiscard]] std::uint16_t port() const noexcept { return listener_->port(); }
	/// @brief Connections open now, started or about to be. @return How many.
	[[nodiscard]] std::size_t connections();

private:
	/// @brief The acceptor thread: accepts, refuses over maxConnections, and posts each new connection's start.
	void acceptLoop();
	/// @brief Gives an accepted connection its peer and starts it (main thread).
	/// @param connection The connection, if it is still held.
	void begin(EditorConnection* connection);
	/// @brief Destroys an ended connection (main thread).
	/// @param connection The connection.
	void remove(EditorConnection& connection);

	Executor& executor_;                      ///< See the constructor.
	const EditorServerOptions& options_;      ///< See the constructor.
	MakePeer makePeer_;                       ///< See the constructor.
	std::unique_ptr<tcp::Listener> listener_; ///< The listening socket.
	std::mutex mutex_;                        ///< Guards connections_ (the acceptor adds, the main thread removes).
	std::unordered_map<EditorConnection*, std::unique_ptr<EditorConnection>> connections_; ///< Every open connection.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
	std::thread acceptor_;                                       ///< Runs acceptLoop().
};

} // namespace affresco::net
