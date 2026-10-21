/// @file
/// @brief format::AsyncCalls and WorkerPool: slow native calls on worker threads, their results through the
/// Executor.

#include "AsyncCalls.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <atomic>
#include <exception>
#include <utility>

namespace affresco::format {

/* WorkerPool */

bool WorkerPool::run(std::function<void()> task) {
	const std::scoped_lock lock(mutex_);
	if(queue_.size() >= maxWaiting_) return false;
	queue_.push_back(std::move(task));
	if(workers_.size() < threads_) // one more thread while there are fewer than asked for
		workers_.emplace_back([this](const std::stop_token& stop) { work(stop); });
	wake_.notify_one();
	return true;
}

void WorkerPool::work(const std::stop_token& stop) {
	std::unique_lock lock(mutex_);
	while(wake_.wait(lock, stop, [this] { return !queue_.empty(); })) {
		const std::function<void()> task = std::move(queue_.front());
		queue_.pop_front();
		lock.unlock();
		task();
		lock.lock();
	}
}

/* AsyncCalls::Job */

/// @brief What a call's worker shares with the main thread: whether it was cancelled, and what it reported since the
/// last delivery, under a mutex. A delivery is posted when there is something new and none is waiting already, so a
/// function that reports fast posts no more than the event loop runs.
struct AsyncCalls::Job final : CallProgress {
	/// @brief A job.
	/// @param call Its call's id.
	/// @param executor Where deliveries are posted, or nullptr (delivered by the caller).
	/// @param calls The calls, used on the main thread only, while alive holds.
	/// @param alive Expires with them.
	Job(std::uint64_t call, Executor* executor, AsyncCalls* calls, std::weak_ptr<bool> alive)
	    : id(call), executor_(executor), calls_(calls), alive_(std::move(alive)) {}

	[[nodiscard]] bool cancelled() const noexcept override { return stopped.load(); }
	void progress(double fraction) override {
		report([fraction](Job& job) { job.progressed = std::clamp(fraction, 0.0, 1.0); });
	}
	void partial(json value) override {
		report([&value](Job& job) { job.partialResult = std::move(value); });
	}
	/// @brief Ends it with the function's result.
	/// @param value The result.
	void succeed(json value) {
		report([&value](Job& job) {
			job.result = std::move(value);
			job.finished = true;
		});
	}
	/// @brief Ends it with the function's error.
	/// @param what Why.
	void failed(std::string what) {
		report([&what](Job& job) {
			job.error = std::move(what);
			job.finished = true;
		});
	}

	const std::uint64_t id;            ///< Its call.
	std::atomic<bool> stopped{false};  ///< See cancelled().
	std::mutex mutex;                  ///< Guards what follows.
	std::optional<double> progressed;  ///< The latest progress not delivered yet.
	std::optional<json> partialResult; ///< The latest partial result not delivered yet.
	std::optional<json> result;        ///< The result, once finished without an error.
	std::optional<std::string> error;  ///< The error, once finished with one.
	bool finished = false;             ///< Whether the function returned.
	bool posted = false;               ///< Whether a delivery waits on the Executor.

private:
	/// @brief Records something new, and posts a delivery unless one waits already or it was cancelled.
	/// @param change Changes the job, under its mutex.
	void report(const std::function<void(Job&)>& change) {
		if(stopped) return;
		bool post = false;
		{
			const std::scoped_lock lock(mutex);
			change(*this);
			post = executor_ && !posted;
			posted = posted || post;
		}
		if(post)
			executor_->post([calls = calls_, alive = alive_, call = id] {
				if(alive.lock()) calls->deliver(call);
			});
	}

	Executor* executor_;        ///< See the constructor.
	AsyncCalls* calls_;         ///< See the constructor.
	std::weak_ptr<bool> alive_; ///< See the constructor.
};

/* AsyncCalls */

AsyncCalls::~AsyncCalls() { cancelAll(); }

bool AsyncCalls::start(const Running& s, const std::string& name, const Runtime::AsyncCall& function) {
	const json& step = s.step;
	Pending call{nullptr,
	             name,
	             {},
	             {},
	             {},
	             step.value("then", json()),
	             step.value("onError", json()),
	             &s.sender,
	             s.event,
	             snapshot(s.locals),
	             s.where};
	if(step.contains("into")) call.into = s.target("into");
	if(step.contains("loading")) call.loading = s.target("loading");
	if(step.contains("progress")) call.progress = s.target("progress");
	const json arguments = step.contains("with") ? s.value("with") : json::object();
	const double timeout = step.contains("timeout") ? s.milliseconds("timeout") : 0.0;

	const std::uint64_t id = next_++;
	const auto job = std::make_shared<Job>(id, executor_, this, alive_);
	call.job = job;
	if(call.loading) write(*call.loading, true, s.where + ".loading");
	pending_.emplace(id, std::move(call));
	const auto body = [job, function, arguments] {
		try {
			job->succeed(function(arguments, *job));
		} catch(const std::exception& error) { // never across threads: the reason is delivered
			job->failed(error.what());
		}
	};
	if(!executor_) {
		body();
		deliver(id);
		return true;
	}
	if(!pool_)
		pool_ = std::make_unique<WorkerPool>(std::clamp(std::thread::hardware_concurrency(), 2U, 4U), maxRunning);
	if(pending_.size() > maxRunning || !pool_->run(body)) {
		failCall(id, "too many calls running (at most " + std::to_string(maxRunning) + ")");
		return true;
	}
	if(timeout > 0.0 && engine_.window) engine_.scheduler.once(timeout, [this, id] { failCall(id, "timeout"); });
	return true;
}

void AsyncCalls::cancel(const std::string& name) {
	for(auto it = pending_.begin(); it != pending_.end();) {
		const auto next = std::next(it);
		if(it->second.name == name) finish(it);
		it = next;
	}
	if(engine_.document.stateChanged()) engine_.reconciler.requestStateUpdate();
}

void AsyncCalls::cancelAll() {
	for(auto& [id, call] : pending_) call.job->stopped = true;
	pending_.clear();
	alive_ = std::make_shared<bool>(true); // deliveries posted until now do nothing
}

void AsyncCalls::deliver(std::uint64_t id) {
	const auto it = pending_.find(id);
	if(it == pending_.end()) return; // cancelled, timed out or ended
	Job& job = *it->second.job;
	std::optional<double> progressed;
	std::optional<json> partialResult;
	bool finished = false;
	{
		const std::scoped_lock lock(job.mutex);
		progressed = std::exchange(job.progressed, std::nullopt);
		partialResult = std::exchange(job.partialResult, std::nullopt);
		finished = job.finished;
		job.posted = false;
	}
	const Pending& running = it->second;
	if(progressed && running.progress) write(*running.progress, *progressed, running.where + ".progress");
	if(partialResult && running.into) write(*running.into, std::move(*partialResult), running.where + ".into");
	if(finished) {
		const Pending call = finish(it);
		if(job.error) {
			if(call.onError.is_null()) engine_.report(call.where + ".call: " + call.name + ": " + *job.error);
			else runSteps(call, call.onError, "error", *job.error, call.where + ".onError");
		} else {
			if(call.into) write(*call.into, *job.result, call.where + ".into");
			if(!call.then.is_null()) runSteps(call, call.then, "result", *job.result, call.where + ".then");
		}
	}
	if(engine_.document.stateChanged()) engine_.reconciler.requestStateUpdate();
}

void AsyncCalls::failCall(std::uint64_t id, const std::string& error) {
	const auto it = pending_.find(id);
	if(it == pending_.end()) return;
	const Pending call = finish(it);
	if(call.onError.is_null()) engine_.report(call.where + ".call: " + call.name + ": " + error);
	else runSteps(call, call.onError, "error", error, call.where + ".onError");
	if(engine_.document.stateChanged()) engine_.reconciler.requestStateUpdate();
}

AsyncCalls::Pending AsyncCalls::finish(std::map<std::uint64_t, Pending>::iterator it) {
	Pending call = std::move(it->second);
	pending_.erase(it);
	call.job->stopped = true;
	if(call.loading) write(*call.loading, false, call.where + ".loading");
	return call;
}

void AsyncCalls::write(const json::json_pointer& pointer, json value, const std::string& where) {
	try {
		expect::shallowEnough(value, pointerDepth(pointer), where);
		statePlace(engine_.versions.state(), pointer, where) = std::move(value);
		engine_.document.markStateChanged(pointer);
	} catch(const std::exception& error) { // a revision may have changed the state since the call started
		engine_.report(error.what());
	}
}

void AsyncCalls::runSteps(const Pending& call, const json& steps, const char* variable, json value,
                          const std::string& where) {
	Window* window = engine_.window;
	Panel* sender = engine_.reconciler.recordOf(call.sender) ? const_cast<Panel*>(call.sender) : nullptr;
	if(!sender && window) sender = &window->root();
	if(!sender) return; // nothing built: no panel to run them from
	const auto scope = std::make_shared<const Scope>(
	    Scope{call.locals.get(), call.locals, json{{variable, std::move(value)}}, {}, {}});
	engine_.runner.handle(steps, *sender, call.event, *scope, where);
}

} // namespace affresco::format
