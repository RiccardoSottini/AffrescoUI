#pragma once

/// @file
/// @brief What differs between POSIX and Winsock under SocketChannel and SocketServer: small functions over a socket
/// and a socket file, and the wait that lets the server's acceptor stop. The channel and the server are written once,
/// on top of them (Sockets.cpp).

#include "affresco/channel/Sockets.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <cerrno>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace affresco::channel::detail {

#ifdef _WIN32
constexpr SocketHandle noSocket = INVALID_SOCKET; ///< What socket() and accept() return on failure.
#else
constexpr SocketHandle noSocket = -1; ///< What socket() and accept() return on failure.
#endif

/// @brief Starts the platform's sockets once for the process (Winsock); nothing on POSIX.
/// @throws std::runtime_error If they can't start.
void startSockets();

/// @brief Describes the last error of this thread.
/// @return The description.
[[nodiscard]] std::string lastError();

/// @brief Keeps a socket from the programs the host starts: a peer's connection or the listening socket must not
/// reach a shell or a debug adapter.
/// @param socket The socket, or noSocket.
/// @return It.
SocketHandle hostOnly(SocketHandle socket) noexcept;

/// @brief Closes a socket.
/// @param socket The socket.
void closeSocket(SocketHandle socket) noexcept;

/// @brief Ends both directions of a connection, and a read blocked on it in another thread.
/// @param socket The socket.
void shutdownSocket(SocketHandle socket) noexcept;

/// @brief Keeps writes to a peer that went away from killing the app.
/// @param socket The socket.
void ignoreBrokenPipes(SocketHandle socket);

/// @brief Receives from a connection.
/// @param socket The socket.
/// @param data Receives the bytes.
/// @param size The most to read.
/// @return How many bytes, 0 when the peer closed, less on an error.
[[nodiscard]] std::ptrdiff_t receive(SocketHandle socket, char* data, std::size_t size);

/// @brief Sends on a connection.
/// @param socket The socket.
/// @param data The bytes.
/// @param size How many.
/// @return How many bytes were sent, less than 1 on an error.
[[nodiscard]] std::ptrdiff_t transmit(SocketHandle socket, const char* data, std::size_t size);

/// @brief Whether the process at the other end of a connection runs as this process's user.
/// @param socket The connection.
/// @return false also when the operating system can't say.
[[nodiscard]] bool sameUser(SocketHandle socket);

/// @brief Removes a socket file, if there is one.
/// @param path The file.
void removeSocketFile(const std::filesystem::path& path);

/// @brief Accepts a connection that is waiting.
/// @param listener The listening socket.
/// @return The connection, or noSocket.
[[nodiscard]] SocketHandle acceptConnection(SocketHandle listener);

/// @brief Connects to a socket file.
/// @param path The file.
/// @return A connected socket, or noSocket.
/// @throws std::runtime_error If the path is too long.
[[nodiscard]] SocketHandle connectTo(const std::filesystem::path& path);

/// @brief Creates a socket file and listens on it; only this user may connect to it.
/// @param path The file (one left by a server that crashed must be removed first).
/// @return The listening socket.
/// @throws std::runtime_error If it can't be created.
[[nodiscard]] SocketHandle listenAt(const std::filesystem::path& path);

#ifdef _WIN32

/// @brief Waits on a listening socket for a connection, or for stop() from another thread: two Winsock events.
class AcceptWait {
public:
	AcceptWait() = default;
	/// @brief Closes the events.
	~AcceptWait() {
		for(WSAEVENT event : {connection_, stop_})
			if(event != WSA_INVALID_EVENT) WSACloseEvent(event);
	}
	AcceptWait(const AcceptWait&) = delete;
	AcceptWait& operator=(const AcceptWait&) = delete;

	/// @brief Starts watching a listening socket. It becomes non-blocking.
	/// @param listener The socket.
	/// @return false on an error (see lastError()).
	[[nodiscard]] bool watch(SocketHandle listener) {
		listener_ = listener;
		connection_ = WSACreateEvent();
		stop_ = WSACreateEvent();
		return connection_ != WSA_INVALID_EVENT && stop_ != WSA_INVALID_EVENT &&
		       WSAEventSelect(listener_, connection_, FD_ACCEPT) == 0;
	}

	/// @brief Blocks until a peer connects, or until stop().
	/// @return true when a connection may be waiting; false once stopped or on an error.
	[[nodiscard]] bool waitForConnection() {
		const WSAEVENT events[2] = {stop_, connection_}; // the lowest signalled index is reported: stopping wins
		if(WSAWaitForMultipleEvents(2, events, FALSE, WSA_INFINITE, FALSE) != WSA_WAIT_EVENT_0 + 1) return false;
		WSANETWORKEVENTS happened{};
		return WSAEnumNetworkEvents(listener_, connection_, &happened) == 0; // also resets the event
	}

	/// @brief Ends waitForConnection(), now or at its next call. Safe from any thread.
	void stop() noexcept { WSASetEvent(stop_); }

private:
	SocketHandle listener_ = noSocket;       ///< The socket watched.
	WSAEVENT connection_ = WSA_INVALID_EVENT; ///< Set by Winsock when a peer connects.
	WSAEVENT stop_ = WSA_INVALID_EVENT;       ///< Set by stop().
};

#else

/// @brief Waits on a listening socket for a connection, or for stop() from another thread: poll() and a pipe.
class AcceptWait {
public:
	AcceptWait() = default;
	/// @brief Closes the pipe.
	~AcceptWait() {
		for(const int fd : wake_)
			if(fd >= 0) close(fd);
	}
	AcceptWait(const AcceptWait&) = delete;
	AcceptWait& operator=(const AcceptWait&) = delete;

	/// @brief Starts watching a listening socket.
	/// @param listener The socket.
	/// @return false on an error (see lastError()).
	[[nodiscard]] bool watch(SocketHandle listener) {
		listener_ = listener;
		if(pipe(wake_.data()) != 0) return false;
		for(const int fd : wake_) hostOnly(fd);
		return true;
	}

	/// @brief Blocks until a peer connects, or until stop().
	/// @return true when a connection may be waiting; false once stopped, or on an error of poll() or the listener.
	[[nodiscard]] bool waitForConnection() {
		std::array<pollfd, 2> watched = {{{listener_, POLLIN, 0}, {wake_[0], POLLIN, 0}}};
		while(true) {
			if(poll(watched.data(), 2, -1) < 0) {
				if(errno == EINTR) continue; // a signal, not a reason to stop accepting
				return false;
			}
			if(watched[1].revents != 0) return false; // stop(), or the pipe broke
			// An error or a hang-up on the listener lasts, and poll() would report it again at once: stop, not spin
			if(watched[0].revents & (POLLERR | POLLHUP | POLLNVAL)) return false;
			if(watched[0].revents & POLLIN) return true;
		}
	}

	/// @brief Ends waitForConnection(), now or at its next call. Safe from any thread.
	void stop() noexcept {
		const char byte = 0;
		if(write(wake_[1], &byte, 1) != 1) shutdown(listener_, SHUT_RDWR); // no pipe: end poll() the hard way
	}

private:
	SocketHandle listener_ = noSocket; ///< The socket watched.
	std::array<int, 2> wake_ = {-1, -1}; ///< Written by stop().
};

#endif

} // namespace affresco::channel::detail
