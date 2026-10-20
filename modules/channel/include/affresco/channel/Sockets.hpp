#pragma once

/// @file
/// @brief Local (Unix domain) socket channels: a server and its connections. POSIX, and Windows 10 1803 or later
/// (Winsock's AF_UNIX).

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace affresco::channel {

#ifdef _WIN32
using SocketHandle = std::uintptr_t; ///< A Winsock `SOCKET`, without including winsock2.h in this header.
#else
using SocketHandle = int; ///< A socket's file descriptor.
#endif

/// The most a SocketChannel holds for a peer that hasn't read it yet, by default: a peer that falls further behind
/// is disconnected. Pictures of the screen ("render") are the largest messages, a few MiB.
inline constexpr std::size_t defaultSendQueueBytes = std::size_t{64} << 20;

/// @brief Whether the program at the other end of a connected local socket runs as this process's user.
///
/// Asks the operating system: `SO_PEERCRED` on Linux, `getpeereid` on macOS and the BSDs, and on Windows the peer's
/// process id (`SIO_AF_UNIX_GETPEERPID`), whose token's user is compared with this process's.
/// @param socket A connected local socket.
/// @return false also when the user can't be read (e.g. a Windows process this user may not inspect).
[[nodiscard]] bool peerIsThisUser(SocketHandle socket);

/// @brief A Channel over a connected local socket: one JSON object per line, both ways.
///
/// Its reader thread only reads the socket and posts what arrives to the Executor; its writer thread only writes
/// what send() queued. Everything else, the signals included, happens on the main thread, which never waits for the
/// peer: a peer that falls more than the send queue's limit behind is disconnected.
class SocketChannel final : public Channel {
public:
	/// @brief Connects to a SocketServer.
	/// @param executor Where messages are delivered (the main thread).
	/// @param path The server's socket.
	/// @return The channel.
	/// @throws std::runtime_error If nothing is listening there.
	[[nodiscard]] static std::unique_ptr<SocketChannel> connect(Executor& executor, const std::filesystem::path& path);

	/// @brief Wraps a connected socket and starts the threads that read and write it.
	/// @param executor Where messages are delivered (the main thread); it must outlive the channel.
	/// @param socket A connected socket; the channel takes ownership.
	/// @param maxQueuedBytes The most it holds unsent; more closes the channel. A single larger message is still
	/// queued when nothing else waits.
	SocketChannel(Executor& executor, SocketHandle socket, std::size_t maxQueuedBytes = defaultSendQueueBytes);
	/// @brief Sends what is still queued (waiting at most a second for a peer that doesn't read), ends both threads
	/// and closes the socket; messages received but not yet delivered are dropped.
	~SocketChannel() override;

	SocketChannel(const SocketChannel&) = delete;
	SocketChannel& operator=(const SocketChannel&) = delete;

	/// @brief Queues a message for the writer thread; returns at once.
	/// @param message The message, without a line break.
	void send(std::string_view message) override;
	/// @brief Whether the other end is still connected.
	[[nodiscard]] bool isOpen() const noexcept { return open_; }

private:
	/// @brief The writer thread: writes queued messages until the channel stops, or a write fails.
	void writeQueued();

	SocketHandle socket_;           ///< The connection.
	std::size_t maxQueuedBytes_;    ///< See the constructor.
	std::atomic<bool> open_ = true; ///< See isOpen().
	std::mutex queueMutex_;         ///< Guards the queue and the fields after it.
	std::condition_variable queueChanged_; ///< A message was queued, the writer finished, or the channel stops.
	std::deque<std::string> queue_;        ///< Messages not yet written, oldest first.
	std::size_t queuedBytes_ = 0;          ///< Their size, with the one being written.
	bool stopping_ = false;                ///< Set by the destructor: write what is queued, then end.
	bool writerDone_ = false;              ///< The writer thread has ended.
	std::thread reader_;                   ///< Reads the socket.
	std::thread writer_;                   ///< Writes the queue (see writeQueued()).
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

/// @brief Listens on a local socket that only this user may open.
///
/// Every connection becomes a SocketChannel, handed to `onConnection` on the main thread. On POSIX the socket file
/// is made `0600`; on Windows its access list gets one entry, for this user, and inherits nothing from the folder.
/// Each connection is also checked when it is accepted (peerIsThisUser() by default): one from a program of another
/// user is refused, closed and logged.
class SocketServer {
public:
	using Connected = std::function<void(std::unique_ptr<Channel>)>; ///< Receives each new connection.
	/// Decides, on the server's own thread, whether to accept a connection (true) or to close it.
	using PeerCheck = std::function<bool(SocketHandle)>;

	/// @brief Starts listening.
	/// @param executor Where connections are delivered (the main thread); it must outlive the server.
	/// @param path The socket file to create.
	/// @param onConnection Receives each new connection.
	/// @param checkPeer Accepts or refuses each connection before it becomes a channel; it runs on the server's
	/// thread, and one that throws refuses. The default accepts programs of this user only.
	/// @throws std::runtime_error If the socket can't be created, e.g. another server is listening there.
	SocketServer(Executor& executor, std::filesystem::path path, Connected onConnection,
	             PeerCheck checkPeer = peerIsThisUser);
	/// @brief Stops listening and removes the socket file.
	~SocketServer();

	SocketServer(const SocketServer&) = delete;
	SocketServer& operator=(const SocketServer&) = delete;

	/// @brief The socket file.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	class Listener; ///< The listening socket, and the wake-up that stops the accept loop (Sockets.cpp).

	/// @brief Runs the peer check on a new connection.
	/// @param connection The connection.
	/// @return Whether to accept it: false also when the check throws.
	bool admits(SocketHandle connection) const noexcept;

	Executor& executor_;                 ///< Delivers connections on the main thread.
	std::filesystem::path path_;         ///< See path().
	Connected onConnection_;             ///< Receives each new connection.
	PeerCheck checkPeer_;                ///< Accepts or refuses each connection.
	std::unique_ptr<Listener> listener_; ///< Waits for connections; stopped by the destructor.
	std::thread acceptor_;               ///< Accepts connections.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

} // namespace affresco::channel
