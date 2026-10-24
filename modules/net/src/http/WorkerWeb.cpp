/// @file
/// @brief A Client's HTTP requests in a browser (cmake/AffrescoUIWeb.cmake): the browser's fetch in place of the curl
/// multi loop, on the main thread. Client.cpp checks every request as it does on the desktop; this file sends it, reads
/// the response through a stream reader with the same limits, and retries as Worker.cpp does.
///
/// What a browser decides for itself: the connections per origin (the client's maxPerOrigin isn't used), TLS, the
/// proxy and the CA file. Cookies and HTTP credentials are never sent (credentials "omit"), and a redirect fails the
/// request (redirect "error"): the browser would follow one to another origin before the worker could check it.

#include "http/Http.hpp"
#include "http/Worker.hpp"

#include <emscripten.h>
#include <emscripten/eventloop.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>

// clang-format off

/// @brief Starts a fetch; what happens goes to the affresco_fetch_* functions below, under its handle.
/// @param handle The fetch's number (never 0).
/// @param method The method.
/// @param url The URL.
/// @param headers The request's headers, as a JSON array of [name, value] pairs.
/// @param body The body's bytes.
/// @param bodyLength Their length (0: no body).
/// @param timeout The milliseconds before the request (or for a stream, its response) times out.
/// @param stream Non-zero for a stream: its timeout ends once the response starts.
EM_JS(void, affresco_fetch_start, (unsigned handle, const char* method, const char* url, const char* headers,
                                   const char* body, size_t bodyLength, double timeout, int stream), {
	const fetches = globalThis.affrescoFetches ??= new Map();
	const controller = new AbortController();
	const state = { controller, timedOut: false, timer: 0 };
	fetches.set(handle, state);
	const live = () => fetches.get(handle) === state;
	const end = code => {
		if (!live()) return;
		clearTimeout(state.timer);
		fetches.delete(handle);
		_affresco_fetch_end(handle, code);
	};
	const stop = () => { controller.abort(); end(42); };
	const init = { method: UTF8ToString(method), headers: JSON.parse(UTF8ToString(headers)), signal: controller.signal,
	               credentials: "omit", redirect: "error", cache: "no-store" };
	if (bodyLength) init.body = HEAPU8.slice(body, body + bodyLength);
	const address = UTF8ToString(url);
	state.timer = setTimeout(() => { state.timedOut = true; controller.abort(); }, timeout);
	(async () => {
		const response = await fetch(address, init);
		if (!live()) return;
		_affresco_fetch_status(handle, response.status);
		for (const [name, value] of response.headers) {
			const namePointer = stringToNewUTF8(name), valuePointer = stringToNewUTF8(value);
			const kept = _affresco_fetch_header(handle, namePointer, valuePointer);
			_free(namePointer);
			_free(valuePointer);
			if (!kept) return stop();
		}
		_affresco_fetch_headers_done(handle);
		if (stream) clearTimeout(state.timer);
		if (response.body) {
			const reader = response.body.getReader();
			for (;;) {
				const { done, value } = await reader.read();
				if (done || !live()) break;
				if (!value.length) continue;
				const pointer = _malloc(value.length);
				HEAPU8.set(value, pointer);
				const kept = _affresco_fetch_body(handle, pointer, value.length);
				_free(pointer);
				if (!kept) return stop();
			}
		}
		end(0);
	})().catch(() => end(state.timedOut ? 28 : 7));
});

/// @brief Stops a fetch: nothing more is told about it.
/// @param handle Its number.
EM_JS(void, affresco_fetch_abort, (unsigned handle), {
	const fetches = globalThis.affrescoFetches;
	const state = fetches && fetches.get(handle);
	if (!state) return;
	fetches.delete(handle);
	clearTimeout(state.timer);
	state.controller.abort();
});

EM_JS_DEPS(affresco_fetch, "$UTF8ToString,$stringToNewUTF8,malloc,free");

// clang-format on

namespace affresco::net {

namespace curl {

bool start() noexcept { return true; } // the browser's fetch needs no start

} // namespace curl

namespace {

/// The most retries a request makes, whatever it asks.
constexpr int maxRetries = 5;
/// The longest wait before a retry.
constexpr std::chrono::milliseconds longestRetryWait{std::chrono::hours{1}};

/// @brief A fetch in flight, as the browser's callbacks find it.
struct Fetch {
	Job* job = nullptr;                ///< Its job, in the worker's running jobs.
	std::function<void(CURLcode)> end; ///< Tells the worker it ended.
};

/// @brief The fetches in flight, by handle (every worker's).
/// @return The map.
std::unordered_map<std::uint32_t, Fetch>& fetches() {
	static std::unordered_map<std::uint32_t, Fetch> map;
	return map;
}

/// @brief A worker's retry timer.
struct Timer {
	int id = 0;                ///< The browser's timer.
	std::function<void()> run; ///< Starts the waiting jobs.
};

/// @brief The retry timers, by worker.
/// @return The map.
std::unordered_map<const void*, Timer>& timers() {
	static std::unordered_map<const void*, Timer> map;
	return map;
}

/// @brief A retry timer went off.
/// @param worker The worker it is for.
void onTimer(void* worker) {
	auto it = timers().find(worker);
	if(it == timers().end()) return;
	auto run = std::move(it->second.run);
	timers().erase(it);
	run();
}

/// @brief Stops a worker's retry timer, if it has one.
/// @param worker The worker.
void clearTimer(const void* worker) {
	auto it = timers().find(worker);
	if(it == timers().end()) return;
	emscripten_clear_timeout(it->second.id);
	timers().erase(it);
}

/// @brief The handle of a fetch, as the running jobs know it (their "easy handle").
/// @param handle The fetch's number.
/// @return It, as a key.
CURL* easyOf(std::uint32_t handle) noexcept {
	return reinterpret_cast<CURL*>(static_cast<std::uintptr_t>(handle)); // NOLINT(performance-no-int-to-ptr): a key
}

/// @brief The fetch's number of a running job's key.
/// @param easy The key.
/// @return The number.
std::uint32_t handleOf(CURL* easy) noexcept {
	return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(easy));
}

/// @brief The job of a fetch still in flight.
/// @param handle The fetch's number.
/// @return It, or nullptr.
Job* jobOf(std::uint32_t handle) {
	auto it = fetches().find(handle);
	return it == fetches().end() ? nullptr : it->second.job;
}

/// @brief Whether a status may be worth a retry.
/// @param status The status.
bool retryableStatus(int status) noexcept { return status == 429 || status == 502 || status == 503 || status == 504; }

} // namespace

} // namespace affresco::net

using affresco::net::Job;

/// @brief The browser read a response's status.
/// @param handle The fetch.
/// @param status The status.
extern "C" EMSCRIPTEN_KEEPALIVE void affresco_fetch_status(std::uint32_t handle, int status) {
	if(Job* job = affresco::net::jobOf(handle)) job->status = status;
}

/// @brief The browser read one of a response's headers (names in lower case).
/// @param handle The fetch.
/// @param name Its name.
/// @param value Its value.
/// @return 0 if the headers are over their limits: the fetch stops.
extern "C" EMSCRIPTEN_KEEPALIVE int affresco_fetch_header(std::uint32_t handle, const char* name, const char* value) {
	Job* job = affresco::net::jobOf(handle);
	if(!job) return 0;
	const std::string_view headerName(name), headerValue(value);
	job->headerBytes += headerName.size() + headerValue.size() + 4; // "name: value\r\n", as curl counts it
	if(job->headerBytes > affresco::net::maxResponseHeaderBytes ||
	   job->responseHeaders.size() >= affresco::net::maxResponseHeaders) {
		job->tooLarge = true;
		return 0;
	}
	job->responseHeaders.emplace_back(headerName, headerValue);
	return 1;
}

/// @brief The browser read all of a response's headers: a stream's events may start.
/// @param handle The fetch.
extern "C" EMSCRIPTEN_KEEPALIVE void affresco_fetch_headers_done(std::uint32_t handle) {
	Job* job = affresco::net::jobOf(handle);
	if(!job) return;
	job->headersDone = true;
	if(!job->streaming() || job->request.stream == affresco::network::Stream::Bytes) return;
	affresco::net::EventSink sink = [job](affresco::network::StreamEvent event) {
		affresco::net::postEvent(*job->executor, job->delivery, std::move(event));
	};
	if(job->request.stream == affresco::network::Stream::Events)
		job->events = std::make_unique<affresco::net::EventStreamParser>(job->request.maxBodyBytes, std::move(sink));
	else job->lines = std::make_unique<affresco::net::LineStreamParser>(job->request.maxBodyBytes, std::move(sink));
}

/// @brief The browser read bytes of a response's body: a stream's parser takes them, or the body keeps them, bounded.
/// @param handle The fetch.
/// @param bytes The bytes.
/// @param length How many.
/// @return 0 if a limit was passed (or nobody listens): the fetch stops.
extern "C" EMSCRIPTEN_KEEPALIVE int affresco_fetch_body(std::uint32_t handle, const char* bytes, std::size_t length) {
	Job* job = affresco::net::jobOf(handle);
	if(!job || job->delivery->cancelled) return 0;
	if(job->request.stream == affresco::network::Stream::Bytes && job->headersDone && job->status >= 200 &&
	   job->status < 300) { // a byte stream's chunks go out as they are, in parts of at most maxBodyBytes
		const std::size_t part = std::max<std::size_t>(job->request.maxBodyBytes, 1);
		for(std::size_t at = 0; at < length; at += part) {
			if(job->delivery->undelivered > job->maxUndelivered) {
				job->tooLarge = true;
				return 0;
			}
			affresco::net::postEvent(*job->executor, job->delivery,
			                         {"bytes", std::string(bytes + at, std::min(part, length - at)), {}});
		}
		return 1;
	}
	if(job->streaming() && (job->events || job->lines)) {
		const std::string_view chunk{bytes, length};
		const bool fed = job->events ? job->events->feed(chunk) : job->lines->feed(chunk);
		if(!fed || job->delivery->undelivered > job->maxUndelivered) {
			job->tooLarge = true;
			return 0;
		}
		return 1;
	}
	if(length > job->request.maxBodyBytes - std::min(job->body.size(), job->request.maxBodyBytes)) {
		job->tooLarge = true;
		return 0;
	}
	job->body.append(bytes, length);
	return 1;
}

/// @brief A fetch ended.
/// @param handle The fetch.
/// @param code How (CURLE_OK: a whole response).
extern "C" EMSCRIPTEN_KEEPALIVE void affresco_fetch_end(std::uint32_t handle, int code) {
	auto& fetches = affresco::net::fetches();
	auto it = fetches.find(handle);
	if(it == fetches.end()) return;
	auto end = it->second.end; // a copy: the worker forgets the fetch
	end(static_cast<CURLcode>(code));
}

namespace affresco::net {

void Job::resetResponse() {
	status = 0;
	responseHeaders.clear();
	headerBytes = 0;
	headersDone = false;
	body.clear();
	tooLarge = false;
	events.reset();
	lines.reset();
}

Worker::Worker(Executor& executor, const ClientOptions& options) : executor_(executor), options_(options) {}

Worker::~Worker() {
	clearTimer(this);
	for(const auto& [easy, job] : running_) {
		fetches().erase(handleOf(easy));
		affresco_fetch_abort(handleOf(easy));
	}
}

std::uint64_t Worker::submit(network::Request request, Url url, std::shared_ptr<RequestDelivery> delivery) {
	auto job = std::make_unique<Job>();
	job->id = ++nextId_;
	job->request = std::move(request);
	job->url = std::move(url);
	job->delivery = std::move(delivery);
	job->executor = &executor_;
	job->maxUndelivered = options_.maxUndelivered;
	job->notBefore = Job::Clock::now();
	const std::uint64_t id = job->id;
	++jobs_;
	waiting_.push_back(std::move(job));
	startWaiting(); // the outcome is posted: no handler runs inside this call
	return id;
}

void Worker::cancel(std::uint64_t id) { drop(id); }

void Worker::drop(std::uint64_t id) {
	for(auto it = running_.begin(); it != running_.end(); ++it) {
		if(it->second->id != id) continue;
		static_cast<void>(takeRunning(it->first));
		--jobs_;
		return;
	}
	auto waiting = std::ranges::find_if(waiting_, [id](const auto& job) { return job->id == id; });
	if(waiting == waiting_.end()) return; // it has ended already
	waiting_.erase(waiting);
	--jobs_;
}

void Worker::startWaiting() {
	const auto now = Job::Clock::now();
	for(auto it = waiting_.begin(); it != waiting_.end();) {
		if((*it)->notBefore > now) {
			++it;
			continue;
		}
		std::unique_ptr<Job> job = std::move(*it);
		it = waiting_.erase(it);
		if(auto error = begin(*job)) {
			fail(std::move(job), {"network", *error, 0, {}});
			continue;
		}
		CURL* easy = job->easy.get();
		running_.emplace(easy, std::move(job));
	}
	clearTimer(this);
	if(waiting_.empty()) return;
	auto& timer = timers()[this];
	timer.run = [this] { startWaiting(); };
	timer.id = emscripten_set_timeout(onTimer, static_cast<double>(nextWait().count()), this);
}

std::optional<std::string> Worker::begin(Job& job) {
	static std::uint32_t lastHandle = 0;
	job.resetResponse();
	if(++lastHandle == 0) ++lastHandle; // 0 is no handle
	const std::uint32_t handle = lastHandle;
	job.easy.reset(easyOf(handle)); // the key of the running job; its deleter does nothing here
	nlohmann::json headers = nlohmann::json::array();
	for(const auto& [name, value] : job.request.headers) headers.push_back({name, value});
	if(job.request.stream == network::Stream::Events && !findHeader(job.request.headers, "accept"))
		headers.push_back({"Accept", "text/event-stream"});
	const std::string headerText = headers.dump();
	fetches()[handle] = {&job, [this, handle](CURLcode code) { finished(easyOf(handle), code); }};
	const std::string& body = job.request.body;
	affresco_fetch_start(handle, job.request.method.c_str(), job.request.url.c_str(), headerText.c_str(), body.data(),
	                     body.size(), static_cast<double>(job.request.timeout.count()),
	                     job.request.stream == network::Stream::None ? 0 : 1);
	return std::nullopt;
}

std::unique_ptr<Job> Worker::takeRunning(CURL* easy) {
	auto it = running_.find(easy);
	if(it == running_.end()) return nullptr;
	std::unique_ptr<Job> job = std::move(it->second);
	running_.erase(it);
	fetches().erase(handleOf(easy));
	affresco_fetch_abort(handleOf(easy)); // nothing if it has ended
	return job;
}

void Worker::finished(CURL* easy, CURLcode code) {
	std::unique_ptr<Job> job = takeRunning(easy);
	if(!job) return;
	if(job->delivery->cancelled) {
		--jobs_;
	} else if(job->tooLarge || code == CURLE_ABORTED_BY_CALLBACK) {
		fail(std::move(job), {"tooLarge", "The response is larger than allowed.", 0, {}});
	} else if(code == CURLE_OPERATION_TIMEDOUT) {
		const bool stream = job->request.stream != network::Stream::None;
		fail(std::move(job),
		     {"timeout", stream ? "The stream didn't start in time." : "The request took too long.", 0, {}});
	} else if(code != CURLE_OK) {
		if(!retry(job))
			fail(std::move(job),
			     {"network", "The request failed: the network, the server's CORS headers or a redirect.", 0, {}});
	} else {
		answered(std::move(job));
	}
	startWaiting(); // a retry may wait
}

void Worker::answered(std::unique_ptr<Job> job) {
	const int status = job->status;
	if(retryableStatus(status) && retry(job)) return;
	if(status < 200 || status >= 300) {
		network::Failure failure{"status", "The server answered " + std::to_string(status) + ".", status,
		                         std::move(job->body)};
		fail(std::move(job), std::move(failure));
		return;
	}
	if(job->lines) job->lines->finish();
	network::Response response{status, std::move(job->responseHeaders), std::move(job->body)};
	postResponse(executor_, job->delivery, std::move(response));
	--jobs_;
}

bool Worker::retry(std::unique_ptr<Job>& job) {
	const bool delivered = job->streaming() && job->headersDone; // events may have gone out: never repeat them
	if(job->request.method != "GET" || delivered || job->attempts >= std::min(job->request.retries, maxRetries))
		return false;
	auto wait = job->request.retryAfter * (std::int64_t{1} << job->attempts);
	if(auto header = findHeader(job->responseHeaders, "retry-after"))
		if(auto asked = parseRetryAfter(*header)) wait = *asked;
	++job->attempts;
	job->easy.reset();
	job->notBefore = Job::Clock::now() + std::min<std::chrono::milliseconds>(wait, longestRetryWait);
	waiting_.push_back(std::move(job));
	return true;
}

void Worker::fail(std::unique_ptr<Job> job, network::Failure failure) {
	postFailure(executor_, job->delivery, std::move(failure));
	--jobs_;
}

std::chrono::milliseconds Worker::nextWait() const {
	const auto now = Job::Clock::now();
	auto until = now + longestRetryWait;
	for(const auto& job : waiting_) until = std::min(until, job->notBefore);
	return std::max(std::chrono::milliseconds{0}, std::chrono::duration_cast<std::chrono::milliseconds>(until - now));
}

} // namespace affresco::net
