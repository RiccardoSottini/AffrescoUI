/// @file
/// @brief The `dap` provider: a debug adapter the host granted (or one already running on a loopback port), spoken to
/// in the Debug Adapter Protocol over Content-Length frames. Each request waits for the response that names its
/// `seq`, within a timeout, and a stopped call sends `cancel`; events reach the streams subscribed to them, and the
/// adapter's own requests (`runInTerminal`, `startDebugging`) go to the screen.

#include "DapProtocol.hpp"
#include "DapTransport.hpp"
#include "Framing.hpp"
#include "Providers.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>

namespace affresco::services {

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t mostWaiting = 256;                     ///< Requests waiting for a response, per service.
constexpr std::size_t mostSubscriptions = 256;               ///< Subscriptions, per service.
constexpr std::size_t largestOutbox = std::size_t{16} << 20; ///< Bytes waiting to be written, at most 16 MiB.
constexpr FrameLimits dapFrames{std::size_t{8} << 10, std::size_t{4} << 20}; ///< Headers 8 KiB, a message 4 MiB.
constexpr auto disconnectGrace = std::chrono::seconds(3); ///< How long an adapter may take to exit after `disconnect`.

/// @brief The reverse requests passed to the screen; any other is answered "not supported" at once.
constexpr std::array<std::string_view, 2> reverseRequests = {"runInTerminal", "startDebugging"};

/// @brief A request waiting for its response.
struct Waiting {
	std::shared_ptr<Call> call;                                       ///< The call.
	std::string command;                                              ///< Its command, to bound the response.
	Clock::time_point deadline;                                       ///< When it times out.
	std::unique_ptr<std::stop_callback<std::function<void()>>> watch; ///< Sends `cancel` when it stops.
};

/// @brief What the service's threads and the responders it hands out share; responders keep it by weak pointer.
struct Link {
	std::mutex mutex;                        ///< Guards everything below.
	std::condition_variable wake;            ///< The writer and the sweeper wait on it.
	std::deque<std::string> outbox;          ///< Framed messages to write.
	std::size_t outboxBytes = 0;             ///< Their size.
	bool closed = false;                     ///< The service is ending, or the adapter went away.
	std::optional<Clock::time_point> ending; ///< When `disconnect` was sent: the adapter's end is then expected.
	bool supportsCancel = false;             ///< The adapter said `supportsCancelRequest`.
	std::int64_t nextSeq = 1;                ///< The next message's `seq`.
	std::map<std::int64_t, Waiting> waiting; ///< Requests by `seq`.
	std::multimap<std::string, std::shared_ptr<Call>, std::less<>> streams; ///< Subscriptions by event ("events": all).

	/// @brief Queues a message with the next `seq`; the caller holds the mutex.
	/// @param message The message, without its `seq`.
	/// @return Its `seq`, or 0 if the link is closed or the outbox is full.
	std::int64_t sendLocked(json message) {
		if(closed) return 0;
		const std::int64_t seq = nextSeq;
		message["seq"] = seq;
		std::string bytes = frameMessage(Framing::ContentLength, message.dump());
		if(outboxBytes + bytes.size() > largestOutbox) return 0;
		++nextSeq;
		outboxBytes += bytes.size();
		outbox.push_back(std::move(bytes));
		wake.notify_all();
		return seq;
	}
	/// @brief Queues a message with the next `seq`.
	/// @param message The message.
	/// @return As sendLocked().
	std::int64_t send(json message) {
		const std::lock_guard lock(mutex);
		return sendLocked(std::move(message));
	}
};

/// @brief One `dap` service: its adapter, a writer, a reader and a sweeper thread, and the requests waiting.
class DapService final : public Service {
public:
	/// @brief Starts the threads on a transport.
	/// @param transport The adapter's stream.
	/// @param timeout How long a request may wait for its response.
	/// @param context What the service works with.
	DapService(std::unique_ptr<DapTransport> transport, std::chrono::milliseconds timeout, ServiceContext context)
	    : context_(std::move(context)), transport_(std::move(transport)), timeout_(timeout) {
		writer_ = std::jthread([this] { writeLoop(); });
		readerThread_ = std::jthread([this] { readLoop(); });
		sweeper_ = std::jthread([this] { sweepLoop(); });
	}

	/// @brief Closes the link, ends the adapter and joins the threads. The calls waiting hear nothing more.
	~DapService() override {
		std::map<std::int64_t, Waiting> waiting;
		{
			const std::lock_guard lock(link_->mutex);
			link_->closed = true;
			waiting.swap(link_->waiting);
			link_->streams.clear();
		}
		link_->wake.notify_all();
		(void)transport_->end();
		writer_.join();
		readerThread_.join();
		sweeper_.join();
	}

	DapService(const DapService&) = delete;
	DapService& operator=(const DapService&) = delete;
	DapService(DapService&&) = delete;
	DapService& operator=(DapService&&) = delete;

	std::optional<std::string> invoke(const MethodSpec& method, std::shared_ptr<Call> call) override {
		json arguments = call->arguments();
		if(auto problem = dap::prepareArguments(method.name, arguments, context_.grant.root)) return problem;
		std::int64_t seq = 0;
		{
			const std::lock_guard lock(link_->mutex);
			if(link_->closed) return "the debug session ended";
			if(link_->waiting.size() >= mostWaiting) return "the debug adapter is busy (256 requests waiting)";
			seq = link_->sendLocked(dap::requestMessage(0, method.name, arguments));
			if(seq == 0) return "the debug adapter is busy (its input is full)";
			link_->waiting[seq] = {call, method.name, Clock::now() + timeout_, nullptr};
			if(method.name == "disconnect") link_->ending = Clock::now();
		}
		watch(seq, call->stopToken());
		return std::nullopt;
	}

	std::optional<std::string> subscribe(const std::string& stream, std::shared_ptr<Call> call) override {
		const std::lock_guard lock(link_->mutex);
		if(link_->closed) return "the debug session ended";
		std::erase_if(link_->streams, [](const auto& entry) { return entry.second->stopToken().stop_requested(); });
		if(link_->streams.size() >= mostSubscriptions) return "the service has 256 subscriptions already";
		link_->streams.emplace(stream, std::move(call));
		return std::nullopt;
	}

private:
	/// @brief Watches a call's stop token: when it stops, the request is forgotten and, if the adapter can, cancelled.
	/// @param seq The request's `seq`.
	/// @param stop Its stop token.
	void watch(std::int64_t seq, const std::stop_token& stop) {
		const std::weak_ptr<Link> weak = link_;
		auto callback = std::make_unique<std::stop_callback<std::function<void()>>>(stop, [weak, seq] {
			const std::shared_ptr<Link> link = weak.lock();
			if(!link) return;
			std::optional<Waiting> forgotten;
			const std::lock_guard lock(link->mutex);
			const auto found = link->waiting.find(seq);
			if(found == link->waiting.end()) return;
			forgotten = std::move(found->second); // its callback, this one, ends after the lock is released
			link->waiting.erase(found);
			if(link->supportsCancel) (void)link->sendLocked(dap::requestMessage(0, "cancel", {{"requestId", seq}}));
		});
		const std::lock_guard lock(link_->mutex);
		if(const auto found = link_->waiting.find(seq); found != link_->waiting.end())
			found->second.watch = std::move(callback);
		// else it was answered or stopped already, and the callback ends here
	}

	/// @brief Writes the outbox to the adapter until the link closes.
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
			if(!transport_->write(bytes)) return close("the debug adapter stopped reading its input");
		}
	}

	/// @brief Times requests out, and ends an adapter that outstays its `disconnect`, until the link closes.
	void sweepLoop() {
		for(;;) {
			std::vector<Waiting> late;
			bool overstayed = false;
			{
				std::unique_lock lock(link_->mutex);
				link_->wake.wait_for(lock, std::chrono::milliseconds(100), [this] { return link_->closed; });
				if(link_->closed) return;
				const Clock::time_point now = Clock::now();
				std::erase_if(link_->waiting, [&late, now](auto& entry) {
					if(entry.second.deadline > now) return false;
					late.push_back(std::move(entry.second));
					return true;
				});
				overstayed = link_->ending && now - *link_->ending > disconnectGrace;
			}
			for(Waiting& request : late)
				request.call->fail("timeout", "the debug adapter didn't answer '" + request.command + "' in time");
			if(overstayed) (void)transport_->end(); // the reader then sees the end, as expected
		}
	}

	/// @brief Reads the adapter's messages until it ends or breaks the protocol.
	void readLoop() {
		char buffer[65536];
		std::vector<std::string> bodies;
		FrameReader reader(Framing::ContentLength, dapFrames);
		for(std::ptrdiff_t count; (count = transport_->read(buffer, sizeof buffer)) > 0;) {
			bodies.clear();
			if(auto problem = reader.feed(std::string_view(buffer, static_cast<std::size_t>(count)), bodies))
				return close("the debug adapter broke the protocol: " + *problem);
			for(const std::string& body : bodies)
				if(auto problem = handle(body)) return close("the debug adapter broke the protocol: " + *problem);
		}
		bool expected = false;
		{
			const std::lock_guard lock(link_->mutex);
			expected = link_->ending.has_value();
		}
		if(expected) return close({});
		const int code = transport_->end();
		close(code > 0 ? "the debug adapter ended unexpectedly (exit code " + std::to_string(code) + ")"
		               : "the debug adapter ended unexpectedly");
	}

	/// @brief Handles one message from the adapter.
	/// @param body The message.
	/// @return Why it is malformed, or std::nullopt.
	std::optional<std::string> handle(const std::string& body) {
		dap::Message message = dap::readMessage(body);
		if(auto* problem = std::get_if<std::string>(&message)) return std::move(*problem);
		if(auto* response = std::get_if<dap::Response>(&message)) answer(std::move(*response));
		else if(auto* event = std::get_if<dap::Event>(&message)) deliver(std::move(*event));
		else request(std::get<dap::Request>(std::move(message)));
		return std::nullopt;
	}

	/// @brief Ends a request with its response.
	/// @param response The response.
	void answer(dap::Response response) {
		std::optional<Waiting> answered;
		{
			const std::lock_guard lock(link_->mutex);
			const auto found = link_->waiting.find(response.requestSeq);
			if(found == link_->waiting.end()) return; // cancelled, timed out, or never sent
			answered = std::move(found->second);
			link_->waiting.erase(found);
			if(answered->command == "initialize" && response.success && response.body.is_object())
				link_->supportsCancel = response.body.value("supportsCancelRequest", false) == true;
		}
		Call& call = *answered->call;
		if(!response.success)
			return call.fail(response.message == "cancelled" ? "cancelled" : "failed", std::move(response.message));
		call.finish(dap::boundResponse(answered->command, std::move(response.body), context_.grant.root));
	}

	/// @brief Delivers an event to its subscribers: those of its name get its body, those of "events" `{event, body}`.
	/// Output events pass within the budget; the first to pass after some were dropped says how many.
	/// @param event The event.
	void deliver(dap::Event event) {
		if(event.event == "output") {
			const json& text = event.body.is_object() ? event.body.value("output", json()) : json();
			const std::size_t bytes = text.is_string() ? text.get_ref<const std::string&>().size() : 0;
			const std::optional<std::size_t> dropped = budget_.admit(bytes, Clock::now());
			if(!dropped) return;
			if(*dropped > 0)
				publish("output", {{"category", "console"},
				                   {"output", "(" + std::to_string(*dropped) +
				                                  " output events dropped: the adapter wrote too fast)\n"}});
		}
		publish(event.event, dap::boundEvent(event.event, std::move(event.body), context_.grant.root));
	}

	/// @brief Gives an event to its subscribers.
	/// @param name The event's name.
	/// @param body Its body.
	void publish(const std::string& name, const json& body) {
		std::vector<std::shared_ptr<Call>> named;
		std::vector<std::shared_ptr<Call>> all;
		{
			const std::lock_guard lock(link_->mutex);
			const auto [first, last] = link_->streams.equal_range(name);
			for(auto entry = first; entry != last; ++entry) named.push_back(entry->second);
			const auto [from, to] = link_->streams.equal_range(std::string_view("events"));
			for(auto entry = from; entry != to; ++entry) all.push_back(entry->second);
		}
		for(const std::shared_ptr<Call>& subscriber : named) subscriber->event(body);
		for(const std::shared_ptr<Call>& subscriber : all) subscriber->event({{"event", name}, {"body", body}});
	}

	/// @brief Hands a reverse request the screen may answer to it (the others are refused at once); its answer is sent
	/// back, if the link still exists.
	/// @param request The request.
	void request(dap::Request request) {
		if(std::find(reverseRequests.begin(), reverseRequests.end(), request.command) == reverseRequests.end()) {
			(void)link_->send(dap::responseMessage(0, request, std::nullopt, "not supported"));
			return;
		}
		const std::weak_ptr<Link> weak = link_;
		std::string command = request.command;
		json arguments = std::move(request.arguments);
		request.arguments = nullptr;
		context_.request(std::move(command), std::move(arguments),
		                 [weak, request = std::move(request)](std::optional<json> value, std::string error) {
			                 if(const std::shared_ptr<Link> link = weak.lock())
				                 (void)link->send(dap::responseMessage(0, request, value, error));
		                 });
	}

	/// @brief Closes the link once: the requests waiting end ("ended"), and the subscriptions finish if the adapter
	/// ended as asked, or fail with the reason. The service reports Stopped or Failed, so a crash is said once.
	/// @param reason Why, or empty for an end that was asked for.
	void close(const std::string& reason) {
		std::map<std::int64_t, Waiting> waiting;
		std::multimap<std::string, std::shared_ptr<Call>, std::less<>> streams;
		{
			const std::lock_guard lock(link_->mutex);
			if(link_->closed) return;
			link_->closed = true;
			waiting.swap(link_->waiting);
			streams.swap(link_->streams);
		}
		link_->wake.notify_all();
		(void)transport_->end();
		for(auto& [seq, entry] : waiting)
			entry.call->fail("ended", reason.empty() ? "the debug session ended" : reason);
		for(auto& [name, call] : streams)
			if(reason.empty()) call->finish(nullptr);
			else call->fail("failed", reason);
		context_.report(reason.empty() ? ServiceState::Stopped : ServiceState::Failed, reason);
	}

	ServiceContext context_;                                ///< See the constructor.
	std::shared_ptr<Link> link_ = std::make_shared<Link>(); ///< Shared with the stop callbacks and responders.
	std::unique_ptr<DapTransport> transport_;               ///< The adapter.
	std::chrono::milliseconds timeout_;                     ///< See the constructor.
	dap::OutputBudget budget_;                              ///< The reader thread's output budget.
	std::jthread writer_;                                   ///< Writes the outbox.
	std::jthread readerThread_;                             ///< Reads the adapter's messages.
	std::jthread sweeper_;                                  ///< Times requests out.
};

/// @brief The requests a screen may send, by effect: those that only read the program's state may be added by a
/// revision; those that run or change the program are the screen file's only.
struct Commands {
	std::vector<std::string_view> reads = {"threads", "stackTrace",         "scopes",      "variables",
	                                       "source",  "loadedSources",      "completions", "exceptionInfo",
	                                       "modules", "breakpointLocations"};
	std::vector<std::string_view> runs = {"initialize",
	                                      "launch",
	                                      "attach",
	                                      "restart",
	                                      "configurationDone",
	                                      "continue",
	                                      "next",
	                                      "stepIn",
	                                      "stepOut",
	                                      "pause",
	                                      "evaluate",
	                                      "disconnect",
	                                      "terminate",
	                                      "setBreakpoints",
	                                      "setFunctionBreakpoints",
	                                      "setExceptionBreakpoints",
	                                      "restartFrame",
	                                      "goto",
	                                      "terminateThreads"};
	std::vector<std::string_view> writes = {"setVariable", "setExpression"};
	std::vector<std::string_view> events = {
	    "events",       "initialized", "stopped",       "continued",      "exited",       "terminated",
	    "thread",       "output",      "breakpoint",    "module",         "loadedSource", "process",
	    "capabilities", "invalidated", "progressStart", "progressUpdate", "progressEnd"};
};

/// @brief The `dap` provider.
class DapProvider final : public Provider {
public:
	std::string_view name() const override { return "dap"; }
	std::string_view about() const override {
		return "a debug adapter the host allows (or one on a loopback port), spoken to in the Debug Adapter Protocol";
	}
	json configurationSchema() const override {
		return {{"type", "object"},
		        {"additionalProperties", false},
		        {"properties",
		         {{"process", {{"type", "string"}}},
		          {"root", {{"type", "string"}}},
		          {"attach",
		           {{"type", "object"},
		            {"required", {"port"}},
		            {"additionalProperties", false},
		            {"properties", {{"port", {{"type", "integer"}, {"minimum", 1}, {"maximum", 65535}}}}}}},
		          {"timeout", {{"type", "integer"}, {"minimum", 100}, {"maximum", 600000}}}}}};
	}
	std::vector<MethodSpec> methods(const json&) const override {
		const Commands commands;
		const json object = {{"type", "object"}};
		std::vector<MethodSpec> specs;
		for(const std::string_view name : commands.reads)
			specs.push_back({std::string(name), Effect::Read, true, false, object, {}});
		for(const std::string_view name : commands.runs)
			specs.push_back({std::string(name), Effect::Run, false, false, object, {}});
		for(const std::string_view name : commands.writes)
			specs.push_back({std::string(name), Effect::Write, false, false, object, {}});
		for(const std::string_view name : commands.events)
			specs.push_back({std::string(name), Effect::Read, true, true, nullptr, {}});
		return specs;
	}
	std::unique_ptr<Service> create(const json& configuration, ServiceContext context) override {
		const bool attaches = configuration.contains("attach");
		if(attaches == context.grant.process.has_value())
			throw std::runtime_error("a dap service names either a granted adapter (process) or a port to attach to");
		const std::chrono::milliseconds timeout(configuration.value("timeout", 30000));
		std::unique_ptr<DapTransport> transport;
		if(attaches) {
			transport = DapTransport::connect(configuration["attach"]["port"].get<std::uint16_t>());
		} else {
			const ProcessGrant& grant = *context.grant.process;
			transport = DapTransport::start({grant.program, grant.arguments, context.grant.root, grant.environment});
		}
		return std::make_unique<DapService>(std::move(transport), timeout, std::move(context));
	}
};

} // namespace

std::unique_ptr<Provider> makeDapProvider() { return std::make_unique<DapProvider>(); }

} // namespace affresco::services
