#pragma once

/// @file
/// @brief format::AsyncCalls: "call" steps whose native function is slow, run on a worker pool so no frame waits for
/// them, with a loading flag, progress, streamed partial results, a timeout and cancellation.

#include "affresco/core/Executor.hpp"
#include "affresco/format/Runtime.hpp"

#include "Expression.hpp"
#include "StepRegistry.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief A few worker threads that run tasks in the order given, started on first use.
///
/// Invariants: at most maxWaiting tasks wait at once (run() refuses more); destroying it stops the threads and joins
/// them, after the task each one runs has returned. Thread-safe.
class WorkerPool {
public:
	/// @brief A pool.
	/// @param threads How many threads it starts (at least one).
	/// @param maxWaiting The most tasks waiting for a thread.
	WorkerPool(std::size_t threads, std::size_t maxWaiting)
	    : threads_(threads ? threads : 1), maxWaiting_(maxWaiting) {}

	/// @brief Queues a task for a worker thread.
	/// @param task The work; must not throw.
	/// @return false if too many tasks wait already (it isn't queued).
	[[nodiscard]] bool run(std::function<void()> task);

private:
	/// @brief Runs queued tasks until asked to stop. Runs on a worker thread.
	/// @param stop Its stop token.
	void work(const std::stop_token& stop);

	std::size_t threads_;                     ///< Threads it starts.
	std::size_t maxWaiting_;                  ///< See run().
	std::mutex mutex_;                        ///< Guards the queue.
	std::condition_variable_any wake_;        ///< The workers wait on it for tasks.
	std::deque<std::function<void()>> queue_; ///< Tasks waiting.
	std::vector<std::jthread> workers_;       ///< Declared last: stopped and joined first.
};

/// @brief The asynchronous native calls of a runtime.
///
/// A call starts on the main thread: its "loading" flag turns true, and its function runs on the worker pool, given
/// the step's "with" and a CallProgress. What the function reports and returns comes back through the Executor,
/// coalesced to the latest values: "progress" and partial results into "into", then the result into "into" and the
/// "then" steps (with `result`), or "onError" (with `error`); "loading" turns false. A "timeout" ends it with the error
/// "timeout"; a "cancel" step ends every call of a name without one. Without an Executor (headless, tests) the function
/// runs at once on the main thread. Invariants: at most maxRunning calls run at once; once cancelled (cancelAll() too)
/// a call changes nothing, whatever its function still returns. Callers: the "call" and "cancel" steps, ~Runtime. Main
/// thread only, except what the workers reach through the jobs.
class AsyncCalls {
public:
	static constexpr std::size_t maxRunning = 64; ///< Calls running or waiting for a worker at once.

	/// @brief The calls of an engine.
	/// @param engine The runtime; outlives this object.
	explicit AsyncCalls(Engine& engine) : engine_(engine) {}

	AsyncCalls(const AsyncCalls&) = delete;
	AsyncCalls& operator=(const AsyncCalls&) = delete;
	AsyncCalls(AsyncCalls&&) = delete;
	AsyncCalls& operator=(AsyncCalls&&) = delete;
	/// @brief Cancels the calls and joins the workers, after the functions they run return.
	~AsyncCalls();

	/// @brief Sets where results are posted (the Application, from Runtime::createWindow()).
	/// @param executor The executor, which must outlive the runtime; nullptr runs calls on the main thread.
	void setExecutor(Executor* executor) noexcept { executor_ = executor; }
	/// @brief Starts a "call" step's asynchronous function.
	/// @param s The step.
	/// @param name The function's name.
	/// @param function The function.
	/// @return Whether to keep going.
	/// @throws DocumentError If a path or a value of the step is wrong.
	bool start(const Running& s, const std::string& name, const Runtime::AsyncCall& function);
	/// @brief Cancels the running calls of a function: they change nothing more, "loading" turns false.
	/// @param name The function's name.
	void cancel(const std::string& name);
	/// @brief Cancels every call, which then does nothing when it returns (~Runtime).
	void cancelAll();

	struct Job;

private:
	/// @brief What the main thread keeps of a running call.
	struct Pending {
		std::shared_ptr<Job> job;                   ///< What the worker shares.
		std::string name;                           ///< The function's name.
		std::optional<json::json_pointer> into;     ///< "into".
		std::optional<json::json_pointer> loading;  ///< "loading".
		std::optional<json::json_pointer> progress; ///< "progress".
		json then;                                  ///< "then" steps, or null.
		json onError;                               ///< "onError" steps, or null.
		Panel* sender = nullptr;                    ///< The panel it comes from (may be gone).
		json event;                                 ///< The event.
		SharedScope locals;                         ///< Variables.
		std::string where;                          ///< The step's place.
		std::uint64_t timeout = 0;                  ///< Its "timeout" timer (Scheduler::once()), or 0 for none.
	};

	/// @brief Applies what a job reported since the last time (main thread, through the Executor).
	/// @param id The job's call.
	void deliver(std::uint64_t id);
	/// @brief Ends a call with an error ("timeout", or a failure) and runs its "onError".
	/// @param id The call.
	/// @param error The error.
	void failCall(std::uint64_t id, const std::string& error);
	/// @brief Ends a call: it is forgotten, its job cancelled, its "loading" false.
	/// @param it The call.
	/// @return What it kept.
	Pending finish(std::map<std::uint64_t, Pending>::iterator it);
	/// @brief Writes a value into the state at a path; a path the state can't take is reported.
	/// @param pointer The path.
	/// @param value The value.
	/// @param where Its place, for errors.
	void write(const json::json_pointer& pointer, json value, const std::string& where);
	/// @brief Runs a call's "then" or "onError" steps with one more variable.
	/// @param call The call.
	/// @param steps The steps.
	/// @param variable The variable's name.
	/// @param value Its value.
	/// @param where Their place.
	void runSteps(const Pending& call, const json& steps, const char* variable, json value, const std::string& where);

	Engine& engine_;                                             ///< The runtime.
	Executor* executor_ = nullptr;                               ///< See setExecutor().
	std::map<std::uint64_t, Pending> pending_;                   ///< The calls running, by id.
	std::uint64_t next_ = 1;                                     ///< The next call's id.
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); ///< Posted deliveries do nothing once it is reset.
	std::unique_ptr<WorkerPool> pool_; ///< Started on the first call; declared last, joined first.
};

} // namespace affresco::format
