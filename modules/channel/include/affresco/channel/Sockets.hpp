#pragma once

/// @file
/// @brief Local (Unix domain) socket channels: a server and its connections. POSIX, and Windows 10 1803 or later
/// (Winsock's AF_UNIX).

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

namespace affresco::channel {

#if defined(_WIN32)
using SocketHandle = std::uintptr_t; ///< A Winsock `SOCKET`, without including winsock2.h in this header.
#else
using SocketHandle = int; ///< A socket's file descriptor.
#endif

/// @brief A Channel over a connected local socket: one JSON object per line, both ways.
///
/// Its reader thread only reads the socket and posts what arrives to the Executor; everything else, the signals
/// included, happens on the main thread.
class SocketChannel final : public Channel {
public:
	/// @brief Connects to a SocketServer.
	/// @param executor Where messages are delivered (the main thread).
	/// @param path The server's socket.
	/// @return The channel.
	/// @throws std::runtime_error If nothing is listening there.
	[[nodiscard]] static std::unique_ptr<SocketChannel> connect(Executor& executor, const std::filesystem::path& path);

	/// @brief Wraps a connected socket and starts a thread reading it.
	/// @param executor Where messages are delivered (the main thread); it must outlive the channel.
	/// @param socket A connected socket; the channel takes ownership.
	SocketChannel(Executor& executor, SocketHandle socket);
	/// @brief Ends the reader thread and closes the socket; messages still queued are dropped.
	~SocketChannel() override;

	SocketChannel(const SocketChannel&) = delete;
	SocketChannel& operator=(const SocketChannel&) = delete;

	void send(std::string_view message) override;
	/// @brief Whether the other end is still connected.
	[[nodiscard]] bool isOpen() const noexcept { return open_; }

private:
	SocketHandle socket_;           ///< The connection.
	std::atomic<bool> open_ = true; ///< See isOpen().
	std::thread reader_;            ///< Reads the socket.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

/// @brief Listens on a local socket that only this user may open.
///
/// Every connection becomes a SocketChannel, handed to `onConnection` on the main thread. On POSIX the socket file
/// is made `0600`; on Windows its access list gets one entry, for this user, and inherits nothing from the folder.
class SocketServer {
public:
	using Connected = std::function<void(std::unique_ptr<Channel>)>; ///< Receives each new connection.

	/// @brief Starts listening.
	/// @param executor Where connections are delivered (the main thread); it must outlive the server.
	/// @param path The socket file to create.
	/// @param onConnection Receives each new connection.
	/// @throws std::runtime_error If the socket can't be created, e.g. another server is listening there.
	SocketServer(Executor& executor, std::filesystem::path path, Connected onConnection);
	/// @brief Stops listening and removes the socket file.
	~SocketServer();

	SocketServer(const SocketServer&) = delete;
	SocketServer& operator=(const SocketServer&) = delete;

	/// @brief The socket file.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	class Listener; ///< The listening socket, and the wake-up that stops the accept loop (Sockets.cpp).

	Executor& executor_;                 ///< Delivers connections on the main thread.
	std::filesystem::path path_;         ///< See path().
	Connected onConnection_;             ///< Receives each new connection.
	std::unique_ptr<Listener> listener_; ///< Waits for connections; stopped by the destructor.
	std::thread acceptor_;               ///< Accepts connections.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

} // namespace affresco::channel
