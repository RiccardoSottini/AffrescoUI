#pragma once

/// @file
/// @brief The worker thread of a Client: one curl multi handle running every HTTP request, with the per-origin
/// limit, retries, redirects within the origin and the stream deadlines. Requests come in, and are cancelled, through
/// a queue under a mutex; outcomes go out through the Executor (Delivery.hpp).

#include "http/Curl.hpp"
#include "http/Delivery.hpp"
#include "http/Streams.hpp"
#include "affresco/net/Client.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace affresco::net {

/// @brief One request, from its start to its outcome (worker thread only, once submitted).
struct Job {
	using Clock = std::chrono::steady_clock; ///< The clock of deadlines.

	std::uint64_t id = 0;                         ///< Its number.
	network::Request request;                     ///< What to send (its url and method change on a redirect).
	Url url;                                      ///< request.url, parsed: its origin is what may be reached.
	std::shared_ptr<RequestDelivery> delivery;    ///< Where the outcome goes.
	Executor* executor = nullptr;                 ///< The main thread.
	std::size_t maxUndelivered = 0;               ///< See ClientOptions.
	curl::Easy easy;                              ///< While it runs.
	curl::List headers;                           ///< The request's headers, while it runs.
	int attempts = 0;                             ///< Retries made.
	int redirects = 0;                            ///< Redirects followed.
	Clock::time_point notBefore{};                ///< A retry waits until then.
	Clock::time_point headersBy{};                ///< A stream fails with "timeout" if its headers aren't in by then.

	int status = 0;                               ///< The status of the response being read.
	network::Headers responseHeaders;             ///< Its headers.
	std::size_t headerBytes = 0;                  ///< Their size.
	bool headersDone = false;                     ///< The final response's headers are in.
	std::string body;                             ///< Its body (not a stream's).
	bool tooLarge = false;                        ///< A limit was passed: the transfer was aborted.
	std::unique_ptr<EventStreamParser> events;    ///< Reads a stream of server-sent events.
	std::unique_ptr<LineStreamParser> lines;      ///< Reads a stream of lines.

	/// @brief Whether the body is a stream being delivered (a stream request, and a 2xx response).
	[[nodiscard]] bool streaming() const noexcept {
		return request.stream != network::Stream::None && status >= 200 && status < 300;
	}
	/// @brief Forgets the response, before an attempt.
	void resetResponse();
};

/// @brief The thread that runs a Client's HTTP requests.
class Worker {
public:
	/// @brief Starts the thread.
	/// @param executor The main thread.
	/// @param options The client's options.
	Worker(Executor& executor, ClientOptions options);
	/// @brief Stops the thread; requests still running end without a word.
	~Worker();
	Worker(const Worker&) = delete;
	Worker& operator=(const Worker&) = delete;
	Worker(Worker&&) = delete;
	Worker& operator=(Worker&&) = delete;

	/// @brief Queues a request (any thread).
	/// @param request The request, checked.
	/// @param url Its URL, parsed.
	/// @param delivery Where its outcome goes.
	/// @return Its number, for cancel().
	[[nodiscard]] std::uint64_t submit(network::Request request, Url url, std::shared_ptr<RequestDelivery> delivery);
	/// @brief Cancels a request (any thread): it stops, and nothing is delivered.
	/// @param id Its number.
	void cancel(std::uint64_t id);
	/// @brief How many requests are submitted and not finished.
	[[nodiscard]] std::size_t jobs() const noexcept { return jobs_; }

private:
	/// @brief A job to add, or (with no job) one to cancel.
	struct Command {
		std::uint64_t id = 0;     ///< The job's number.
		std::unique_ptr<Job> job; ///< The job to add, or nullptr to cancel.
	};

	/// @brief The thread's loop.
	/// @param stop Asks it to end.
	void run(const std::stop_token& stop);
	/// @brief Takes the commands queued since the last turn.
	void takeCommands();
	/// @brief Drops a job, wherever it is.
	/// @param id Its number.
	void drop(std::uint64_t id);
	/// @brief Starts the waiting jobs whose time has come and whose origin has room.
	void startWaiting();
	/// @brief Sets a job's easy handle up and adds it to the multi handle.
	/// @param job The job.
	/// @return An error, or nullopt.
	std::optional<std::string> begin(Job& job);
	/// @brief Sets a job's method and body.
	/// @param job The job.
	static void applyMethod(Job& job);
	/// @brief Takes a job out of the multi handle and the running jobs.
	/// @param easy Its easy handle.
	/// @return The job, or nullptr if it was not running.
	std::unique_ptr<Job> takeRunning(CURL* easy);
	/// @brief Handles a finished transfer.
	/// @param easy Its handle.
	/// @param code curl's result.
	void finished(CURL* easy, CURLcode code);
	/// @brief Handles a transfer that ended with a whole response.
	/// @param job The job.
	void answered(std::unique_ptr<Job> job);
	/// @brief Follows a redirect within the origin, or fails.
	/// @param job The job.
	void redirect(std::unique_ptr<Job> job);
	/// @brief Queues a job to try again, if it may.
	/// @param job The job (left as it is if it may not).
	/// @return Whether it was queued.
	bool retry(std::unique_ptr<Job>& job);
	/// @brief Ends a job with a failure.
	/// @param job The job.
	/// @param failure The failure.
	void fail(std::unique_ptr<Job> job, network::Failure failure);
	/// @brief Fails the streams whose headers are late.
	void checkDeadlines();
	/// @brief How long the next poll may wait.
	[[nodiscard]] std::chrono::milliseconds nextWait() const;

	Executor& executor_;                                   ///< See the constructor.
	ClientOptions options_;                                ///< See the constructor.
	curl::Multi multi_;                                    ///< Runs the transfers.
	std::mutex mutex_;                                     ///< Guards commands_.
	std::vector<Command> commands_;                        ///< Queued by submit() and cancel().
	std::atomic<std::uint64_t> nextId_{0};                 ///< The last number given.
	std::atomic<std::size_t> jobs_{0};                     ///< See jobs().
	std::deque<std::unique_ptr<Job>> waiting_;             ///< Jobs not running yet (worker thread).
	std::unordered_map<CURL*, std::unique_ptr<Job>> running_; ///< Jobs in the multi handle (worker thread).
	std::unordered_map<std::string, std::size_t> perOrigin_;  ///< Running jobs per origin (worker thread).
	std::jthread thread_;                                  ///< Last: it starts once everything else is ready.
};

} // namespace affresco::net
