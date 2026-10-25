/// @file
/// @brief format::ServiceCalls: service calls and subscriptions, their reports delivered through the Executor, and
/// the requests services send the screen.

#include "ServiceCalls.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <cstring>
#include <deque>
#include <functional>
#include <iterator>
#include <mutex>
#include <stop_token>
#include <utility>

namespace affresco::format {

namespace {

constexpr std::size_t maxRequests = 256; ///< Requests from services waiting for "respond" at once.

/// @brief An error as "onError" sees it.
/// @param code A short code, e.g. "cancelled".
/// @param message For people.
/// @param data More detail, or null.
/// @return {code, message, data}.
json errorOf(std::string code, std::string message, json data = nullptr) {
	return {{"code", std::move(code)}, {"message", std::move(message)}, {"data", std::move(data)}};
}

/// @brief Checks the name a service step calls: a service and its method, written out.
/// @param name The name.
/// @param at Its place, for the error.
void checkMethodName(const std::string& name, const std::string& at) {
	const std::size_t dot = name.find('.');
	if(name.contains("{{") || dot == std::string::npos || dot == 0 || dot + 1 == name.size())
		fail(at, "names a service and its method, written out: \"files.read\"");
}

/// @brief Reads a service step's "keep": how many of the latest events "into" keeps.
/// @param s The step.
/// @return 1 to ServiceCalls::maxKeep.
std::size_t keepOf(const Running& s) {
	const json keep = s.value("keep");
	const auto most = static_cast<long long>(ServiceCalls::maxKeep);
	if(!keep.is_number_integer() || keep.get<long long>() < 1 || keep.get<long long>() > most)
		fail(s.where + ".keep", "a whole number from 1 to 10000");
	return keep.get<std::size_t>();
}

/// @brief How long a call may run before it fails with "timeout".
/// @param s The step.
/// @param stream true for a subscription, which runs until it is cancelled.
/// @param connection Whether it goes through a connection, which times its requests out itself.
/// @return Milliseconds.
double timeoutOf(const Running& s, bool stream, bool connection) {
	if(!stream && s.step.contains("timeout"))
		return std::clamp(s.milliseconds("timeout"), 1.0, ServiceCalls::maxTimeout);
	return connection ? ServiceCalls::maxTimeout : ServiceCalls::defaultTimeout;
}

/// @brief Why a call's method can't be found.
/// @param name "service.method".
/// @param service The service.
/// @param connection Whether it goes through the connections.
/// @param declared Whether the screen declares a connection by the service's name.
/// @return The error's code and message.
std::pair<std::string, std::string> missingMethod(const std::string& name, const std::string& service, bool connection,
                                                  bool declared) {
	if(connection) return {"notFound", "no operation '" + name + "' (see the \"connections\" section)"};
	if(declared) return {"wrong", "'" + service + "' is a connection: use request"};
	return {"notFound", "no service method '" + name + "' (see the \"services\" section)"};
}

/// @brief Why a method can't be called this way: a stream invoked, or a call subscribed to.
/// @param spec The method.
/// @param name "service.method".
/// @param stream true for "subscribe".
/// @param connection Whether it goes through the connections.
/// @return The message, or "" if it can.
std::string wrongKind(const services::MethodSpec& spec, const std::string& name, bool stream, bool connection) {
	if(spec.streams == stream || spec.name == "*") return ""; // "*": a method the service doesn't list, either kind
	if(stream) return "'" + name + (connection ? "' is a call: use request" : "' is a call: use invoke");
	return "'" + name + "' is a stream: use subscribe";
}

} // namespace

/* ServiceCalls::Job */

/// @brief What a call shares with its service: the arguments, the stop source, and what the service reported since
/// the last delivery, under a mutex. A delivery is posted when there is something new and none waits already, so a
/// service that reports fast posts no more than the event loop runs; the oldest events beyond maxKeep are dropped.
struct ServiceCalls::Job final : services::Call {
	/// @brief A job.
	/// @param call Its call's number.
	/// @param with Its arguments.
	/// @param executor Where deliveries are posted, or nullptr (delivered by start()).
	/// @param calls The calls, used on the main thread only, while alive holds.
	/// @param alive Expires with them.
	Job(std::uint64_t call, json with, Executor* executor, ServiceCalls* calls, std::weak_ptr<bool> alive)
	    : id(call), with_(std::move(with)), executor_(executor), calls_(calls), alive_(std::move(alive)) {}

	[[nodiscard]] const json& arguments() const noexcept override { return with_; }
	[[nodiscard]] std::stop_token stopToken() const noexcept override { return stop_.get_token(); }
	void progress(json value) override {
		report([&value](Job& job) { job.progressed = std::move(value); });
	}
	void event(json value) override {
		report([&value](Job& job) {
			job.events.push_back(std::move(value));
			if(job.events.size() > maxKeep) job.events.pop_front();
		});
	}
	void finish(json value) override {
		report([&value](Job& job) {
			job.result = std::move(value);
			job.finished = true;
		});
	}
	void fail(std::string code, std::string message, json data) override {
		report([&](Job& job) {
			job.error = errorOf(std::move(code), std::move(message), std::move(data));
			job.finished = true;
		});
	}
	/// @brief Tells the service to stop; nothing it reports afterwards is kept.
	void stop() noexcept { stop_.request_stop(); }

	const std::uint64_t id;         ///< Its call.
	std::mutex mutex;               ///< Guards what follows.
	std::optional<json> progressed; ///< The latest progress not delivered yet.
	std::deque<json> events;        ///< The events not delivered yet, oldest first.
	std::optional<json> result;     ///< The result, once finished without an error.
	std::optional<json> error;      ///< The error, once finished with one.
	bool finished = false;          ///< Whether the service ended it.
	bool posted = false;            ///< Whether a delivery waits on the Executor.

private:
	/// @brief Records something new, and posts a delivery unless one waits already; nothing once stopped or ended.
	/// @param change Changes the job, under its mutex.
	void report(const std::function<void(Job&)>& change) {
		if(stop_.stop_requested()) return;
		bool post = false;
		{
			const std::scoped_lock lock(mutex);
			if(finished) return;
			change(*this);
			post = executor_ && !posted;
			posted = posted || post;
		}
		if(post)
			executor_->post([calls = calls_, alive = alive_, call = id] {
				if(alive.lock()) calls->deliver(call);
			});
	}

	json with_;                 ///< See arguments().
	std::stop_source stop_;     ///< See stopToken().
	Executor* executor_;        ///< See the constructor.
	ServiceCalls* calls_;       ///< See the constructor.
	std::weak_ptr<bool> alive_; ///< See the constructor.
};

/* ServiceCalls */

// NOLINTNEXTLINE(bugprone-exception-escape): cancelAll() only throws if memory runs out or a service's responder throws
ServiceCalls::~ServiceCalls() {
	cancelAll();
	setServices(nullptr);
}

void ServiceCalls::setServices(services::Services* services) {
	if(services_) {
		services_->onStatus({});
		services_->onRequest({});
	}
	services_ = services;
	loaded_.reset();
	if(!services_) return;
	services_->onStatus([this, alive = std::weak_ptr<bool>(alive_)] {
		if(!alive.lock()) return;
		engine_.document.globalsChanged(); // the "services" variable
		engine_.reconciler.requestReconcile();
	});
	services_->onRequest([this, alive = std::weak_ptr<bool>(alive_)](const std::string& service,
	                                                                 const std::string& method, const json& params,
	                                                                 services::Responder respond) {
		if(alive.lock()) request(service, method, params, std::move(respond));
		else respond(std::nullopt, "the screen closed");
	});
	load();
}

void ServiceCalls::load() {
	if(!engine_.window) return; // build() loads them first, once the host granted what they use
	const json& document = engine_.versions.document();
	for(const std::string& refused : engine_.connections.load(document.value("connections", json())))
		engine_.reportRefused("AffrescoUI screen: connections: " + refused); // nothing when the section didn't change
	if(!services_) return;
	const json section = document.contains("services") ? document["services"] : json();
	if(loaded_ && *loaded_ == section) return;
	loaded_ = section;
	for(const std::string& refused : services_->load(section))
		engine_.reportRefused("AffrescoUI screen: services: " + refused);
	engine_.document.globalsChanged(); // the "services" variable
}

ServiceCalls::Pending ServiceCalls::read(const Running& s, bool stream) const {
	const json& step = s.step;
	const char* key = stream ? "subscribe" : step.contains("request") ? "request" : "invoke";
	const std::string at = s.where + "." + key;
	const std::string name = string(step[key], at);
	checkMethodName(name, at);
	Pending call;
	call.name = name;
	call.stream = stream;
	call.connection =
	    !std::strcmp(key, "request") || (stream && engine_.connections.declares(name.substr(0, name.find('.'))));
	if(step.contains("id")) call.id = string(s.value("id"), s.where + ".id");
	if(step.contains("into")) call.into = s.target("into");
	if(step.contains("append")) call.append = s.target("append");
	if(step.contains("keep")) call.keep = keepOf(s);
	if(!stream) {
		if(step.contains("loading")) call.loading = s.target("loading");
		if(step.contains("progress")) call.progress = s.target("progress");
		call.onDone = step.value("onDone", json());
	}
	call.onEvent = step.value(stream ? "do" : "onEvent", json());
	call.onError = step.value("onError", json());
	call.sender = &s.sender;
	call.event = s.event;
	call.locals = snapshot(s.locals);
	call.where = s.where;
	return call;
}

bool ServiceCalls::start(const Running& s, bool stream) {
	Pending call = read(s, stream);
	const json arguments = s.step.contains("with") ? s.value("with") : json::object();
	const double timeout = timeoutOf(s, stream, call.connection);
	if(!call.id.empty()) cancel(call.id); // the newer call wins
	const bool full = busy(stream);

	const std::uint64_t number = next_++;
	const auto job = std::make_shared<Job>(number, arguments, executor_, this, alive_);
	call.job = job;
	if(call.loading) write(*call.loading, true, call.where + ".loading");
	const std::string name = call.name;
	const bool connection = call.connection;
	pending_.emplace(number, std::move(call));
	if(full)
		job->fail("busy", stream ? "too many subscriptions (at most 256)" : "too many calls running (at most 1024)",
		          nullptr);
	else launch(number, name, stream, connection, *job);
	const auto started = pending_.find(number); // a service may have ended it already
	if(!stream && engine_.window && started != pending_.end())
		started->second.timeout = engine_.scheduler.once(timeout, [this, number, alive = std::weak_ptr<bool>(alive_)] {
			if(alive.lock()) failCall(number, errorOf("timeout", "the call took too long"));
		});
	if(!executor_) deliver(number); // what a service reported before it returned; nothing after
	if(engine_.document.stateChanged()) engine_.reconciler.requestStateUpdate();
	return true;
}

void ServiceCalls::launch(std::uint64_t number, const std::string& name, bool stream, bool connection, Job& job) {
	const std::size_t dot = name.find('.');
	const std::string service = name.substr(0, dot), method = name.substr(dot + 1);
	services::Services* backend = connection ? &engine_.connections : services_;
	const services::MethodSpec* spec = backend ? backend->method(service, method) : nullptr;
	if(!spec) {
		auto [code, message] = missingMethod(name, service, connection, engine_.connections.declares(service));
		return job.fail(std::move(code), std::move(message), nullptr);
	}
	if(std::string wrong = wrongKind(*spec, name, stream, connection); !wrong.empty())
		return job.fail("wrong", std::move(wrong), nullptr);
	if(!connection && spec->effect != services::Effect::Read && engine_.versions.previewing()) // connections mock
		return job.fail("preview", "while a proposal is previewed, a service only reads", nullptr);
	const std::shared_ptr<Job> shared = pending_.at(number).job;
	const std::optional<std::string> error =
	    stream ? backend->subscribe(service, method, shared) : backend->invoke(service, method, shared);
	if(error) job.fail("refused", *error, nullptr);
}

bool ServiceCalls::busy(bool stream) const {
	if(pending_.size() >= maxRunning) return true;
	const auto subscriptions = std::ranges::count_if(pending_, [](const auto& entry) { return entry.second.stream; });
	return stream && std::cmp_greater_equal(subscriptions, maxSubscriptions);
}

bool ServiceCalls::cancel(const std::string& id) {
	const auto it = std::ranges::find_if(pending_, [&id](const auto& entry) { return entry.second.id == id; });
	if(it == pending_.end()) return false;
	failCall(it->first, errorOf("cancelled", "the call was cancelled"));
	return true;
}

void ServiceCalls::forgetSender(const Panel& panel) {
	if(pending_.empty()) return;
	std::vector<json::json_pointer> loading;
	for(auto it = pending_.begin(); it != pending_.end();) {
		if(it->second.sender != &panel) {
			++it;
			continue;
		}
		it->second.job->stop();
		if(it->second.loading) loading.push_back(*it->second.loading);
		it = pending_.erase(it);
	}
	if(loading.empty() || !engine_.window) return;
	engine_.scheduler.defer([this, loading = std::move(loading), alive = std::weak_ptr<bool>(alive_)] {
		if(!alive.lock()) return;
		for(const json::json_pointer& pointer : loading) write(pointer, false, "loading");
		flush();
	}); // not while the reconciler removes panels
}

void ServiceCalls::cancelAll() {
	for(auto& [number, call] : pending_) call.job->stop();
	pending_.clear();
	std::map<std::uint64_t, Request> waiting = std::exchange(requests_, {});
	alive_ = std::make_shared<bool>(true); // deliveries posted until now do nothing
	for(auto& [number, waitingRequest] : waiting) waitingRequest.respond(std::nullopt, "the screen closed");
}

void ServiceCalls::deliver(std::uint64_t call) {
	const auto it = pending_.find(call);
	if(it == pending_.end()) return; // cancelled, timed out or ended
	const std::shared_ptr<Job> job = it->second.job;
	std::optional<json> progressed;
	std::deque<json> events;
	bool finished = false;
	{
		const std::scoped_lock lock(job->mutex);
		progressed = std::exchange(job->progressed, std::nullopt);
		events = std::exchange(job->events, {});
		finished = job->finished;
		job->posted = false;
	}
	if(const auto& progress = it->second.progress; progressed && progress)
		write(*progress, std::move(*progressed), it->second.where + ".progress");
	if(!events.empty())
		deliverEvents(
		    call, std::vector<json>(std::make_move_iterator(events.begin()), std::make_move_iterator(events.end())));
	const auto still = pending_.find(call); // the steps may have cancelled it
	if(finished && still != pending_.end()) {
		const Pending ended = finish(still);
		if(job->error) {
			failed(ended, *job->error);
		} else if(job->result) {
			if(ended.into && !ended.stream) write(*ended.into, *job->result, ended.where + ".into");
			if(!ended.onDone.is_null()) runSteps(ended, ended.onDone, "result", *job->result, ended.where + ".onDone");
		}
	}
	flush();
}

void ServiceCalls::deliverEvents(std::uint64_t call, std::vector<json> events) {
	const Pending& running = pending_.at(call);
	if(running.append) {
		try {
			json& list = statePlace(engine_.versions.state(), *running.append, running.where + ".append");
			if(!list.is_array()) list = json::array();
			for(const json& event : events) { // copied: `into` and `do` get them too
				expect::shallowEnough(event, pointerDepth(*running.append) + 1, running.where + ".append");
				list.push_back(event);
			}
			if(list.size() > running.keep)
				list.erase(list.begin(), list.begin() + static_cast<std::ptrdiff_t>(list.size() - running.keep));
			engine_.document.markStateChanged(*running.append);
		} catch(const std::exception& error) { // a revision may have changed the state since the call started
			engine_.report(error.what());
		}
	}
	if(running.into && running.stream) write(*running.into, events.back(), running.where + ".into");
	if(running.onEvent.is_null()) return;
	const char* key = running.stream ? ".do" : ".onEvent";
	for(json& event : events) {
		const auto it = pending_.find(call);
		if(it == pending_.end()) return; // a step ended it
		runSteps(it->second, it->second.onEvent, "event", std::move(event), it->second.where + key);
	}
}

void ServiceCalls::failCall(std::uint64_t call, const json& error) {
	const auto it = pending_.find(call);
	if(it == pending_.end()) return;
	failed(finish(it), error);
	flush();
}

void ServiceCalls::failed(const Pending& call, const json& error) {
	json flat; // a connection's error, RFC 0003: {code, message, status, body}
	if(call.connection && error["data"].is_object())
		flat = {{"code", error["code"]},
		        {"message", error["message"]},
		        {"status", error["data"].value("status", json())},
		        {"body", error["data"].value("body", json())}};
	const json& shown = flat.is_null() ? error : flat;
	if(call.onError.is_null()) {
		if(shown["code"] == "cancelled") return; // cancelled on purpose (a "cancel" step, a new call with its id)
		const std::string& message = shown["message"].get_ref<const std::string&>();
		engine_.report(call.where + ": " + call.name + ": " + message);
	} else {
		runSteps(call, call.onError, "error", shown, call.where + ".onError");
	}
}

ServiceCalls::Pending ServiceCalls::finish(std::map<std::uint64_t, Pending>::iterator it) {
	Pending call = std::move(it->second);
	pending_.erase(it);
	call.job->stop();
	if(call.timeout) engine_.scheduler.cancelOnce(call.timeout); // or each frame would look at it
	if(call.loading) write(*call.loading, false, call.where + ".loading");
	if(call.progress) write(*call.progress, nullptr, call.where + ".progress");
	return call;
}

void ServiceCalls::write(const json::json_pointer& pointer, json value, const std::string& where) {
	try {
		expect::shallowEnough(value, pointerDepth(pointer), where);
		statePlace(engine_.versions.state(), pointer, where) = std::move(value);
		engine_.document.markStateChanged(pointer);
	} catch(const std::exception& error) { // a revision may have changed the state since the call started
		engine_.report(error.what());
	}
}

void ServiceCalls::runSteps(const Pending& call, const json& steps, const char* variable, json value,
                            const std::string& where) {
	Window* window = engine_.window;
	Panel* sender = engine_.reconciler.recordOf(call.sender) ? call.sender : nullptr;
	if(!sender && window) sender = &window->root();
	if(!sender) return; // nothing built: no panel to run them from
	// Copies on purpose: the steps may end the call they come from, and with it these. A stream's event is the steps'
	// `event` (the name always means the event, never a local); the other steps see the event that started the call.
	const bool streamed = std::strcmp(variable, "event") == 0;
	const json handler = steps;      // NOLINT(performance-unnecessary-copy-initialization)
	const std::string place = where; // NOLINT(performance-unnecessary-copy-initialization)
	const json event = streamed ? value : call.event;
	const auto scope = std::make_shared<const Scope>(
	    Scope{call.locals.get(), call.locals, json{{variable, std::move(value)}}, {}, {}});
	engine_.runner.handle(handler, *sender, event, *scope, place);
}

void ServiceCalls::request(const std::string& service, const std::string& method, const json& params,
                           services::Responder respond) {
	const std::string action = service + ":" + method;
	if(!engine_.window) return respond(std::nullopt, "the screen isn't shown");
	if(!engine_.owner.hasAction(action)) return respond(std::nullopt, std::string(services::methodNotFound));
	if(requests_.size() >= maxRequests) return respond(std::nullopt, "too many requests waiting (at most 256)");
	const std::uint64_t number = next_++;
	requests_.emplace(number, Request{std::move(respond), service, method});
	engine_.scheduler.once(requestTimeout, [this, number, alive = std::weak_ptr<bool>(alive_)] {
		if(alive.lock()) refuseRequest(number, "the screen didn't respond in 30 s");
	});
	const Scope scope{
	    nullptr, nullptr, {{"request", {{"service", service}, {"method", method}, {"id", number}}}}, {}, {}};
	engine_.runner.handle(json(action), engine_.window->root(), params, scope, "services." + service + ".request");
	flush();
}

bool ServiceCalls::respond(const Running& s) {
	const json* request = s.locals.find("request");
	const auto it = request && request->is_object() && (*request)["id"].is_number_unsigned()
	                    ? requests_.find((*request)["id"].get<std::uint64_t>())
	                    : requests_.end();
	if(!request || !request->is_object()) fail(s.where + ".respond", "runs only in an action a service's request runs");
	if(it == requests_.end()) fail(s.where + ".respond", "the request was answered already, or timed out");
	const services::Responder responder = std::move(it->second.respond);
	requests_.erase(it);
	if(s.step.contains("error")) responder(std::nullopt, string(s.value("error"), s.where + ".error"));
	else responder(s.value("respond"), {});
	return true;
}

void ServiceCalls::refuseRequest(std::uint64_t id, const std::string& error) {
	const auto it = requests_.find(id);
	if(it == requests_.end()) return;
	const services::Responder responder = std::move(it->second.respond);
	requests_.erase(it);
	responder(std::nullopt, error);
}

void ServiceCalls::flush() {
	if(engine_.document.stateChanged()) engine_.reconciler.requestStateUpdate();
}

} // namespace affresco::format
