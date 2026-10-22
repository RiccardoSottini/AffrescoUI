#pragma once

/// @file
/// @brief What Forge's language server client (Lsp.cpp) sends with: a call to a server's service, answered on the main
/// thread, the arguments of the screen's calls, and columns in UTF-16 units.

#include <affresco/core/Executor.hpp>
#include <affresco/core/Json.hpp>
#include <affresco/core/Services.hpp>

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

namespace forge::lsp {

/// @brief A call to a server whose answer is handled on the main thread, while the client is alive.
class ServerCall final : public affresco::services::Call {
public:
	/// @brief Gets the result, or no result and the error.
	using Done = std::function<void(std::optional<affresco::json> result, const std::string& error)>;
	/// @brief Gets one event of a subscription.
	using Events = std::function<void(const affresco::json& event)>;

	/// @brief A call.
	/// @param arguments The params sent.
	/// @param executor The main thread; nullptr drops what arrives.
	/// @param alive The client's guard.
	/// @param done Gets the result (may be empty).
	/// @param events Gets a subscription's events (may be empty).
	ServerCall(affresco::json arguments, affresco::Executor* executor, std::weak_ptr<bool> alive, Done done,
	           Events events = {})
	    : arguments_(std::move(arguments)), executor_(executor), alive_(std::move(alive)), done_(std::move(done)),
	      events_(std::move(events)) {}

	const affresco::json& arguments() const noexcept override { return arguments_; }
	std::stop_token stopToken() const noexcept override { return stop_.get_token(); }
	void progress(affresco::json) override {}
	void event(affresco::json event) override {
		if(!events_ || ended_ || stop_.stop_requested()) return;
		post([events = events_, event = std::move(event)] { events(event); });
	}
	void finish(affresco::json result) override {
		if(ended_.exchange(true) || !done_) return;
		post([done = done_, result = std::move(result)] { done(result, {}); });
	}
	void fail(std::string, std::string message, affresco::json) override {
		if(ended_.exchange(true) || !done_) return;
		post([done = done_, message = std::move(message)] { done(std::nullopt, message); });
	}
	/// @brief Asks the service to stop it (a subscription of a server that is reset).
	void stop() { stop_.request_stop(); }

private:
	/// @brief Runs a task on the main thread, if the client is still alive then.
	/// @param task The task.
	void post(std::function<void()> task) {
		if(executor_)
			executor_->post([alive = alive_, task = std::move(task)] {
				if(alive.lock()) task();
			});
	}

	affresco::json arguments_;        ///< See arguments().
	affresco::Executor* executor_;    ///< See the constructor.
	std::weak_ptr<bool> alive_;       ///< See the constructor.
	Done done_;                       ///< See the constructor.
	Events events_;                   ///< See the constructor.
	std::stop_source stop_;           ///< See stopToken().
	std::atomic<bool> ended_ = false; ///< After finish() or fail().
};

/// @brief A text argument of a call.
/// @param with The arguments.
/// @param key The argument.
/// @param bytes At most this long.
/// @return It, or "".
/// @throws std::runtime_error If it is longer.
[[nodiscard]] std::string textArgument(const affresco::json& with, const char* key, std::size_t bytes);

/// @brief A positive number argument of a call.
/// @param with The arguments.
/// @param key The argument.
/// @return It, or 1 if it isn't one.
[[nodiscard]] std::size_t positionArgument(const affresco::json& with, const char* key);

/// @brief UTF-16 units of the first code points of a line.
/// @param line The line.
/// @param columns How many code points.
/// @return Their units.
[[nodiscard]] std::size_t utf16Units(std::u32string_view line, std::size_t columns);

/// @brief The code points of a line that make up a number of UTF-16 units.
/// @param line The line.
/// @param units The units.
/// @return The code points (a unit inside a pair counts its whole code point).
[[nodiscard]] std::size_t codePointsOf(std::u32string_view line, std::size_t units);

} // namespace forge::lsp
