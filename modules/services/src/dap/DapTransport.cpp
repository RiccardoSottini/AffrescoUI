/// @file
/// @brief DapTransport (DapTransport.hpp): a granted program's pipes, or a loopback TCP connection (POSIX for now).

#include "dap/DapTransport.hpp"

#include <stdexcept>
#include <string>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <mutex>
#endif

namespace affresco::services {

namespace {

/// @brief A granted program's stdin and stdout.
class ProcessTransport final : public DapTransport {
public:
	/// @brief Starts the program.
	/// @param launch What to start.
	explicit ProcessTransport(const Launch& launch) : child_(ChildProcess::start(launch)) {}
	/// @brief Ends the adapter, so no debug adapter outlives its session.
	~ProcessTransport() override { (void)child_->end(); }
	ProcessTransport(const ProcessTransport&) = delete;
	ProcessTransport& operator=(const ProcessTransport&) = delete;
	ProcessTransport(ProcessTransport&&) = delete;
	ProcessTransport& operator=(ProcessTransport&&) = delete;

	bool write(std::string_view bytes) override { return child_->write(bytes); }
	std::ptrdiff_t read(char* buffer, std::size_t size) override { return child_->read(buffer, size); }
	void closeInput() noexcept override { child_->closeInput(); }
	int end() noexcept override { return child_->end(); }

private:
	std::unique_ptr<ChildProcess> child_; ///< The adapter.
};

#ifndef _WIN32
/// @brief A TCP connection to the loopback address.
class SocketTransport final : public DapTransport {
public:
	/// @brief Connects.
	/// @param port The port on 127.0.0.1.
	/// @throws std::runtime_error If nothing listens there.
	explicit SocketTransport(std::uint16_t port) : socket_(::socket(AF_INET, SOCK_STREAM, 0)) {
		if(socket_ < 0) throw std::runtime_error("can't make a socket to attach to the debug adapter");
		fcntl(socket_, F_SETFD, FD_CLOEXEC); // programs the host starts later don't keep the adapter connected
#ifdef SO_NOSIGPIPE
		const int on = 1;
		(void)setsockopt(socket_, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if(::connect(socket_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
			::close(socket_);
			throw std::runtime_error("no debug adapter listens on 127.0.0.1:" + std::to_string(port));
		}
	}
	/// @brief Stops the reader, then closes the connection (the adapter keeps running: it wasn't started here).
	~SocketTransport() override {
		(void)end();
		::close(socket_);
	}
	SocketTransport(const SocketTransport&) = delete;
	SocketTransport& operator=(const SocketTransport&) = delete;
	SocketTransport(SocketTransport&&) = delete;
	SocketTransport& operator=(SocketTransport&&) = delete;

	bool write(std::string_view bytes) override {
		while(!bytes.empty()) {
#ifdef MSG_NOSIGNAL
			const ssize_t sent = ::send(socket_, bytes.data(), bytes.size(), MSG_NOSIGNAL);
#else
			const ssize_t sent = ::send(socket_, bytes.data(), bytes.size(), 0);
#endif
			if(sent < 0 && errno == EINTR) continue;
			if(sent <= 0) return false;
			bytes.remove_prefix(static_cast<std::size_t>(sent));
		}
		return true;
	}
	std::ptrdiff_t read(char* buffer, std::size_t size) override {
		for(;;) {
			const ssize_t count = ::recv(socket_, buffer, size, 0);
			if(count < 0 && errno == EINTR) continue;
			return count;
		}
	}
	void closeInput() noexcept override { (void)::shutdown(socket_, SHUT_WR); }
	int end() noexcept override {
		const std::lock_guard lock(mutex_);
		if(!ended_) (void)::shutdown(socket_, SHUT_RDWR); // a blocked recv returns 0
		ended_ = true;
		return -1;
	}

private:
	int socket_;         ///< The connection; closed by the destructor only, so a blocked read never sees it reused.
	std::mutex mutex_;   ///< Guards ended_.
	bool ended_ = false; ///< end() was called.
};
#endif

} // namespace

std::unique_ptr<DapTransport> DapTransport::start(const Launch& launch) {
	return std::make_unique<ProcessTransport>(launch);
}

std::unique_ptr<DapTransport> DapTransport::connect([[maybe_unused]] std::uint16_t port) {
#ifdef _WIN32
	throw std::runtime_error("attaching to a debug adapter over a socket isn't supported on Windows yet");
#else
	return std::make_unique<SocketTransport>(port);
#endif
}

} // namespace affresco::services
