#pragma once

/// @file
/// @brief format::Connections: the screen's declared connections (RFC 0003) as a services backend. An operation is
/// an HTTP request built from its declaration and the step's `with`; a message is a frame on the connection's
/// WebSocket, whose incoming messages run the actions "<connection>:<type>".

#include "affresco/core/Network.hpp"
#include "affresco/core/Services.hpp"

#include "ConnectionSpec.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief The connections a screen declares, and the requests and sockets running on them.
///
/// ServiceCalls starts each `request` and `subscribe` through it as it does a service's call: the same Call, so
/// results, events, loading, timeouts and `cancel` work alike. Every request is built from the declaration: the step
/// supplies only the arguments, so a document can't choose a host, a method or a header. Invariants: no request
/// leaves while the host gives no network (setNetwork()) or allows no connection's origin; at most
/// maxRequestsPerSecond requests per connection; while a proposal is previewed, nothing but a read operation without a
/// mock reaches a server (the others answer with their mock, or fail with "preview"); a request ended by its call
/// (cancelled, timed out) is cancelled on the network. Main thread only: the Network calls its handlers there.
class Connections final : public services::Services {
public:
	static constexpr std::size_t maxRequestsPerSecond = 50; ///< Requests one connection starts in a second.
	static constexpr std::size_t maxMessageBytes = std::size_t{8} << 20; ///< The largest message sent or received.

	/// @brief The connections of an engine.
	/// @param engine The runtime; outlives this object.
	explicit Connections(Engine& engine) : engine_(engine) {}
	Connections(const Connections&) = delete;
	Connections& operator=(const Connections&) = delete;
	Connections(Connections&&) = delete;
	Connections& operator=(Connections&&) = delete;
	/// @brief Cancels every request and closes every socket.
	~Connections() override;

	/// @brief Sets the network the requests go through (Screen::network()); the connections load again.
	/// @param network The network, which must outlive the runtime; nullptr for none (every connection is refused).
	void setNetwork(network::Network* network);
	/// @brief Whether the screen declares a connection.
	/// @param name Its name.
	/// @return Whether it does.
	[[nodiscard]] bool declares(std::string_view name) const { return connections_.contains(name); }
	/// @brief Cancels every request (their calls fail with "cancelled"), closes every socket and forgets the
	/// connections.
	void closeAll();

	/* services::Services */
	/// @brief Reads the "connections" section: requests running are cancelled, sockets close, and each connection's
	/// origin is checked against the host's allowlist; the allowed sockets connect.
	/// @param section The section (null: none).
	/// @return One line for each connection refused, or why the section is wrong.
	[[nodiscard]] std::vector<std::string> load(const json& section) override;
	[[nodiscard]] const services::MethodSpec* method(std::string_view service, std::string_view name) const override;
	[[nodiscard]] std::optional<std::string> invoke(std::string_view service, std::string_view name,
	                                                std::shared_ptr<services::Call> call) override;
	[[nodiscard]] std::optional<std::string> subscribe(std::string_view service, std::string_view name,
	                                                   std::shared_ptr<services::Call> call) override;
	/// @brief The `connections` variable: `{name: {online, pending, state, error}}`, with state "idle",
	/// "connecting", "open", "closed" or "refused".
	/// @return It.
	[[nodiscard]] json status() const override;
	void onStatus(std::function<void()> callback) override { onStatus_ = std::move(callback); }
	/// @brief Connections send no requests to answer: the handler is ignored.
	void onRequest(services::RequestHandler) override {}

private:
	/// @brief What a declared connection is doing.
	struct Live {
		ConnectionSpec spec;                                   ///< Its declaration.
		std::map<std::string, services::MethodSpec, std::less<>> methods; ///< Its operations as the services see them.
		std::string state = "idle";                        ///< See status().
		std::optional<std::string> error;                  ///< The last failure, or none.
		std::size_t pending = 0;                           ///< Requests running.
		bool online = true;                                ///< HTTP: the last request reached the server.
		std::deque<double> started;                        ///< When the requests of the last second started.
		std::unique_ptr<network::WebSocket> socket;        ///< Its WebSocket, while open or opening.
	};

	/// @brief A request running.
	struct Running {
		std::unique_ptr<network::Transfer> transfer;            ///< Cancelled when destroyed.
		std::shared_ptr<services::Call> call;                   ///< Where it reports.
		std::string connection;                                 ///< Its connection.
		bool ended = false;                                     ///< Answered or stopped: it waits to be forgotten.
		std::optional<std::stop_callback<std::function<void()>>> stop; ///< Ends it when its call is stopped.
	};

	/// @brief Starts a request or sends a message, or answers it from its mock while a proposal is previewed.
	/// @param service The connection.
	/// @param name The operation or message.
	/// @param call The call.
	/// @param stream Whether it is a subscription.
	/// @return Why it can't start, or nothing.
	std::optional<std::string> start(std::string_view service, std::string_view name,
	                                 const std::shared_ptr<services::Call>& call, bool stream);
	/// @brief Answers a call while a proposal is previewed, unless it may reach the server (a read without a mock).
	/// @param operation The operation.
	/// @param call The call: finished with the mock's body, or failed ("status" from a mock, else "preview").
	/// @param stream Whether it is a subscription.
	/// @return Whether it answered.
	bool previewed(const Operation& operation, services::Call& call, bool stream) const;
	/// @brief The network handlers of a request: they answer its call, note the connection's health, then end it.
	/// @param number The request.
	/// @param result The declaration's "result", or none.
	/// @param stream Whether its events go to the call.
	/// @return The handlers.
	[[nodiscard]] network::Handlers handlers(std::uint64_t number, const std::optional<json::json_pointer>& result,
	                                         bool stream);
	/// @brief Sends a message over its connection's socket.
	/// @param live The connection.
	/// @param operation The message.
	/// @param call The call, finished or failed at once.
	void sendMessage(Live& live, const Operation& operation, services::Call& call);
	/// @brief Builds an operation's request from its declaration and the call's arguments.
	/// @param live The connection.
	/// @param operation The operation.
	/// @param with The arguments.
	/// @return The request.
	/// @throws DocumentError If an argument can't be used (a path value that is missing, empty or not a scalar).
	[[nodiscard]] network::Request build(const Live& live, const Operation& operation, const json& with) const;
	/// @brief Whether a connection may start one more request now (maxRequestsPerSecond); counts it if so.
	/// @param live The connection.
	/// @return Whether it may.
	[[nodiscard]] bool admit(Live& live);
	/// @brief Forgets a request (after its handler returns, never inside it), and counts it done.
	/// @param number Its number.
	void end(std::uint64_t number);
	/// @brief Opens a connection's WebSocket.
	/// @param live The connection.
	void open(Live& live);
	/// @brief Handles a connection's WebSocket closing: its ":error" and ":closed" actions, then a reconnect if it asks.
	/// @param name The connection.
	/// @param failure Why, or nothing for a clean close.
	void closed(const std::string& name, const std::optional<network::Failure>& failure);
	/// @brief Runs the action "<connection>:<what>" with an event, if the screen has it.
	/// @param connection The connection.
	/// @param what "open", "closed", "error", or a message's type.
	/// @param event The event.
	void runAction(const std::string& connection, const std::string& what, const json& event);
	/// @brief Finds a connection, unless it was loaded again since.
	/// @param name Its name.
	/// @param generation The generation_ when the caller started.
	/// @return It, or nullptr.
	[[nodiscard]] Live* find(const std::string& name, std::uint64_t generation);
	/// @brief Keeps a transfer until the current task returns, then destroys it (never inside its own handler).
	/// @param transfer The transfer.
	void retire(std::unique_ptr<network::Transfer> transfer);
	/// @brief Calls the status callback.
	void changed() const;

	Engine& engine_;                                             ///< The runtime.
	network::Network* network_ = nullptr;                        ///< See setNetwork().
	std::map<std::string, Live, std::less<>> connections_;       ///< The connections, by name.
	std::map<std::uint64_t, Running> running_;                   ///< The requests running, by number.
	std::vector<std::unique_ptr<network::Transfer>> retired_;    ///< See retire().
	std::optional<json> section_;                                ///< The section loaded last.
	std::function<void()> onStatus_;                             ///< See onStatus().
	std::uint64_t next_ = 1;                                     ///< The next request's number.
	std::uint64_t generation_ = 1;                               ///< Bumped by each load: older reconnects do nothing.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Deferred work does nothing once it is reset.
};

} // namespace affresco::format
