/// @file
/// @brief The `jsonrpc` provider: a granted program spoken to in JSON-RPC 2.0 over its stdin and stdout, framed LSP's
/// way or one message per line. Calls wait by id, `$/cancelRequest` cancels them, `$/progress` and notifications that
/// name a call reach it, other notifications reach the streams subscribed to them, and the program's own requests go
/// to the screen.

#include "rpc/Framing.hpp"
#include "process/Process.hpp"
#include "Providers.hpp"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>

namespace affresco::services {

namespace {

constexpr std::size_t mostWaiting = 256;                     ///< Calls waiting for an answer, per service.
constexpr std::size_t mostSubscriptions = 256;               ///< Subscriptions, per service.
constexpr std::size_t largestOutbox = std::size_t{16} << 20; ///< Bytes waiting to be written, at most 16 MiB.
constexpr std::size_t longestMessage = 1000;                 ///< An error message from the program, cut to 1000 bytes.
constexpr int internalError = -32603;                        ///< JSON-RPC's InternalError.
constexpr int methodNotFoundCode = -32601;                   ///< JSON-RPC's MethodNotFound.

/// @brief A call waiting for its answer.
struct Waiting {
	std::shared_ptr<Call> call;                                       ///< The call.
	std::unique_ptr<std::stop_callback<std::function<void()>>> watch; ///< Sends `$/cancelRequest` when it stops.
};

/// @brief What the service's threads and the responders it hands out share; responders keep it by weak pointer.
struct Link {
	std::mutex mutex;                          ///< Guards everything below.
	std::condition_variable wake;              ///< The writer waits on it.
	std::deque<std::string> outbox;            ///< Framed messages to write.
	std::size_t outboxBytes = 0;               ///< Their size.
	bool closed = false;                       ///< The service is ending, or the program went away.
	std::int64_t nextId = 1;                   ///< The next call's id.
	std::map<std::int64_t, Waiting> waiting;   ///< Calls by id.
	std::multimap<std::string, std::shared_ptr<Call>, std::less<>> streams; ///< Subscriptions by method.
	Framing framing = Framing::ContentLength;  ///< How messages are framed.

	/// @brief Queues a message to write.
	/// @param message The message.
	/// @return false if the link is closed or the outbox is full.
	bool send(const json& message) {
		std::string bytes = frameMessage(framing, message.dump());
		const std::scoped_lock lock(mutex);
		if(closed || outboxBytes + bytes.size() > largestOutbox) return false;
		outboxBytes += bytes.size();
		outbox.push_back(std::move(bytes));
		wake.notify_one();
		return true;
	}
};

/// @brief The answer to the program's request, as the screen gave it.
/// @param id The request's id.
/// @param value The value, or std::nullopt.
/// @param error Why not, if there is no value.
/// @return The response.
json response(const json& id, const std::optional<json>& value, const std::string& error) {
	if(value) return {{"jsonrpc", "2.0"}, {"id", id}, {"result", *value}};
	const int code = error == methodNotFound ? methodNotFoundCode : internalError;
	return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", error}}}};
}

/// @brief Text from the program, for a message: cut to longestMessage, and only if it is UTF-8.
/// @param value The value.
/// @return The text.
std::string messageOf(const json& value) {
	if(!value.is_string()) return "the service answered with an error";
	const std::string text = value.get<std::string>().substr(0, longestMessage);
	return isUtf8(text) ? text : "the service answered with an error";
}

/// @brief One `jsonrpc` service: its program, a reader and a writer thread, and the calls waiting.
class JsonRpcService final : public Service {
public:
	/// @brief Starts the program and its threads.
	/// @param launch What to run.
	/// @param framing How messages are framed.
	/// @param context What the service works with.
	/// @param notifications The methods sent as notifications: no id, and the call ends once it is sent.
	/// @throws std::runtime_error If the program won't start.
	JsonRpcService(const Launch& launch, Framing framing, ServiceContext context, std::set<std::string> notifications = {})
	    : context_(std::move(context)), child_(ChildProcess::start(launch)), reader_(framing),
	      notifications_(std::move(notifications)) {
		link_->framing = framing;
		writer_ = std::jthread([this] { writeLoop(); });
		readerThread_ = std::jthread([this] { readLoop(); });
	}

	/// @brief Closes the link, ends the program and joins the threads. The calls waiting hear nothing more.
	~JsonRpcService() override {
		std::map<std::int64_t, Waiting> waiting;
		{
			const std::scoped_lock lock(link_->mutex);
			link_->closed = true;
			waiting.swap(link_->waiting);
			link_->streams.clear();
		}
		link_->wake.notify_all();
		(void)child_->end();
		writer_.join();
		readerThread_.join();
	}

	JsonRpcService(const JsonRpcService&) = delete;
	JsonRpcService& operator=(const JsonRpcService&) = delete;
	JsonRpcService(JsonRpcService&&) = delete;
	JsonRpcService& operator=(JsonRpcService&&) = delete;

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		if(notifications_.contains(method.name)) {
			json message = {{"jsonrpc", "2.0"}, {"method", method.name}};
			if(!call->arguments().is_null()) message["params"] = call->arguments();
			if(!link_->send(message)) return "the service is busy (its input is full)";
			call->finish(nullptr);
			return std::nullopt;
		}
		std::int64_t id = 0;
		{
			const std::scoped_lock lock(link_->mutex);
			if(link_->closed) return "the service ended";
			if(link_->waiting.size() >= mostWaiting) return "the service is busy (256 calls waiting)";
			id = link_->nextId++;
			link_->waiting[id].call = call;
		}
		json message = {{"jsonrpc", "2.0"}, {"id", id}, {"method", method.name}};
		if(!call->arguments().is_null()) message["params"] = call->arguments();
		if(!link_->send(message)) {
			const std::scoped_lock lock(link_->mutex);
			link_->waiting.erase(id);
			return "the service is busy (its input is full)";
		}
		watch(id, call->stopToken());
		return std::nullopt;
	}

	std::optional<std::string> subscribe(const std::string& stream, std::shared_ptr<Call> call) override {
		const std::scoped_lock lock(link_->mutex);
		if(link_->closed) return "the service ended";
		std::erase_if(link_->streams, [](const auto& entry) { return entry.second->stopToken().stop_requested(); });
		if(link_->streams.size() >= mostSubscriptions) return "the service has 256 subscriptions already";
		link_->streams.emplace(stream, std::move(call));
		return std::nullopt;
	}

private:
	/// @brief Watches a call's stop token: when it stops, the call is forgotten and the program told.
	/// @param id The call's id.
	/// @param stop Its stop token.
	void watch(std::int64_t id, const std::stop_token& stop) {
		const std::weak_ptr<Link> weak = link_;
		auto callback = std::make_unique<std::stop_callback<std::function<void()>>>(stop, [weak, id] {
			const std::shared_ptr<Link> link = weak.lock();
			if(!link) return;
			std::optional<Waiting> forgotten;
			{
				const std::scoped_lock lock(link->mutex);
				const auto found = link->waiting.find(id);
				if(found == link->waiting.end()) return;
				forgotten = std::move(found->second);
				link->waiting.erase(found);
			}
			(void)link->send({{"jsonrpc", "2.0"}, {"method", "$/cancelRequest"}, {"params", {{"id", id}}}});
		});
		const std::scoped_lock lock(link_->mutex);
		if(const auto found = link_->waiting.find(id); found != link_->waiting.end())
			found->second.watch = std::move(callback);
		// else it was answered or stopped already, and the callback ends here
	}

	/// @brief Writes the outbox to the program until the link closes.
	void writeLoop() {
		for(;;) {
			std::string bytes;
			{
				std::unique_lock lock(link_->mutex);
				link_->wake.wait(lock, [this] { return link_->closed || !link_->outbox.empty(); });
				if(link_->closed) return;
				bytes = std::move(link_->outbox.front());
				link_->outbox.pop_front();
				link_->outboxBytes -= bytes.size();
			}
			if(!child_->write(bytes)) return close("the service stopped reading its input");
		}
	}

	/// @brief Reads the program's messages until it ends or breaks the protocol.
	void readLoop() {
		char buffer[65536];
		std::vector<std::string> bodies;
		for(std::ptrdiff_t count; (count = child_->read(buffer, sizeof buffer)) > 0;) {
			bodies.clear();
			if(auto problem = reader_.feed(std::string_view(buffer, static_cast<std::size_t>(count)), bodies))
				return close("the service broke the protocol: " + *problem);
			for(const std::string& body : bodies)
				if(auto problem = handle(body)) return close("the service broke the protocol: " + *problem);
		}
		close("the service ended");
	}

	/// @brief Handles one message from the program.
	/// @param body The message.
	/// @return Why it is malformed, or std::nullopt.
	std::optional<std::string> handle(const std::string& body) {
		json message;
		try {
			message = parseUntrusted(body);
		} catch(const std::exception&) {
			return "a message isn't JSON";
		}
		if(!message.is_object()) return "a message isn't an object";
		const auto method = message.find("method");
		const auto id = message.find("id");
		if(method != message.end()) {
			if(!method->is_string()) return "a method isn't a string";
			const json params = message.contains("params") ? message["params"] : json();
			if(id != message.end()) request(*id, method->get<std::string>(), params);
			else notify(method->get<std::string>(), params);
			return std::nullopt;
		}
		if(id == message.end()) return "a message has neither a method nor an id";
		answer(*id, message);
		return std::nullopt;
	}

	/// @brief Ends a call with its answer.
	/// @param id The answer's id.
	/// @param message The answer.
	void answer(const json& id, const json& message) {
		if(!id.is_number_integer()) return; // not one of ours
		std::optional<Waiting> answered;
		{
			const std::scoped_lock lock(link_->mutex);
			const auto found = link_->waiting.find(id.get<std::int64_t>());
			if(found == link_->waiting.end()) return; // cancelled, or never sent
			answered = std::move(found->second);
			link_->waiting.erase(found);
		}
		Call& call = *answered->call;
		if(const auto error = message.find("error"); error != message.end() && error->is_object()) {
			const json code = error->value("code", json(internalError));
			call.fail(code.is_number_integer() ? std::to_string(code.get<std::int64_t>()) : "failed",
			          messageOf(error->value("message", json())), error->value("data", json()));
		} else {
			call.finish(message.value("result", json()));
		}
	}

	/// @brief Delivers a notification: progress or an event to the call it names, and an event to its subscribers.
	/// @param method Its method.
	/// @param params Its parameters.
	void notify(const std::string& method, const json& params) {
		const bool progress = method == "$/progress";
		const json& named = !params.is_object() ? json() : progress ? params.value("token", json())
		                                                            : params.value("request", json());
		std::vector<std::shared_ptr<Call>> to;
		std::shared_ptr<Call> call;
		{
			const std::scoped_lock lock(link_->mutex);
			if(named.is_number_integer())
				if(const auto found = link_->waiting.find(named.get<std::int64_t>()); found != link_->waiting.end())
					call = found->second.call;
			const auto [first, last] = link_->streams.equal_range(method);
			for(auto entry = first; entry != last; ++entry) to.push_back(entry->second);
		}
		if(call && progress) call->progress(params.value("value", json()));
		else if(call) call->event(params);
		for(const std::shared_ptr<Call>& subscriber : to) subscriber->event(params);
	}

	/// @brief Hands the program's request to the screen; its answer is sent back, if the link still exists.
	/// @param id The request's id.
	/// @param method Its method.
	/// @param params Its parameters.
	void request(const json& id, std::string method, json params) {
		const std::weak_ptr<Link> weak = link_;
		context_.request(std::move(method), std::move(params),
		                 [weak, id](const std::optional<json>& value, const std::string& error) {
			                 if(const std::shared_ptr<Link> link = weak.lock())
				                 (void)link->send(response(id, value, error));
		                 });
	}

	/// @brief Closes the link once: the calls waiting and the subscriptions fail, and the service reports Failed.
	/// @param reason Why.
	void close(const std::string& reason) {
		std::map<std::int64_t, Waiting> waiting;
		std::multimap<std::string, std::shared_ptr<Call>, std::less<>> streams;
		{
			const std::scoped_lock lock(link_->mutex);
			if(link_->closed) return;
			link_->closed = true;
			waiting.swap(link_->waiting);
			streams.swap(link_->streams);
		}
		link_->wake.notify_all();
		(void)child_->end();
		for(auto& [id, entry] : waiting) entry.call->fail("failed", reason);
		for(auto& [method, call] : streams) call->fail("failed", reason);
		context_.report(ServiceState::Failed, reason);
	}

	ServiceContext context_;                          ///< See the constructor.
	std::shared_ptr<Link> link_ = std::make_shared<Link>(); ///< Shared with the stop callbacks and responders.
	std::unique_ptr<ChildProcess> child_;             ///< The program.
	FrameReader reader_;                              ///< The reader thread's framing.
	std::set<std::string> notifications_;             ///< See the constructor.
	std::jthread writer_;                             ///< Writes the outbox.
	std::jthread readerThread_;                       ///< Reads the program's messages.
};

/// @brief Reads `methods` into specs: effect "read", "write" or "run", or "event" for a notification stream.
/// @param methods The `methods` object of a configuration.
/// @return The specs.
std::vector<MethodSpec> declaredMethods(const json& methods) {
	std::vector<MethodSpec> specs;
	for(const auto& [name, declared] : methods.items()) {
		const std::string effect = declared.value("effect", std::string("read"));
		MethodSpec spec{name, effect == "write" ? Effect::Write : effect == "run" ? Effect::Run : Effect::Read,
		                declared.value("revisable", true), effect == "event", declared.value("arguments", json()),
		                declared.value("about", std::string())};
		specs.push_back(std::move(spec));
	}
	return specs;
}

/// @brief The `jsonrpc` provider.
class JsonRpcProvider final : public Provider {
public:
	std::string_view name() const override { return "jsonrpc"; }
	std::string_view about() const override {
		return "a program the host allows, spoken to in JSON-RPC 2.0 (LSP-style) over its stdin and stdout";
	}
	json configurationSchema() const override {
		const json method = {{"type", "object"},
		                     {"additionalProperties", false},
		                     {"properties",
		                      {{"effect", {{"enum", {"read", "write", "run", "event"}}}},
		                       {"revisable", {{"type", "boolean"}}},
		                       {"about", {{"type", "string"}, {"maxLength", 200}}},
		                       {"arguments", {{"type", "object"}}}}}};
		return {{"type", "object"},
		        {"required", {"process"}},
		        {"additionalProperties", false},
		        {"properties",
		         {{"process", {{"type", "string"}}},
		          {"root", {{"type", "string"}}},
		          {"framing", {{"enum", {"contentLength", "lines"}}}},
		          {"methods", {{"type", "object"}, {"additionalProperties", method}}},
		          {"notifications", {{"type", "array"}, {"maxItems", 256}, {"items", {{"type", "string"}}}}}}}};
	}
	std::vector<MethodSpec> methods(const json& configuration) const override {
		return declaredMethods(configuration.value("methods", json::object()));
	}
	bool takesUnlistedMethods() const noexcept override { return true; }
	std::unique_ptr<Service> create(const json& configuration, ServiceContext context) override {
		const ProcessGrant& grant = *context.grant.process;
		const Launch launch{grant.program, grant.arguments, context.grant.root, grant.environment};
		const Framing framing =
		    configuration.value("framing", std::string("contentLength")) == "lines" ? Framing::Lines : Framing::ContentLength;
		std::set<std::string> notifications;
		for(const json& name : configuration.value("notifications", json::array()))
			if(name.is_string()) notifications.insert(name.get<std::string>());
		return std::make_unique<JsonRpcService>(launch, framing, std::move(context), std::move(notifications));
	}
};

} // namespace

std::unique_ptr<Provider> makeJsonRpcProvider() { return std::make_unique<JsonRpcProvider>(); }

} // namespace affresco::services
