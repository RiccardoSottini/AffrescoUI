#pragma once

/// @file
/// @brief HTTP for a worker thread: a Network that belongs to that thread (its handlers run there, from a queue it
/// pumps), allowing one origin only, and a request that waits for its outcome while it can be stopped.

#include <affresco/core/Executor.hpp>
#include <affresco/core/Network.hpp>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <variant>

namespace affresco::intelligence {

/// @brief Makes the Network a session uses, given the Executor its handlers must be posted to.
using NetworkFactory = std::function<std::unique_ptr<affresco::network::Network>(affresco::Executor&)>;

/// @brief The net module's Client: TLS verified, every byte bounded (modules/net/README.md).
/// @return A factory of Clients.
[[nodiscard]] NetworkFactory systemNetwork();

/// @brief An Executor whose tasks run on the thread that pumps it.
class PumpedExecutor final : public affresco::Executor {
public:
	/// @brief Queues a task; safe from any thread.
	/// @param task The task.
	void post(std::function<void()> task) override;
	/// @brief Runs the tasks queued, waiting at most a while for the first.
	/// @param wait The longest to wait for one.
	void pump(std::chrono::milliseconds wait);

private:
	std::mutex mutex_;                        ///< Guards tasks_.
	std::condition_variable wake_;            ///< Told of a new task.
	std::deque<std::function<void()>> tasks_; ///< Queued, not yet run.
};

/// @brief The outcome of a request: its response or its failure.
using Outcome = std::variant<affresco::network::Response, affresco::network::Failure>;

/// @brief Requests to one origin, from one thread (the one that made it): the session's Network allows that origin
/// only, so a URL elsewhere (a redirect, a base URL changed under it) is refused before anything is sent.
class HttpSession {
public:
	/// @brief Makes the Network and allows the origin.
	/// @param factory Makes the Network.
	/// @param origin E.g. "https://api.anthropic.com" (http only on the loopback, as the net module enforces).
	/// @throws std::runtime_error If the origin isn't one the Network takes.
	HttpSession(const NetworkFactory& factory, const std::string& origin);
	/// @brief Cancels what is in flight.
	~HttpSession();
	HttpSession(const HttpSession&) = delete;
	HttpSession& operator=(const HttpSession&) = delete;
	HttpSession(HttpSession&&) = delete;
	HttpSession& operator=(HttpSession&&) = delete;

	/// @brief Sends a request and waits for its outcome; on the thread that made the session.
	/// @param request The request (its timeout bounds the wait).
	/// @param stopped Asked every 50 ms; true cancels the request (a "cancelled" failure).
	/// @return The outcome.
	[[nodiscard]] Outcome fetch(affresco::network::Request request, const std::function<bool()>& stopped);

private:
	PumpedExecutor executor_;                             ///< Runs the Network's handlers on this thread.
	std::unique_ptr<affresco::network::Network> network_; ///< Allows the one origin.
};

/// @brief The origin of a URL: its scheme, host and port.
/// @param url E.g. "https://api.openai.com/v1".
/// @return E.g. "https://api.openai.com", or nullopt if it isn't an http(s) URL.
[[nodiscard]] std::optional<std::string> originOf(const std::string& url);

} // namespace affresco::intelligence
