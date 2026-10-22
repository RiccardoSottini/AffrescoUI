/// @file
/// @brief The worker thread of a Client: the curl multi loop, and what happens when a transfer ends (a response, a
/// redirect within the origin, a retry, a failure).

#include "http/Worker.hpp"

#include "http/Http.hpp"

#include <algorithm>
#include <utility>

namespace affresco::net {

namespace {

/// The most redirects a request follows.
constexpr int maxRedirects = 5;
/// The most retries a request makes, whatever it asks.
constexpr int maxRetries = 5;
/// The longest the loop sleeps without looking at its queue and deadlines.
constexpr std::chrono::milliseconds longestWait{250};
/// The longest wait before a retry.
constexpr std::chrono::milliseconds longestRetryWait{std::chrono::hours{1}};

/// @brief Takes the line ending off a header line.
/// @param line The line.
/// @return It, without "\r\n" or "\n".
std::string_view withoutLineEnding(std::string_view line) noexcept {
	while(!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.remove_suffix(1);
	return line;
}

/// @brief Creates the parser of a stream once its response is a success.
/// @param job The job.
void startStream(Job& job) {
	EventSink sink = [&job](network::StreamEvent event) { postEvent(*job.executor, job.delivery, std::move(event)); };
	if(job.request.stream == network::Stream::Events)
		job.events = std::make_unique<EventStreamParser>(job.request.maxBodyBytes, std::move(sink));
	else
		job.lines = std::make_unique<LineStreamParser>(job.request.maxBodyBytes, std::move(sink));
}

/// @brief curl's header callback: reads the status line and the headers of each response, bounded.
/// @param buffer One header line.
/// @param size 1.
/// @param count Its length.
/// @param user The Job.
/// @return count, or 0 to abort the transfer.
// NOLINTNEXTLINE(readability-non-const-parameter, misc-const-correctness): curl's CURLOPT_HEADERFUNCTION type
std::size_t onHeader(char* buffer, std::size_t size, std::size_t count, void* user) {
	Job& job = *static_cast<Job*>(user);
	const std::size_t length = size * count;
	const std::string_view line = withoutLineEnding({buffer, length});
	if(line.starts_with("HTTP/")) { // a new response (after a 100 Continue, say): forget the last one's headers
		job.status = parseStatusLine(line).value_or(0);
		job.responseHeaders.clear();
		job.headerBytes = 0;
		job.headersDone = false;
		return length;
	}
	if(line.empty()) {
		if(job.status >= 200) {
			job.headersDone = true;
			if(job.streaming()) startStream(job);
		}
		return length;
	}
	job.headerBytes += length;
	if(job.headerBytes > maxResponseHeaderBytes || job.responseHeaders.size() >= maxResponseHeaders) {
		job.tooLarge = true;
		return 0;
	}
	if(auto header = parseHeaderLine(line)) job.responseHeaders.push_back(std::move(*header));
	return length;
}

/// @brief curl's write callback: feeds a stream's parser, or keeps the body, bounded.
/// @param buffer Bytes of the body.
/// @param size 1.
/// @param count How many.
/// @param user The Job.
/// @return count, or 0 to abort the transfer.
// NOLINTNEXTLINE(readability-non-const-parameter, misc-const-correctness): curl's CURLOPT_WRITEFUNCTION type
std::size_t onBody(char* buffer, std::size_t size, std::size_t count, void* user) {
	Job& job = *static_cast<Job*>(user);
	const std::size_t length = size * count;
	if(job.delivery->cancelled) return 0; // nobody listens any more
	if(job.streaming() && (job.events || job.lines)) {
		const std::string_view chunk{buffer, length};
		const bool fed = job.events ? job.events->feed(chunk) : job.lines->feed(chunk);
		if(!fed || job.delivery->undelivered > job.maxUndelivered) {
			job.tooLarge = true;
			return 0;
		}
		return length;
	}
	if(length > job.request.maxBodyBytes - std::min(job.body.size(), job.request.maxBodyBytes)) {
		job.tooLarge = true;
		return 0;
	}
	job.body.append(buffer, length);
	return length;
}

/// @brief Whether a status may be worth a retry.
/// @param status The status.
bool retryableStatus(int status) noexcept {
	return status == 429 || status == 502 || status == 503 || status == 504;
}

} // namespace

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

Worker::Worker(Executor& executor, ClientOptions options)
    : executor_(executor), options_(std::move(options)), multi_(curl_multi_init()),
      thread_([this](const std::stop_token& stop) { run(stop); }) {}

Worker::~Worker() {
	thread_.request_stop();
	if(multi_) curl_multi_wakeup(multi_.get());
	if(thread_.joinable()) thread_.join();
	for(auto& [easy, job] : running_) curl_multi_remove_handle(multi_.get(), easy);
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
	{
		const std::scoped_lock lock(mutex_);
		commands_.push_back({id, std::move(job)});
	}
	curl_multi_wakeup(multi_.get());
	return id;
}

void Worker::cancel(std::uint64_t id) {
	{
		const std::scoped_lock lock(mutex_);
		commands_.push_back({id, nullptr});
	}
	curl_multi_wakeup(multi_.get());
}

void Worker::run(const std::stop_token& stop) {
	if(!multi_) return;
	while(!stop.stop_requested()) {
		takeCommands();
		startWaiting();
		checkDeadlines();
		int running = 0;
		curl_multi_perform(multi_.get(), &running);
		int left = 0;
		while(const CURLMsg* message = curl_multi_info_read(multi_.get(), &left))
			if(message->msg == CURLMSG_DONE) finished(message->easy_handle, message->data.result);
		int ready = 0;
		curl_multi_poll(multi_.get(), nullptr, 0, static_cast<int>(nextWait().count()), &ready);
	}
}

void Worker::takeCommands() {
	std::vector<Command> commands;
	{
		const std::scoped_lock lock(mutex_);
		commands.swap(commands_);
	}
	for(auto& command : commands) {
		if(command.job)
			waiting_.push_back(std::move(command.job));
		else
			drop(command.id);
	}
}

void Worker::drop(std::uint64_t id) {
	for(const auto& [easy, job] : running_) {
		if(job->id != id) continue;
		static_cast<void>(takeRunning(easy));
		--jobs_;
		return;
	}
	const auto waiting = std::ranges::find_if(waiting_, [id](const auto& job) { return job->id == id; });
	if(waiting == waiting_.end()) return; // it has ended already
	waiting_.erase(waiting);
	--jobs_;
}

void Worker::startWaiting() {
	const auto now = Job::Clock::now();
	for(auto it = waiting_.begin(); it != waiting_.end();) {
		const std::string origin = (*it)->url.origin();
		if((*it)->notBefore > now || perOrigin_[origin] >= options_.maxPerOrigin) {
			++it;
			continue;
		}
		std::unique_ptr<Job> job = std::move(*it);
		it = waiting_.erase(it);
		if(auto error = begin(*job)) {
			fail(std::move(job), {"network", *error, 0, {}});
			continue;
		}
		++perOrigin_[origin];
		CURL* easy = job->easy.get();
		running_.emplace(easy, std::move(job));
	}
}

std::optional<std::string> Worker::begin(Job& job) {
	job.resetResponse();
	job.headers.reset();
	job.easy.reset(curl_easy_init());
	CURL* easy = job.easy.get();
	if(!easy) return "Out of memory.";
	curl::applyCommonOptions(easy, options_);
	curl_easy_setopt(easy, CURLOPT_URL, job.request.url.c_str());
	bool listed = true;
	for(const auto& [name, value] : job.request.headers)
		listed = listed && curl::appendHeader(job.headers, std::string(name).append(": ").append(value));
	if(job.request.stream == network::Stream::Events && !findHeader(job.request.headers, "accept"))
		listed = listed && curl::appendHeader(job.headers, "Accept: text/event-stream");
	listed = listed && curl::appendHeader(job.headers, "Expect:"); // no 100-continue round trip
	if(!listed) return "Out of memory.";
	curl_easy_setopt(easy, CURLOPT_HTTPHEADER, job.headers.get());
	curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, onHeader);
	curl_easy_setopt(easy, CURLOPT_HEADERDATA, static_cast<void*>(&job));
	curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, onBody);
	curl_easy_setopt(easy, CURLOPT_WRITEDATA, static_cast<void*>(&job));
	applyMethod(job);
	const auto timeout = static_cast<long>(job.request.timeout.count());
	if(job.request.stream == network::Stream::None) {
		curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, timeout);
		curl_easy_setopt(easy, CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(job.request.maxBodyBytes));
	} else { // a stream may last: only the wait for its response is bounded
		curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, timeout);
		job.headersBy = Job::Clock::now() + job.request.timeout;
	}
	if(curl_multi_add_handle(multi_.get(), easy) != CURLM_OK) return "The request couldn't start.";
	return std::nullopt;
}

void Worker::applyMethod(Job& job) {
	CURL* easy = job.easy.get();
	const std::string& method = job.request.method;
	if(method == "GET") {
		curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
		return;
	}
	if(method != "POST") curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, method.c_str());
	if(method == "DELETE" && job.request.body.empty()) return;
	curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(job.request.body.size()));
	curl_easy_setopt(easy, CURLOPT_POSTFIELDS, job.request.body.c_str()); // the job keeps the body until it ends
}

std::unique_ptr<Job> Worker::takeRunning(CURL* easy) {
	const auto it = running_.find(easy);
	if(it == running_.end()) return nullptr;
	std::unique_ptr<Job> job = std::move(it->second);
	running_.erase(it);
	curl_multi_remove_handle(multi_.get(), easy);
	const auto origin = perOrigin_.find(job->url.origin());
	if(origin != perOrigin_.end() && --origin->second == 0) perOrigin_.erase(origin);
	return job;
}

void Worker::finished(CURL* easy, CURLcode code) {
	std::unique_ptr<Job> job = takeRunning(easy);
	if(!job) return;
	if(job->delivery->cancelled) { // its owner is gone; the cancel command will find nothing
		--jobs_;
		return;
	}
	if(job->tooLarge || code == CURLE_FILESIZE_EXCEEDED) {
		fail(std::move(job), {"tooLarge", "The response is larger than allowed.", 0, {}});
	} else if(code == CURLE_OPERATION_TIMEDOUT) {
		fail(std::move(job), {"timeout", "The request took too long.", 0, {}});
	} else if(code != CURLE_OK) {
		std::string message = curl_easy_strerror(code);
		if(!retry(job)) fail(std::move(job), {"network", std::move(message), 0, {}});
	} else {
		answered(std::move(job));
	}
}

void Worker::answered(std::unique_ptr<Job> job) {
	const int status = job->status;
	if(status >= 300 && status < 400 && status != 304) {
		redirect(std::move(job));
		return;
	}
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

void Worker::redirect(std::unique_ptr<Job> job) {
	char* location = nullptr; // NOLINT(misc-const-correctness): curl_easy_getinfo writes a char*
	curl_easy_getinfo(job->easy.get(), CURLINFO_REDIRECT_URL, &location);
	std::optional<Url> target = location ? parseUrl(location) : std::nullopt;
	const int status = job->status;
	if(!target || target->origin() != job->url.origin() || job->redirects >= maxRedirects) {
		network::Failure failure{"status", "The server redirected to a place that isn't followed.", status,
		                         std::move(job->body)};
		fail(std::move(job), std::move(failure));
		return;
	}
	job->request.url = location;
	job->url = std::move(*target);
	++job->redirects;
	if(status == 303 || ((status == 301 || status == 302) && job->request.method == "POST")) {
		job->request.method = "GET";
		job->request.body.clear();
	}
	job->easy.reset();
	job->notBefore = Job::Clock::now();
	waiting_.push_front(std::move(job));
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

void Worker::checkDeadlines() {
	const auto now = Job::Clock::now();
	std::vector<CURL*> late;
	for(const auto& [easy, job] : running_)
		if(job->request.stream != network::Stream::None && !job->headersDone && job->headersBy <= now) late.push_back(easy);
	for(CURL* easy : late)
		fail(takeRunning(easy), {"timeout", "The stream didn't start in time.", 0, {}});
}

std::chrono::milliseconds Worker::nextWait() const {
	const auto now = Job::Clock::now();
	auto until = now + longestWait;
	for(const auto& job : waiting_) until = std::min(until, job->notBefore);
	for(const auto& [easy, job] : running_)
		if(job->request.stream != network::Stream::None && !job->headersDone) until = std::min(until, job->headersBy);
	return std::max(std::chrono::milliseconds{0}, std::chrono::duration_cast<std::chrono::milliseconds>(until - now));
}

} // namespace affresco::net
