#pragma once

/// @file
/// @brief What the network tests share: a local server on the loopback that answers with what a test scripts, and an
/// Executor that worker threads may post to.

#include "Http.hpp"
#include "Tcp.hpp"

#include <affresco/core/Executor.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace affresco::test {

/// @brief An Executor that any thread may post to; the test's thread runs the tasks in runUntil().
class ThreadExecutor final : public Executor {
public:
	/// @brief Keeps a task (from any thread).
	/// @param task The task.
	void post(std::function<void()> task) override {
		{
			const std::scoped_lock lock(mutex_);
			tasks_.push_back(std::move(task));
		}
		ready_.notify_one();
	}

	/// @brief Runs the posted tasks until a condition holds, or a timeout.
	/// @param done The condition, checked after each task.
	/// @param timeout The longest wait.
	/// @return Whether the condition holds.
	bool runUntil(const std::function<bool()>& done, std::chrono::milliseconds timeout = std::chrono::seconds{10}) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while(!done()) {
			std::vector<std::function<void()>> tasks;
			{
				std::unique_lock lock(mutex_);
				if(!ready_.wait_until(lock, deadline, [this] { return !tasks_.empty(); })) return done();
				tasks.swap(tasks_);
			}
			for(auto& task : tasks) task();
		}
		return true;
	}

	/// @brief Runs what was posted within a short time (to see that nothing more comes).
	/// @param time How long.
	void runFor(std::chrono::milliseconds time) {
		static_cast<void>(runUntil([] { return false; }, time));
	}

private:
	std::mutex mutex_;                         ///< Guards tasks_.
	std::condition_variable ready_;            ///< Signalled on post().
	std::vector<std::function<void()>> tasks_; ///< Posted, not yet run.
};

/// @brief A request the server read.
struct ServedRequest {
	std::string head; ///< The request line and the headers.
	std::string body; ///< The body (by Content-Length).
};

/// @brief Reads one request from a connection: the head, then a body of Content-Length bytes.
/// @param connection The connection.
/// @return The request, or an empty head if it didn't come.
inline ServedRequest readRequest(net::tcp::Connection& connection) {
	std::string bytes;
	std::size_t end = std::string::npos;
	while((end = bytes.find("\r\n\r\n")) == std::string::npos)
		if(connection.receive(bytes, std::chrono::seconds{5}) != net::tcp::Received::Data) return {};
	ServedRequest request{bytes.substr(0, end + 4), bytes.substr(end + 4)};
	std::size_t length = 0;
	if(auto block = net::parseHeaderBlock(request.head))
		if(auto value = net::findHeader(block->headers, "content-length")) length = std::stoul(*value);
	while(request.body.size() < length)
		if(connection.receive(request.body, std::chrono::seconds{5}) != net::tcp::Received::Data) break;
	return request;
}

/// @brief A whole HTTP/1.1 response.
/// @param status The status line's code and reason, e.g. "200 OK".
/// @param body The body.
/// @param headers Extra header lines, each ending in "\r\n".
/// @return The bytes.
inline std::string httpResponse(const std::string& status, const std::string& body, const std::string& headers = {}) {
	return "HTTP/1.1 " + status + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + headers +
	       "Connection: close\r\n\r\n" + body;
}

/// @brief A server on 127.0.0.1 and a free port: each connection goes to the handler, on the server's thread, one
/// after another. It stops when destroyed.
class LocalServer {
public:
	/// @brief The handler of one connection.
	using Handler = std::function<void(net::tcp::Connection&)>;

	/// @brief Starts listening.
	/// @param handler Called with each connection.
	explicit LocalServer(Handler handler) : handler_(std::move(handler)) {
		std::string error;
		listener_ = net::tcp::Listener::open("127.0.0.1", 0, error);
		if(listener_) thread_ = std::jthread([this] { serve(); });
	}
	/// @brief Stops listening, and waits for the thread.
	~LocalServer() {
		if(listener_) listener_->close();
	}
	LocalServer(const LocalServer&) = delete;
	LocalServer& operator=(const LocalServer&) = delete;
	LocalServer(LocalServer&&) = delete;
	LocalServer& operator=(LocalServer&&) = delete;

	/// @brief The server's origin, e.g. "http://127.0.0.1:50123".
	/// @param scheme "http" or "ws".
	[[nodiscard]] std::string origin(const std::string& scheme = "http") const {
		return scheme + "://127.0.0.1:" + std::to_string(listener_ ? listener_->port() : 0);
	}
	/// @brief How many connections it took.
	[[nodiscard]] int connections() const noexcept { return connections_; }

private:
	/// @brief The thread: accepts and handles connections until closed.
	void serve() {
		while(auto connection = listener_->accept()) {
			++connections_;
			handler_(*connection);
			connection->close();
		}
	}

	Handler handler_;                            ///< See the constructor.
	std::unique_ptr<net::tcp::Listener> listener_; ///< The socket.
	std::atomic<int> connections_{0};            ///< See connections().
	std::jthread thread_;                        ///< Last: it starts once the rest is ready.
};

} // namespace affresco::test
