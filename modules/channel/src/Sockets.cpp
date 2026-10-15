#include "sgl/channel/Sockets.hpp"

#include <stdexcept>

#if defined(_WIN32)

namespace sgl::channel {

std::unique_ptr<SocketChannel> SocketChannel::connect(Executor&, const std::filesystem::path&) {
	throw std::runtime_error("sGLGUI: SocketChannel is not available on Windows yet");
}
SocketChannel::SocketChannel(Executor&, int) {}
SocketChannel::~SocketChannel() = default;
void SocketChannel::send(std::string_view) {}

SocketServer::SocketServer(Executor& executor, std::filesystem::path, Connected) : executor_(executor) {
	throw std::runtime_error("sGLGUI: SocketServer is not available on Windows yet");
}
SocketServer::~SocketServer() = default;

} // namespace sgl::channel

#else

#include "Lines.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace sgl::channel {

namespace {

/// @brief Makes the address of a socket file.
/// @param path The file.
/// @return The address.
/// @throws std::runtime_error If the path is too long.
sockaddr_un address(const std::filesystem::path& path) {
	sockaddr_un result{};
	result.sun_family = AF_UNIX;
	const std::string text = path.string();
	if(text.size() >= sizeof(result.sun_path)) throw std::runtime_error("sGLGUI: socket path too long: " + text);
	std::memcpy(result.sun_path, text.c_str(), text.size() + 1);
	return result;
}

/// @brief Connects to a socket file.
/// @param path The file.
/// @return A connected socket, or -1.
int connectTo(const std::filesystem::path& path) {
	const sockaddr_un target = address(path);
	const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd < 0) return -1;
	if(::connect(fd, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) != 0) {
		close(fd);
		return -1;
	}
	return fd;
}

/// @brief Keeps writes to a peer that went away from killing the app (SIGPIPE).
/// @param fd The socket.
void ignoreBrokenPipes(int fd) {
	std::signal(SIGPIPE, SIG_IGN); // writing to a peer that went away must not kill the app
#if defined(SO_NOSIGPIPE)
	const int on = 1;
	setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
}

} // namespace

/* SocketChannel */

std::unique_ptr<SocketChannel> SocketChannel::connect(Executor& executor, const std::filesystem::path& path) {
	const int fd = connectTo(path);
	if(fd < 0) throw std::runtime_error("sGLGUI: nothing is listening at " + path.string());
	return std::make_unique<SocketChannel>(executor, fd);
}

SocketChannel::SocketChannel(Executor& executor, int socket) : socket_(socket) {
	ignoreBrokenPipes(socket_);
	reader_ = std::thread([this, &executor] { detail::readLines(socket_, executor, *this, alive_, &open_); });
}

SocketChannel::~SocketChannel() {
	*alive_ = false;
	shutdown(socket_, SHUT_RDWR); // ends the reader's read()
	if(reader_.joinable()) reader_.join();
	close(socket_);
}

void SocketChannel::send(std::string_view message) {
	if(open_ && !detail::writeAll(socket_, message)) open_ = false;
}

/* SocketServer */

SocketServer::SocketServer(Executor& executor, std::filesystem::path path, Connected onConnection)
    : executor_(executor), path_(std::move(path)), onConnection_(std::move(onConnection)) {
	// A socket file left by a server that crashed is replaced; one that still answers is in use
	if(const int existing = connectTo(path_); existing >= 0) {
		close(existing);
		throw std::runtime_error("sGLGUI: another app is already listening at " + path_.string());
	}
	unlink(path_.c_str());

	const sockaddr_un local = address(path_);
	listener_ = socket(AF_UNIX, SOCK_STREAM, 0);
	if(listener_ < 0 || bind(listener_, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0 ||
	   chmod(path_.c_str(), S_IRUSR | S_IWUSR) != 0 || listen(listener_, 8) != 0 || pipe(wake_) != 0) {
		const std::string reason = std::strerror(errno);
		if(listener_ >= 0) close(listener_);
		throw std::runtime_error("sGLGUI: could not listen at " + path_.string() + ": " + reason);
	}

	acceptor_ = std::thread([this] {
		pollfd watched[2] = {{listener_, POLLIN, 0}, {wake_[0], POLLIN, 0}};
		while(poll(watched, 2, -1) >= 0 && !(watched[1].revents & POLLIN)) {
			if(!(watched[0].revents & POLLIN)) continue;
			const int fd = accept(listener_, nullptr, nullptr);
			if(fd < 0) continue;
			executor_.post([this, fd, alive = alive_] {
				if(*alive) onConnection_(std::make_unique<SocketChannel>(executor_, fd));
				else close(fd);
			});
		}
	});
}

SocketServer::~SocketServer() {
	*alive_ = false;
	const char stop = 0;
	if(write(wake_[1], &stop, 1) != 1) shutdown(listener_, SHUT_RDWR);
	if(acceptor_.joinable()) acceptor_.join();
	for(int fd : {listener_, wake_[0], wake_[1]})
		if(fd >= 0) close(fd);
	unlink(path_.c_str());
}

} // namespace sgl::channel

#endif
