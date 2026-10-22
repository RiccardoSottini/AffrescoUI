#pragma once

/// @file
/// @brief Delivering what the transfer threads find to the main thread, safely: each request or connection has one
/// Delivery, which the posted tasks hold (never the transfer itself). Its handlers are only touched, called and
/// destroyed on the main thread; once it is cancelled, nothing more is called.

#include "affresco/core/Executor.hpp"
#include "affresco/core/Network.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>

namespace affresco::net {

/// @brief The handlers of one transfer, and whether they may still be called.
template<typename Handlers> struct Delivery {
	/// @brief Holds the handlers.
	/// @param handlersToCall The handlers.
	explicit Delivery(Handlers handlersToCall) : handlers(std::move(handlersToCall)) {}

	std::atomic<bool> cancelled{false};      ///< The owner was destroyed, or the end was delivered.
	std::atomic<std::size_t> undelivered{0}; ///< Events or messages posted and not yet handled.
	Handlers handlers;                       ///< Main thread only.

	/// @brief Ends the delivery on the main thread: nothing more is called, and the handlers are released here rather
	/// than on a transfer thread.
	void cancel() {
		cancelled = true;
		handlers = {};
	}
};

/// @brief The delivery of an HTTP request.
using RequestDelivery = Delivery<network::Handlers>;
/// @brief The delivery of a WebSocket connection.
using SocketDelivery = Delivery<network::SocketHandlers>;

/// @brief Posts a request's failure (from any thread).
/// @param executor The main thread.
/// @param delivery The delivery.
/// @param failure The failure.
inline void postFailure(Executor& executor, const std::shared_ptr<RequestDelivery>& delivery, network::Failure failure) {
	// NOLINTNEXTLINE(readability-redundant-lambda-parameter-list): `] mutable` without () is C++23
	executor.post([delivery, failure = std::move(failure)]() mutable {
		if(delivery->cancelled) return;
		const auto failed = std::move(delivery->handlers.failed);
		delivery->cancel();
		if(failed) failed(std::move(failure));
	});
}

/// @brief Posts a request's response (from any thread).
/// @param executor The main thread.
/// @param delivery The delivery.
/// @param response The response.
inline void postResponse(Executor& executor, const std::shared_ptr<RequestDelivery>& delivery, network::Response response) {
	// NOLINTNEXTLINE(readability-redundant-lambda-parameter-list): `] mutable` without () is C++23
	executor.post([delivery, response = std::move(response)]() mutable {
		if(delivery->cancelled) return;
		const auto done = std::move(delivery->handlers.done);
		delivery->cancel();
		if(done) done(std::move(response));
	});
}

/// @brief Posts one event of a stream (from any thread), counted until it is handled.
/// @param executor The main thread.
/// @param delivery The delivery.
/// @param event The event.
inline void postEvent(Executor& executor, const std::shared_ptr<RequestDelivery>& delivery, network::StreamEvent event) {
	++delivery->undelivered;
	// NOLINTNEXTLINE(readability-redundant-lambda-parameter-list): `] mutable` without () is C++23
	executor.post([delivery, event = std::move(event)]() mutable {
		--delivery->undelivered;
		if(delivery->cancelled || !delivery->handlers.event) return;
		const auto handle = delivery->handlers.event; // a copy: the handler may destroy the transfer
		handle(std::move(event));
	});
}

} // namespace affresco::net
