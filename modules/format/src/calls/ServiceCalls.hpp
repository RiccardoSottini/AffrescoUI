#pragma once

/// @file
/// @brief format::ServiceCalls: the "invoke", "request" and "subscribe" steps (RFC 0002, 0003), their results, events
/// and progress delivered into the state on the main thread, and the requests services send the screen ("respond").

#include "affresco/core/Executor.hpp"
#include "affresco/core/Services.hpp"

#include "Expression.hpp"
#include "StepRegistry.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief The service calls and subscriptions of a runtime, and the requests its services are waiting on.
///
/// A step starts a call through a backend found by the name before its first dot: the screen's Services (setServices())
/// for a declared service ("invoke"), its Connections for a declared connection ("request"; "subscribe" to either).
/// Everything the call reports arrives through the Executor, coalesced per frame: progress, events (appended to a list,
/// or run through "onEvent"/"do" one batch at a time), then the result ("into", "onDone") or the error ("onError", or
/// reported). Invariants: at most maxRunning calls and maxSubscriptions subscriptions at once; a call with an "id"
/// cancels the running one with the same id; once ended (finished, failed, cancelled, timed out, its node removed,
/// cancelAll()) a call changes nothing more, whatever its service still delivers. Main thread only, except what
/// services reach through the jobs.
class ServiceCalls {
public:
	static constexpr std::size_t maxRunning = 1024;      ///< Calls running at once, per screen.
	static constexpr std::size_t maxSubscriptions = 256; ///< Subscriptions at once, per screen.
	static constexpr std::size_t defaultKeep = 1000;     ///< "keep" when the step has none.
	static constexpr std::size_t maxKeep = 10000;        ///< The largest "keep"; also the events a call queues.
	static constexpr double defaultTimeout = 30000;      ///< "timeout", in milliseconds, when the step has none.
	static constexpr double maxTimeout = 3600000;        ///< The largest "timeout": an hour.
	static constexpr double requestTimeout = 30000;      ///< How long a service's request waits for "respond".

	/// @brief The calls of an engine.
	/// @param engine The runtime; outlives this object.
	explicit ServiceCalls(Engine& engine) : engine_(engine) {}
	ServiceCalls(const ServiceCalls&) = delete;
	ServiceCalls& operator=(const ServiceCalls&) = delete;
	ServiceCalls(ServiceCalls&&) = delete;
	ServiceCalls& operator=(ServiceCalls&&) = delete;
	/// @brief Cancels every call and answers every waiting request with an error.
	~ServiceCalls();

	/// @brief Sets where results are posted (the Application, from Runtime::createWindow()).
	/// @param executor The executor, which must outlive the runtime; without one, only what a service reports before
	/// its invoke() returns is delivered.
	void setExecutor(Executor* executor) noexcept { executor_ = executor; }
	/// @brief Sets the screen's services, and takes their status changes and requests.
	/// @param services The services, which must outlive the runtime (or be replaced first); nullptr for none.
	void setServices(services::Services* services);
	/// @brief The screen's services.
	/// @return Them, or nullptr.
	[[nodiscard]] services::Services* services() const noexcept { return services_; }
	/// @brief Loads the document's "services" section into the services, if it changed since the last load; each
	/// service refused is reported.
	void load();

	/// @brief Starts an "invoke", "request" or "subscribe" step.
	/// @param s The step.
	/// @param stream true for "subscribe".
	/// @return Whether to keep going (always true: the call is asynchronous).
	/// @throws DocumentError If the step is wrong: a computed or malformed name, a path the state can't take.
	bool start(const Running& s, bool stream);
	/// @brief Cancels the call or subscription with an id: its "onError" runs with the code "cancelled".
	/// @param id The id.
	/// @return Whether there was one.
	bool cancel(const std::string& id);
	/// @brief Ends the calls started from a panel that is being removed (Reconciler::forgetSubtree()).
	/// @param panel The panel.
	void forgetSender(const Panel& panel);
	/// @brief Cancels every call, which then changes nothing, and answers waiting requests with an error (~Runtime).
	void cancelAll();
	/// @brief Answers a request a service sent (the "respond" step).
	/// @param s The step; its scope holds `request`.
	/// @return Whether to keep going.
	/// @throws DocumentError If the step isn't run for a request, or the request was answered already.
	bool respond(const Running& s);

	struct Job;

private:
	/// @brief What the main thread keeps of a running call.
	struct Pending {
		std::shared_ptr<Job> job;                   ///< What the service shares.
		std::string name;                           ///< "service.method".
		std::string id;                             ///< "id", or empty.
		bool stream = false;                        ///< A subscription.
		bool connection = false;                    ///< Through the screen's connections, not its services.
		std::optional<json::json_pointer> into;     ///< "into".
		std::optional<json::json_pointer> append;   ///< "append".
		std::size_t keep = defaultKeep;             ///< "keep".
		std::optional<json::json_pointer> loading;  ///< "loading".
		std::optional<json::json_pointer> progress; ///< "progress".
		json onEvent;                               ///< "onEvent" or "do" steps, or null.
		json onDone;                                ///< "onDone" steps, or null.
		json onError;                               ///< "onError" steps, or null.
		Panel* sender = nullptr;                    ///< The panel it comes from, or nullptr for the document.
		json event;                                 ///< The event it was started by.
		SharedScope locals;                         ///< Variables.
		std::string where;                          ///< The step's place.
	};

	/// @brief A service's request waiting for "respond".
	struct Request {
		services::Responder respond; ///< Answers it, once.
		std::string service;         ///< The service.
		std::string method;          ///< The method it asks for.
	};

	/// @brief Reads a step into a Pending: "invoke" names a service, "request" a connection, and "subscribe" a
	/// connection if the screen declares one by that name, else a service.
	/// @param s The step.
	/// @param stream true for "subscribe".
	/// @return The call, its job not made yet.
	[[nodiscard]] Pending read(const Running& s, bool stream) const;
	/// @brief Applies what a job reported since the last delivery (main thread, through the Executor).
	/// @param call The call's number.
	void deliver(std::uint64_t call);
	/// @brief Asks the service to start a call, or fails it (through its job, so always later): no such method, a
	/// stream invoked or a call subscribed to, a method that changes the world while a proposal is previewed.
	/// @param number The call's number (in pending_).
	/// @param name "service.method".
	/// @param stream true for "subscribe".
	/// @param connection Whether it goes through the connections.
	/// @param job Its job.
	void launch(std::uint64_t number, const std::string& name, bool stream, bool connection, Job& job);
	/// @brief Tells whether a new call would be one too many: maxRunning calls, or maxSubscriptions subscriptions.
	/// @param stream true for a subscription.
	/// @return true if it would.
	[[nodiscard]] bool busy(bool stream) const;
	/// @brief Applies a batch of events: appended, kept as the latest, or run through the call's steps.
	/// @param call The call's number (running).
	/// @param events The events, oldest first.
	void deliverEvents(std::uint64_t call, std::vector<json> events);
	/// @brief Ends a call with an error and runs its "onError" (or reports it).
	/// @param call The call's number.
	/// @param error {code, message, data}.
	void failCall(std::uint64_t call, const json& error);
	/// @brief Runs an ended call's "onError", or reports its error.
	/// @param call The call.
	/// @param error {code, message, data}.
	void failed(const Pending& call, const json& error);
	/// @brief Ends a call: forgotten, its job stopped, its "loading" false and its "progress" null.
	/// @param it The call.
	/// @return What it kept.
	Pending finish(std::map<std::uint64_t, Pending>::iterator it);
	/// @brief Writes a value into the state at a path; a path the state can't take is reported.
	/// @param pointer The path.
	/// @param value The value.
	/// @param where Its place, for errors.
	void write(const json::json_pointer& pointer, json value, const std::string& where);
	/// @brief Runs a call's steps with one more variable, from its panel (or the root, for the document's calls).
	/// @param call The call.
	/// @param steps The steps.
	/// @param variable The variable's name.
	/// @param value Its value.
	/// @param where Their place.
	void runSteps(const Pending& call, const json& steps, const char* variable, json value, const std::string& where);
	/// @brief Runs the action "<service>:<method>" for a service's request, or answers that nothing handles it.
	/// @param service The service.
	/// @param method The method.
	/// @param params Its parameters, the action's `event`.
	/// @param respond Answers it.
	void request(const std::string& service, const std::string& method, const json& params,
	             services::Responder respond);
	/// @brief Answers a waiting request with an error, if it still waits.
	/// @param id The request's number.
	/// @param error Why.
	void refuseRequest(std::uint64_t id, const std::string& error);
	/// @brief Calls finished state changes to the screen: one update.
	void flush();

	Engine& engine_;                                             ///< The runtime.
	Executor* executor_ = nullptr;                               ///< See setExecutor().
	services::Services* services_ = nullptr;                     ///< See setServices().
	std::map<std::uint64_t, Pending> pending_;                   ///< The calls running, by number.
	std::map<std::uint64_t, Request> requests_;                  ///< Requests waiting for "respond", by number.
	std::optional<json> loaded_;                                 ///< The section load() loaded last.
	std::uint64_t next_ = 1;                                     ///< The next call's or request's number.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Posted deliveries do nothing once it is reset.
};

} // namespace affresco::format
