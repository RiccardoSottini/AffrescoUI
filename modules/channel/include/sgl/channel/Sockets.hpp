#pragma once

#include "sgl/core/Channel.hpp"
#include "sgl/core/Executor.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <thread>

namespace sgl::channel {

// A Channel over a connected local socket: one JSON object per line, in both directions. POSIX only for now.
class SocketChannel final : public Channel {
public:
	// Connects to a SocketServer listening at `path`. Throws std::runtime_error if nothing is listening there.
	[[nodiscard]] static std::unique_ptr<SocketChannel> connect(Executor& executor, const std::filesystem::path& path);

	SocketChannel(Executor& executor, int socket); // takes ownership of a connected socket
	~SocketChannel() override;

	SocketChannel(const SocketChannel&) = delete;
	SocketChannel& operator=(const SocketChannel&) = delete;

	void send(std::string_view message) override;
	[[nodiscard]] bool isOpen() const noexcept { return open_; }

private:
	int socket_ = -1;
	std::atomic<bool> open_ = true;
	std::thread reader_;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); // guards tasks still queued after destruction
};

// Listens on a local (Unix domain) socket, readable and writable only by this user. Every connection becomes a
// SocketChannel, handed to `onConnection` on the main thread. POSIX only for now.
class SocketServer {
public:
	using Connected = std::function<void(std::unique_ptr<Channel>)>;

	// Throws std::runtime_error, e.g. if another server is already listening at `path`
	SocketServer(Executor& executor, std::filesystem::path path, Connected onConnection);
	~SocketServer(); // stops listening and removes the socket file

	SocketServer(const SocketServer&) = delete;
	SocketServer& operator=(const SocketServer&) = delete;

	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	Executor& executor_;
	std::filesystem::path path_;
	Connected onConnection_;
	int listener_ = -1;
	int wake_[2] = {-1, -1}; // written to by the destructor to stop the accept loop
	std::thread acceptor_;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace sgl::channel
