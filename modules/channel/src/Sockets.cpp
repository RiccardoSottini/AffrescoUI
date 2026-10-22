/// @file
/// @brief SocketChannel and SocketServer. What differs between POSIX and Winsock is in one block of small
/// functions ("The platform"); the channel and the server above it are written once.

#include "affresco/channel/Sockets.hpp"

#include "Lines.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <afunix.h>
#include <windows.h>
#include <aclapi.h>

#include <climits>
#include <system_error>
#include <vector>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace affresco::channel {

namespace {

/* The platform */

#ifdef _WIN32

static_assert(sizeof(SocketHandle) == sizeof(SOCKET), "SocketHandle must hold a SOCKET");

constexpr SocketHandle noSocket = INVALID_SOCKET; ///< What socket() and accept() return on failure.
using AddressLength = int;                        ///< The type of an address's length.

/// @brief Starts Winsock once for the process. It is never cleaned up: sockets may live until the process exits.
/// @throws std::runtime_error If Winsock can't start.
void startSockets() {
	static const bool started = [] {
		WSADATA data{};
		return WSAStartup(MAKEWORD(2, 2), &data) == 0;
	}();
	if(!started) throw std::runtime_error("AffrescoUI: could not start Winsock");
}

/// @brief Describes the last error of this thread (Winsock's and Windows' share one slot).
/// @return The description.
std::string lastError() { return std::system_category().message(static_cast<int>(GetLastError())); }

/// @brief Keeps a socket from the programs the host starts: on Windows they get only the handles listed for them.
/// @param socket The socket.
/// @return It.
SocketHandle hostOnly(SocketHandle socket) noexcept { return socket; }

/// @brief Closes a socket.
/// @param socket The socket.
void closeSocket(SocketHandle socket) noexcept { closesocket(socket); }

/// @brief Ends both directions of a connection, and a recv() blocked on it in another thread.
/// @param socket The socket.
void shutdownSocket(SocketHandle socket) noexcept {
	shutdown(socket, SD_BOTH);
	// Unlike POSIX, shutdown() doesn't end a recv() that is already waiting; cancelling the socket's I/O does
	CancelIoEx(reinterpret_cast<HANDLE>(socket), nullptr);
}

/// @brief Nothing to do: Winsock reports a peer that went away as an error from send(), with no signal.
void ignoreBrokenPipes(SocketHandle) {}

/// @brief Receives from a connection.
/// @param socket The socket.
/// @param data Receives the bytes.
/// @param size The most to read (one chunk).
/// @return How many bytes, 0 when the peer closed, less on an error.
std::ptrdiff_t receive(SocketHandle socket, char* data, std::size_t size) {
	return recv(socket, data, static_cast<int>(size), 0);
}

/// @brief Sends on a connection.
/// @param socket The socket.
/// @param data The bytes.
/// @param size How many; more than INT_MAX are sent by the next call.
/// @return How many bytes were sent, less than 1 on an error.
std::ptrdiff_t transmit(SocketHandle socket, const char* data, std::size_t size) {
	constexpr std::size_t most = INT_MAX; // send() takes an int
	return send(socket, data, static_cast<int>(size < most ? size : most), 0);
}

/// @brief Reads the user a process runs as.
/// @param process The process (GetCurrentProcess() for this one).
/// @return A TOKEN_USER, in a buffer; empty if it can't be read.
std::vector<std::byte> userOf(HANDLE process) {
	HANDLE token = nullptr;
	if(!OpenProcessToken(process, TOKEN_QUERY, &token)) return {};
	DWORD size = 0;
	GetTokenInformation(token, TokenUser, nullptr, 0, &size); // asks for the size
	std::vector<std::byte> user(size);
	if(!GetTokenInformation(token, TokenUser, user.data(), size, &size)) user.clear();
	CloseHandle(token);
	return user;
}

/// @brief Reads this process's user.
/// @return A TOKEN_USER, in a buffer; empty if it can't be read.
std::vector<std::byte> currentUser() { return userOf(GetCurrentProcess()); }

/// @brief The SID in a TOKEN_USER buffer.
/// @param user A buffer from userOf(), not empty.
/// @return Its SID.
PSID sidOf(std::vector<std::byte>& user) { return reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid; }

#ifndef SIO_AF_UNIX_GETPEERPID
#define SIO_AF_UNIX_GETPEERPID _WSAIOR(IOC_VENDOR, 256) ///< In afunix.h of the Windows 10 SDKs from build 17063.
#endif

/// @brief Whether the process at the other end of a connection runs as this process's user: Winsock gives its
/// process id, and its token gives its user.
/// @param socket The connection.
/// @return false also when the process can't be opened or its token read (e.g. another user's, or an elevated one).
bool sameUser(SocketHandle socket) {
	ULONG peer = 0;
	DWORD size = 0;
	if(WSAIoctl(socket, SIO_AF_UNIX_GETPEERPID, nullptr, 0, &peer, sizeof(peer), &size, nullptr, nullptr) != 0)
		return false;
	const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, peer);
	if(!process) return false;
	std::vector<std::byte> theirs = userOf(process);
	CloseHandle(process);
	std::vector<std::byte> ours = currentUser();
	return !theirs.empty() && !ours.empty() && EqualSid(sidOf(theirs), sidOf(ours)) != FALSE;
}

/// @brief Lets only this user open the socket file, as `chmod 0600` does on POSIX: the file's access list becomes
/// one entry that gives this user full access, and it no longer inherits the folder's. Connecting needs write access
/// to the file, so other users can't connect.
/// @param path The socket file.
/// @return false on an error (see lastError()).
bool restrictToOwner(const std::filesystem::path& path) {
	std::vector<std::byte> user = currentUser();
	if(user.empty()) return false;

	EXPLICIT_ACCESSW entry{};
	entry.grfAccessPermissions = GENERIC_ALL;
	entry.grfAccessMode = SET_ACCESS;
	entry.grfInheritance = NO_INHERITANCE;
	entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	entry.Trustee.TrusteeType = TRUSTEE_IS_USER;
	entry.Trustee.ptstrName = static_cast<LPWSTR>(sidOf(user));
	PACL access = nullptr;
	DWORD error = SetEntriesInAclW(1, &entry, nullptr, &access);
	if(error == ERROR_SUCCESS) {
		// The socket file is a reparse point: open the file itself, not what it would point to
		const HANDLE file = CreateFileW(path.c_str(), READ_CONTROL | WRITE_DAC,
		                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		                                FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if(file == INVALID_HANDLE_VALUE) error = GetLastError();
		else {
			error = SetSecurityInfo(file, SE_FILE_OBJECT,
			                        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr,
			                        access, nullptr);
			CloseHandle(file);
		}
		LocalFree(access);
	}
	SetLastError(error);
	return error == ERROR_SUCCESS;
}

/// @brief Removes a socket file, if there is one.
/// @param path The file.
void removeSocketFile(const std::filesystem::path& path) { DeleteFileW(path.c_str()); }

/// @brief Accepts a connection that is waiting.
/// @param listener The listening socket.
/// @return The connection, or noSocket.
SocketHandle acceptConnection(SocketHandle listener) {
	const SOCKET connection = accept(listener, nullptr, nullptr);
	if(connection == INVALID_SOCKET) return noSocket;
	// It inherits the listener's event selection, which also made it non-blocking; the channel's reader blocks
	u_long blocking = 0;
	if(WSAEventSelect(connection, nullptr, 0) != 0 || ioctlsocket(connection, FIONBIO, &blocking) != 0) {
		closesocket(connection);
		return noSocket;
	}
	return connection;
}

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

constexpr SocketHandle noSocket = -1; ///< What socket() and accept() return on failure.
using AddressLength = socklen_t;      ///< The type of an address's length.

/// @brief Nothing to start on POSIX.
void startSockets() {}

/// @brief Describes the last error of this thread.
/// @return The description.
std::string lastError() { return std::strerror(errno); }

/// @brief Keeps a descriptor from the programs the host starts (close-on-exec): a peer's connection or the
/// listening socket must not reach a shell or a debug adapter.
/// @param socket The descriptor, or noSocket.
/// @return It.
SocketHandle hostOnly(SocketHandle socket) noexcept {
	if(socket != noSocket) fcntl(socket, F_SETFD, FD_CLOEXEC);
	return socket;
}

/// @brief Closes a socket.
/// @param socket The socket.
void closeSocket(SocketHandle socket) noexcept { close(socket); }

/// @brief Ends both directions of a connection, which ends a read() blocked on it in another thread.
/// @param socket The socket.
void shutdownSocket(SocketHandle socket) noexcept { shutdown(socket, SHUT_RDWR); }

/// @brief Keeps writes to a peer that went away from killing the app (SIGPIPE).
/// @param socket The socket.
void ignoreBrokenPipes([[maybe_unused]] SocketHandle socket) {
	std::signal(SIGPIPE, SIG_IGN); // writing to a peer that went away must not kill the app
#ifdef SO_NOSIGPIPE
	const int on = 1;
	setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
}

/// @brief Receives from a connection.
/// @param socket The socket.
/// @param data Receives the bytes.
/// @param size The most to read.
/// @return How many bytes, 0 when the peer closed, less on an error.
std::ptrdiff_t receive(SocketHandle socket, char* data, std::size_t size) {
	return detail::readDescriptor(socket, data, size);
}

/// @brief Sends on a connection.
/// @param socket The socket.
/// @param data The bytes.
/// @param size How many.
/// @return How many bytes were sent, less than 1 on an error.
std::ptrdiff_t transmit(SocketHandle socket, const char* data, std::size_t size) {
	return detail::writeDescriptor(socket, data, size);
}

/// @brief Whether the process at the other end of a connection runs as this process's user (its effective user).
/// @param socket The connection.
/// @return false also when the operating system can't say.
bool sameUser(SocketHandle socket) {
#ifdef __linux__
	ucred peer{};
	socklen_t size = sizeof(peer);
	return getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &peer, &size) == 0 && size == sizeof(peer) &&
	       peer.uid == geteuid();
#elif defined(__EMSCRIPTEN__)
	(void)socket;
	return false; // a browser can't say, so local connections are refused
#else
	uid_t user = 0;
	gid_t group = 0;
	return getpeereid(socket, &user, &group) == 0 && user == geteuid();
#endif
}

/// @brief Lets only this user open the socket file (`0600`).
/// @param path The socket file.
/// @return false on an error (see lastError()).
bool restrictToOwner(const std::filesystem::path& path) { return chmod(path.c_str(), S_IRUSR | S_IWUSR) == 0; }

/// @brief Removes a socket file, if there is one.
/// @param path The file.
void removeSocketFile(const std::filesystem::path& path) { unlink(path.c_str()); }

/// @brief Accepts a connection that is waiting.
/// @param listener The listening socket.
/// @return The connection, or noSocket.
SocketHandle acceptConnection(SocketHandle listener) { return hostOnly(accept(listener, nullptr, nullptr)); }

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

/* Shared by both */

/// @brief Makes the address of a socket file.
/// @param path The file.
/// @return The address.
/// @throws std::runtime_error If the path is too long.
sockaddr_un address(const std::filesystem::path& path) {
	sockaddr_un result{};
	result.sun_family = AF_UNIX;
	const std::string text = path.string();
	if(text.size() >= sizeof(result.sun_path)) throw std::runtime_error("AffrescoUI: socket path too long: " + text);
	std::memcpy(result.sun_path, text.c_str(), text.size() + 1);
	return result;
}

/// @brief The length of a socket file's address, as the socket functions take it.
constexpr auto addressLength = static_cast<AddressLength>(sizeof(sockaddr_un));

/// @brief Connects to a socket file.
/// @param path The file.
/// @return A connected socket, or noSocket.
SocketHandle connectTo(const std::filesystem::path& path) {
	startSockets();
	const sockaddr_un target = address(path);
	const SocketHandle connection = hostOnly(socket(AF_UNIX, SOCK_STREAM, 0));
	if(connection == noSocket) return noSocket;
	if(::connect(connection, reinterpret_cast<const sockaddr*>(&target), addressLength) != 0) {
		closeSocket(connection);
		return noSocket;
	}
	return connection;
}

/// @brief Creates a socket file and listens on it.
/// @param path The file (one left by a server that crashed must be removed first).
/// @return The listening socket.
/// @throws std::runtime_error If it can't be created.
SocketHandle listenAt(const std::filesystem::path& path) {
	startSockets();
	const sockaddr_un local = address(path);
	const SocketHandle listener = hostOnly(socket(AF_UNIX, SOCK_STREAM, 0));
	if(listener == noSocket || bind(listener, reinterpret_cast<const sockaddr*>(&local), addressLength) != 0 ||
	   !restrictToOwner(path) || listen(listener, 8) != 0) {
		const std::string reason = lastError();
		if(listener != noSocket) closeSocket(listener);
		throw std::runtime_error("AffrescoUI: could not listen at " + path.string() + ": " + reason);
	}
	return listener;
}

} // namespace

bool peerIsThisUser(SocketHandle socket) { return sameUser(socket); }

/* SocketServer::Listener */

/// @brief A listening socket, and the wake-up that lets the destructor stop a thread blocked accepting on it.
///
/// accept() runs on the server's acceptor thread; stop() on the main thread.
class SocketServer::Listener {
public:
	/// @brief Creates the socket file and listens on it.
	/// @param path The socket file.
	/// @throws std::runtime_error If it can't be created.
	explicit Listener(const std::filesystem::path& path) : socket_(listenAt(path)) {
		if(!wait_.watch(socket_)) {
			const std::string reason = lastError();
			closeSocket(socket_);
			throw std::runtime_error("AffrescoUI: could not listen at " + path.string() + ": " + reason);
		}
	}
	/// @brief Closes the socket (the file stays: the server removes it).
	~Listener() { closeSocket(socket_); }

	Listener(const Listener&) = delete;
	Listener& operator=(const Listener&) = delete;

	/// @brief Waits for the next connection.
	/// @return The connection, or nothing once stop() was called.
	[[nodiscard]] std::optional<SocketHandle> accept() {
		while(wait_.waitForConnection())
			if(const SocketHandle connection = acceptConnection(socket_); connection != noSocket) return connection;
		return std::nullopt;
	}

	/// @brief Ends accept(), now or at its next call.
	void stop() noexcept { wait_.stop(); }

private:
	SocketHandle socket_; ///< The listening socket.
	AcceptWait wait_;     ///< Wakes accept() for a connection or for stop().
};

/* SocketChannel */

std::unique_ptr<SocketChannel> SocketChannel::connect(Executor& executor, const std::filesystem::path& path) {
	const SocketHandle connection = connectTo(path);
	if(connection == noSocket) throw std::runtime_error("AffrescoUI: nothing is listening at " + path.string());
	return std::make_unique<SocketChannel>(executor, connection);
}

SocketChannel::SocketChannel(Executor& executor, SocketHandle socket, std::size_t maxQueuedBytes)
    : socket_(socket), maxQueuedBytes_(maxQueuedBytes) {
	ignoreBrokenPipes(socket_);
	reader_ = std::thread([this, &executor] {
		const auto read = [this](char* data, std::size_t size) { return receive(socket_, data, size); };
		detail::readLines(read, executor, *this, alive_, &open_);
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
	shutdownSocket(socket_); // ends the reader's read, and a write the peer never took
	if(reader_.joinable()) reader_.join();
	if(writer_.joinable()) writer_.join();
	closeSocket(socket_);
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
	shutdownSocket(socket_); // the reader sees the end and reports onClosed
}

void SocketChannel::writeQueued() {
	const auto write = [this](const char* data, std::size_t size) { return transmit(socket_, data, size); };
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
	if(const SocketHandle existing = connectTo(path_); existing != noSocket) {
		closeSocket(existing);
		throw std::runtime_error("AffrescoUI: another app is already listening at " + path_.string());
	}
	removeSocketFile(path_);
	listener_ = std::make_unique<Listener>(path_);

	acceptor_ = std::thread([this] {
		while(const std::optional<SocketHandle> connection = listener_->accept()) {
			if(!admits(*connection)) {
				log(LogLevel::Warning, "AffrescoUI: refused a connection to " + path_.string() +
				                           ": the program on the other end runs as another user");
				closeSocket(*connection);
				continue;
			}
			executor_.post([this, socket = *connection, alive = alive_] {
				if(*alive) onConnection_(std::make_unique<SocketChannel>(executor_, socket));
				else closeSocket(socket);
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
	removeSocketFile(path_);
}

} // namespace affresco::channel
