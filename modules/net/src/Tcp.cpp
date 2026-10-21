/// @file
/// @brief Plain TCP connections and listeners. Waits poll the socket in short slices, so close() from another thread
/// ends them on every platform without signals or wake pipes.

#include "Tcp.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <climits>
#include <system_error>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace affresco::net::tcp {

namespace {

/// The longest one poll waits before looking at close() again.
constexpr std::chrono::milliseconds pollSlice{100};

/* The platform */

#ifdef _WIN32

using Native = SOCKET;                            ///< A socket as the system has it.
constexpr Native noSocket = INVALID_SOCKET;       ///< What socket() and accept() return on failure.
using AddressLength = int;                        ///< The type of an address's length.

/// @brief Starts Winsock once for the process (never cleaned up: sockets may live until it exits).
/// @return Whether it started.
bool startSockets() {
	static const bool started = [] {
		WSADATA data{};
		return WSAStartup(MAKEWORD(2, 2), &data) == 0;
	}();
	return started;
}

/// @brief Describes the last error of this thread.
std::string lastError() { return std::system_category().message(WSAGetLastError()); }

/// @brief Closes a socket.
/// @param socket The socket.
void closeNative(Native socket) noexcept { closesocket(socket); }

/// @brief Waits for a socket.
/// @param socket The socket.
/// @param events POLLIN or POLLOUT.
/// @param milliseconds The longest wait.
/// @return Above 0 when ready, 0 on a timeout, below 0 on an error.
int pollNative(Native socket, short events, int milliseconds) {
	WSAPOLLFD watched{socket, events, 0};
	return WSAPoll(&watched, 1, milliseconds);
}

/// @brief Receives into a buffer.
/// @param socket The socket.
/// @param data The buffer.
/// @param size Its size.
/// @return How many bytes, 0 when the peer closed, below 0 on an error.
std::ptrdiff_t receiveNative(Native socket, char* data, std::size_t size) {
	return recv(socket, data, static_cast<int>(size), 0);
}

/// @brief Sends what fits.
/// @param socket The socket.
/// @param data The bytes.
/// @param size How many (at most INT_MAX are sent by one call).
/// @return How many were sent, below 1 on an error.
std::ptrdiff_t sendNative(Native socket, const char* data, std::size_t size) {
	return send(socket, data, static_cast<int>(std::min<std::size_t>(size, INT_MAX)), 0);
}

/// @brief Nothing to do: Winsock reports a peer that went away as an error from send(), with no signal.
void ignoreBrokenPipes(Native) noexcept {}

#else

using Native = int;                  ///< A socket as the system has it.
constexpr Native noSocket = -1;      ///< What socket() and accept() return on failure.
using AddressLength = socklen_t;     ///< The type of an address's length.

/// @brief Nothing to start on POSIX.
bool startSockets() { return true; }

/// @brief Describes the last error of this thread.
std::string lastError() { return std::strerror(errno); }

/// @brief Closes a socket.
/// @param socket The socket.
void closeNative(Native socket) noexcept { ::close(socket); }

/// @brief Waits for a socket.
/// @param socket The socket.
/// @param events POLLIN or POLLOUT.
/// @param milliseconds The longest wait.
/// @return Above 0 when ready, 0 on a timeout (or a signal), below 0 on an error.
int pollNative(Native socket, short events, int milliseconds) {
	pollfd watched{socket, events, 0};
	const int result = poll(&watched, 1, milliseconds);
	return result < 0 && errno == EINTR ? 0 : result;
}

/// @brief Receives into a buffer.
/// @param socket The socket.
/// @param data The buffer.
/// @param size Its size.
/// @return How many bytes, 0 when the peer closed, below 0 on an error.
std::ptrdiff_t receiveNative(Native socket, char* data, std::size_t size) { return recv(socket, data, size, 0); }

/// @brief Sends what fits.
/// @param socket The socket.
/// @param data The bytes.
/// @param size How many.
/// @return How many were sent, below 1 on an error.
std::ptrdiff_t sendNative(Native socket, const char* data, std::size_t size) {
#ifdef MSG_NOSIGNAL
	return ::send(socket, data, size, MSG_NOSIGNAL);
#else
	return ::send(socket, data, size, 0);
#endif
}

/// @brief Keeps writes to a peer that went away from killing the app (SIGPIPE).
/// @param socket The socket.
void ignoreBrokenPipes([[maybe_unused]] Native socket) noexcept {
#ifdef SO_NOSIGPIPE
	const int on = 1;
	setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
}

#endif

/* Shared by both */

/// @brief The handle of a native socket.
/// @param socket The socket.
Handle handleOf(Native socket) noexcept { return static_cast<Handle>(socket); }

/// @brief The native socket of a handle.
/// @param handle The handle.
Native nativeOf(Handle handle) noexcept { return static_cast<Native>(handle); }

/// @brief The address of a connected peer, without its port.
/// @param address The address accept() gave.
std::string addressText(const sockaddr_storage& address) {
	std::array<char, INET6_ADDRSTRLEN> text{};
	const void* bytes = address.ss_family == AF_INET6
	                        ? static_cast<const void*>(&reinterpret_cast<const sockaddr_in6&>(address).sin6_addr)
	                        : static_cast<const void*>(&reinterpret_cast<const sockaddr_in&>(address).sin_addr);
	if(!inet_ntop(address.ss_family, bytes, text.data(), text.size())) return "unknown";
	return text.data();
}

/// @brief Parses an address into a socket address.
/// @param host "127.0.0.1", "::1", "0.0.0.0", "::".
/// @param port The port.
/// @param address Receives it.
/// @return Its length, or 0 if the host isn't an address.
AddressLength parseAddress(const std::string& host, std::uint16_t port, sockaddr_storage& address) {
	address = {};
	auto& v4 = reinterpret_cast<sockaddr_in&>(address);
	if(inet_pton(AF_INET, host.c_str(), &v4.sin_addr) == 1) {
		v4.sin_family = AF_INET;
		v4.sin_port = htons(port);
		return static_cast<AddressLength>(sizeof(sockaddr_in));
	}
	auto& v6 = reinterpret_cast<sockaddr_in6&>(address);
	if(inet_pton(AF_INET6, host.c_str(), &v6.sin6_addr) == 1) {
		v6.sin6_family = AF_INET6;
		v6.sin6_port = htons(port);
		return static_cast<AddressLength>(sizeof(sockaddr_in6));
	}
	return 0;
}

/// @brief The port a bound socket has.
/// @param socket The socket.
std::uint16_t boundPort(Native socket) {
	sockaddr_storage address{};
	auto length = static_cast<AddressLength>(sizeof(address));
	if(getsockname(socket, reinterpret_cast<sockaddr*>(&address), &length) != 0) return 0;
	return ntohs(address.ss_family == AF_INET6 ? reinterpret_cast<const sockaddr_in6&>(address).sin6_port
	                                           : reinterpret_cast<const sockaddr_in&>(address).sin_port);
}

/// @brief Makes a socket for a connection: no delay (small frames go at once), no SIGPIPE.
/// @param socket The socket.
void prepare(Native socket) noexcept {
	const int on = 1;
	setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
	ignoreBrokenPipes(socket);
}

} // namespace

Connection::~Connection() { closeNative(nativeOf(handle_)); }

std::unique_ptr<Connection> Connection::open(const std::string& host, std::uint16_t port) {
	if(!startSockets()) return nullptr;
	addrinfo hints{};
	hints.ai_socktype = SOCK_STREAM;
	addrinfo* found = nullptr;
	if(getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0) return nullptr;
	std::unique_ptr<Connection> connection;
	for(const addrinfo* at = found; at && !connection; at = at->ai_next) {
		const Native socket = ::socket(at->ai_family, at->ai_socktype, at->ai_protocol);
		if(socket == noSocket) continue;
		if(::connect(socket, at->ai_addr, static_cast<AddressLength>(at->ai_addrlen)) != 0) {
			closeNative(socket);
			continue;
		}
		prepare(socket);
		connection = std::make_unique<Connection>(handleOf(socket), host);
	}
	freeaddrinfo(found);
	return connection;
}

int Connection::wait(bool write, std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	while(!closed_) {
		const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
		if(left.count() < 0) return 0;
		const int result = pollNative(nativeOf(handle_), write ? POLLOUT : POLLIN, static_cast<int>(std::min(left, pollSlice).count()));
		if(result != 0) return result > 0 ? 1 : -1;
	}
	return -1;
}

Received Connection::receive(std::string& into, std::chrono::milliseconds timeout) {
	const int ready = wait(false, timeout);
	if(ready == 0) return Received::TimedOut;
	if(ready < 0) return Received::Failed;
	std::array<char, 65536> buffer{};
	const auto count = receiveNative(nativeOf(handle_), buffer.data(), buffer.size());
	if(count == 0) return Received::Closed;
	if(count < 0) return Received::Failed;
	into.append(buffer.data(), static_cast<std::size_t>(count));
	return Received::Data;
}

bool Connection::send(std::string_view bytes, std::chrono::milliseconds timeout) {
	const auto deadline = std::chrono::steady_clock::now() + timeout;
	while(!bytes.empty()) {
		const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
		if(wait(true, std::max(left, std::chrono::milliseconds{0})) != 1) return false;
		const auto count = sendNative(nativeOf(handle_), bytes.data(), bytes.size());
		if(count < 1) return false;
		bytes.remove_prefix(static_cast<std::size_t>(count));
	}
	return true;
}

void Connection::close() noexcept { closed_ = true; }

std::unique_ptr<Listener> Listener::open(const std::string& host, std::uint16_t port, std::string& error) {
	sockaddr_storage address{};
	const AddressLength length = parseAddress(host, port, address);
	if(length == 0) {
		error = "'" + host + "' isn't an IP address";
		return nullptr;
	}
	if(!startSockets()) {
		error = "the sockets could not start";
		return nullptr;
	}
	const Native socket = ::socket(address.ss_family, SOCK_STREAM, IPPROTO_TCP);
	const int on = 1;
#ifndef _WIN32
	if(socket != noSocket) setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); // restart while TIME_WAIT
#else
	if(socket != noSocket) setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&on), sizeof(on));
#endif
	if(socket == noSocket || bind(socket, reinterpret_cast<const sockaddr*>(&address), length) != 0 ||
	   listen(socket, 64) != 0) {
		error = lastError();
		if(socket != noSocket) closeNative(socket);
		return nullptr;
	}
	return std::make_unique<Listener>(handleOf(socket), boundPort(socket));
}

Listener::~Listener() { closeNative(nativeOf(handle_)); }

std::unique_ptr<Connection> Listener::accept() {
	while(!closed_) {
		const int ready = pollNative(nativeOf(handle_), POLLIN, static_cast<int>(pollSlice.count()));
		if(ready < 0) return nullptr;
		if(ready == 0) continue;
		sockaddr_storage address{};
		auto length = static_cast<AddressLength>(sizeof(address));
		const Native socket = ::accept(nativeOf(handle_), reinterpret_cast<sockaddr*>(&address), &length);
		if(socket == noSocket) continue; // the peer went away before it was accepted
		prepare(socket);
		return std::make_unique<Connection>(handleOf(socket), addressText(address));
	}
	return nullptr;
}

std::optional<std::pair<std::string, std::uint16_t>> splitAddress(std::string_view address) {
	const auto colon = address.rfind(':');
	if(colon == std::string_view::npos || colon == 0) return std::nullopt;
	std::string_view host = address.substr(0, colon);
	if(host.starts_with('[') && host.ends_with(']')) host = host.substr(1, host.size() - 2);
	const auto digits = address.substr(colon + 1);
	if(digits.empty() || digits.size() > 5) return std::nullopt;
	unsigned port = 0;
	for(const char c : digits) {
		if(c < '0' || c > '9') return std::nullopt;
		port = port * 10 + static_cast<unsigned>(c - '0');
	}
	if(port > 65535) return std::nullopt;
	return std::pair{std::string(host), static_cast<std::uint16_t>(port)};
}

} // namespace affresco::net::tcp
