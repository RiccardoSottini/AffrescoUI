#pragma once

/// @file
/// @brief LoopbackRedirect: where a browser sign-in returns (RFC 8252 §7.3). It listens on 127.0.0.1, on a port the
/// system picks, for one request to exactly "/callback" whose "state" matches; it answers the browser with a short
/// page and hands the authorization code to the main thread.

#include "affresco/core/Executor.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace affresco::net {

namespace tcp {
class Connection;
class Listener;
} // namespace tcp

/// @brief A one-time listener for a browser sign-in's redirect.
///
/// It authenticates its one caller by the state: a random value only the browser that was sent to the provider
/// carries back. Requests elsewhere than "/callback" are answered 404 and ignored; a request to "/callback" with a
/// wrong or missing state ends the sign-in. Every request is bounded (8 KiB, 10 seconds).
class LoopbackRedirect {
public:
	/// @brief What it hands back, on the main thread: the code, or nullopt and an error for people.
	using Received = std::function<void(std::optional<std::string> code, std::string error)>;

	/// @brief Starts listening.
	/// @param executor Runs `received` on the main thread; outlives this object.
	/// @param state The state the redirect must carry.
	/// @param received Called once, unless this object is destroyed first.
	/// @param error Receives why it couldn't listen.
	/// @return The listener, or nullptr.
	[[nodiscard]] static std::unique_ptr<LoopbackRedirect> open(Executor& executor, std::string state,
	                                                            Received received, std::string& error);
	/// @brief Stops listening and waits for its thread; `received` is not called afterwards.
	~LoopbackRedirect();
	LoopbackRedirect(const LoopbackRedirect&) = delete;
	LoopbackRedirect& operator=(const LoopbackRedirect&) = delete;
	LoopbackRedirect(LoopbackRedirect&&) = delete;
	LoopbackRedirect& operator=(LoopbackRedirect&&) = delete;

	/// @brief The redirect URI to send: "http://127.0.0.1:<port>/callback".
	[[nodiscard]] std::string uri() const;

private:
	/// @brief Takes a listening socket.
	/// @param executor See open().
	/// @param listener The socket.
	/// @param state See open().
	/// @param received See open().
	LoopbackRedirect(Executor& executor, std::unique_ptr<tcp::Listener> listener, std::string state,
	                 Received received);
	/// @brief The thread: answers requests until one to "/callback" came, or close.
	void serve();
	/// @brief Answers one request.
	/// @param connection The request's connection.
	/// @return Whether it was the redirect (the thread then ends).
	bool answer(tcp::Connection& connection);
	/// @brief Ends the sign-in with the redirect's code, or with the provider's refusal, and answers the browser.
	/// @param connection The redirect's connection.
	/// @param query Its query, whose state was checked.
	void takeCode(tcp::Connection& connection, std::string_view query);
	/// @brief Hands the outcome to the main thread, once.
	/// @param code The code, or nullopt.
	/// @param error Why not, or empty.
	void deliver(std::optional<std::string> code, std::string error);

	Executor& executor_;                                       ///< See open().
	std::unique_ptr<tcp::Listener> listener_;                  ///< The socket.
	std::string state_;                                        ///< See open().
	Received received_;                                        ///< See open().
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Posted outcomes do nothing once it is reset.
	std::mutex mutex_;                                         ///< Guards current_.
	tcp::Connection* current_ = nullptr;                       ///< The request being read, closed by the destructor.
	std::jthread thread_;                                      ///< Last: it starts once the rest is ready.
};

} // namespace affresco::net
