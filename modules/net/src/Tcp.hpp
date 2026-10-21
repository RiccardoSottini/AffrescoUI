#pragma once

/// @file
/// @brief Plain TCP for the WebSocket server (and the tests' local servers): a listener and its connections, with
/// every wait bounded by a timeout and ended by close() from another thread. What differs between POSIX and Winsock
/// is in one block of Tcp.cpp ("The platform").

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace affresco::net::tcp {

/// @brief A socket's handle: an int on POSIX, a SOCKET on Windows.
using Handle = std::intptr_t;

/// @brief What receive() found.
enum class Received {
	Data,     ///< Bytes were read.
	Closed,   ///< The peer closed its side.
	TimedOut, ///< Nothing came before the timeout.
	Failed,   ///< An error, or close() was called.
};

/// @brief One connection. Thread-safe: one thread reads, another writes, any thread may close().
class Connection {
public:
	/// @brief Takes a connected socket.
	/// @param handle The socket.
	/// @param peer Its address, e.g. "127.0.0.1".
	Connection(Handle handle, std::string peer) noexcept : handle_(handle), peer_(std::move(peer)) {}
	/// @brief Closes the socket.
	~Connection();
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;
	Connection(Connection&&) = delete;
	Connection& operator=(Connection&&) = delete;

	/// @brief Connects to a host's port (the tests' clients).
	/// @param host A name or an address, e.g. "127.0.0.1".
	/// @param port The port.
	/// @return The connection, or nullptr.
	[[nodiscard]] static std::unique_ptr<Connection> open(const std::string& host, std::uint16_t port);

	/// @brief Waits for bytes, at most a timeout.
	/// @param into Receives them (appended), at most 64 KiB at a time.
	/// @param timeout The longest wait.
	/// @return What happened.
	[[nodiscard]] Received receive(std::string& into, std::chrono::milliseconds timeout);
	/// @brief Sends every byte, waiting at most a timeout for room.
	/// @param bytes The bytes.
	/// @param timeout The longest the whole send may take.
	/// @return false on an error, a timeout or after close().
	[[nodiscard]] bool send(std::string_view bytes, std::chrono::milliseconds timeout);
	/// @brief Ends the connection: waits in receive() and send() return at once (within their poll slice).
	void close() noexcept;
	/// @brief The peer's address, without its port.
	[[nodiscard]] const std::string& peer() const noexcept { return peer_; }

private:
	/// @brief Waits until the socket can be read or written, in short slices that notice close().
	/// @param write Whether to wait for room to write.
	/// @param timeout The longest wait.
	/// @return 1 when ready, 0 on a timeout, -1 on an error or after close().
	int wait(bool write, std::chrono::milliseconds timeout);

	Handle handle_;                   ///< The socket.
	std::string peer_;                ///< See peer().
	std::atomic<bool> closed_{false}; ///< close() was called.
};

/// @brief A listening socket. accept() runs on one thread; close() may come from any.
class Listener {
public:
	/// @brief Listens on an address.
	/// @param host "127.0.0.1", "::1", "0.0.0.0" or "::" (an address, not a name).
	/// @param port The port, or 0 for any free one (see port()).
	/// @param error Receives why it failed.
	/// @return The listener, or nullptr.
	[[nodiscard]] static std::unique_ptr<Listener> open(const std::string& host, std::uint16_t port,
	                                                    std::string& error);
	/// @brief Takes a listening socket.
	/// @param handle The socket.
	/// @param port The port it listens on.
	Listener(Handle handle, std::uint16_t port) noexcept : handle_(handle), port_(port) {}
	/// @brief Closes the socket.
	~Listener();
	Listener(const Listener&) = delete;
	Listener& operator=(const Listener&) = delete;
	Listener(Listener&&) = delete;
	Listener& operator=(Listener&&) = delete;

	/// @brief Waits for the next connection, until close().
	/// @return It, or nullptr once closed or on an error that lasts.
	[[nodiscard]] std::unique_ptr<Connection> accept();
	/// @brief Ends accept() (within its poll slice), now or at its next call.
	void close() noexcept { closed_ = true; }
	/// @brief The port it listens on.
	[[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
	Handle handle_;                   ///< The socket.
	std::uint16_t port_;              ///< See port().
	std::atomic<bool> closed_{false}; ///< close() was called.
};

/// @brief Splits "host:port" ("127.0.0.1:7420", "[::1]:7420").
/// @param address The address.
/// @return The host (without brackets) and the port, or nullopt.
[[nodiscard]] std::optional<std::pair<std::string, std::uint16_t>> splitAddress(std::string_view address);

} // namespace affresco::net::tcp
