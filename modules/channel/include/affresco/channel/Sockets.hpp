#pragma once

/// @file
/// @brief Local (Unix domain) socket channels: a server and its connections.

#include "affresco/core/Channel.hpp"
#include "affresco/core/Executor.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

namespace affresco::channel {

/// @brief A Channel over a connected local socket: one JSON object per line, both ways. POSIX only for now.
class SocketChannel final : public Channel {
public:
	/// @brief Connects to a SocketServer.
	/// @param executor Where messages are delivered (the main thread).
	/// @param path The server's socket.
	/// @return The channel.
	/// @throws std::runtime_error If nothing is listening there.
	[[nodiscard]] static std::unique_ptr<SocketChannel> connect(Executor& executor, const std::filesystem::path& path);

	/// @brief Wraps a connected socket and starts a thread reading it.
	/// @param executor Where messages are delivered (the main thread).
	/// @param socket A connected socket; the channel takes ownership.
	SocketChannel(Executor& executor, int socket);
	~SocketChannel() override;

	SocketChannel(const SocketChannel&) = delete;
	SocketChannel& operator=(const SocketChannel&) = delete;

	void send(std::string_view message) override;
	/// @brief Whether the other end is still connected.
	[[nodiscard]] bool isOpen() const noexcept { return open_; }

private:
	int socket_ = -1;               ///< The connection.
	std::atomic<bool> open_ = true; ///< See isOpen().
	std::thread reader_;            ///< Reads the socket.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

/// @brief Listens on a local socket, accessible only to this user. POSIX only for now.
///
/// Every connection becomes a SocketChannel, handed to `onConnection` on the main thread.
class SocketServer {
public:
	using Connected = std::function<void(std::unique_ptr<Channel>)>; ///< Receives each new connection.

	/// @brief Starts listening.
	/// @param executor Where connections are delivered (the main thread).
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
	Executor& executor_;         ///< Delivers connections on the main thread.
	std::filesystem::path path_; ///< See path().
	Connected onConnection_;     ///< Receives each new connection.
	int listener_ = -1;          ///< The listening socket.
	int wake_[2] = {-1, -1};     ///< Pipe written by the destructor to stop the accept loop.
	std::thread acceptor_;       ///< Accepts connections.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Guards tasks still queued after destruction.
};

} // namespace affresco::channel
