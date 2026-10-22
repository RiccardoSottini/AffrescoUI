/// @file
/// @brief OAuthTimer: tasks run on the main thread after a delay (OAuthTimer.hpp).

#include "OAuthTimer.hpp"

#include <algorithm>
#include <vector>

namespace affresco::net {

OAuthTimer::OAuthTimer(Executor& executor)
    : executor_(executor), thread_([this](const std::stop_token& stop) { run(stop); }) {}

OAuthTimer::~OAuthTimer() {
	alive_.reset();
	thread_.request_stop();
	changed_.notify_all();
}

OAuthTimer::Id OAuthTimer::after(std::chrono::milliseconds delay, std::function<void()> task) {
	Id id = 0;
	{
		const std::scoped_lock lock(mutex_);
		id = next_++;
		waiting_.emplace(id, Waiting{std::chrono::steady_clock::now() + delay, std::move(task)});
	}
	changed_.notify_all();
	return id;
}

void OAuthTimer::cancel(Id id) {
	const std::scoped_lock lock(mutex_);
	waiting_.erase(id);
}

void OAuthTimer::run(const std::stop_token& stop) {
	std::unique_lock lock(mutex_);
	while(!stop.stop_requested()) {
		const auto now = std::chrono::steady_clock::now();
		auto first = now + std::chrono::hours{1}; // never time_point::max(): some clocks overflow converting it
		std::vector<Id> due;
		for(const auto& [id, waiting] : waiting_)
			if(waiting.due <= now) due.push_back(id);
			else first = std::min(first, waiting.due);
		for(const Id id : due) {
			// Posted by handle: a cancel() before it runs still stops it
			executor_.post([this, alive = std::weak_ptr<bool>(alive_), id] {
				if(!alive.lock()) return;
				std::function<void()> task;
				{
					const std::scoped_lock taken(mutex_);
					const auto found = waiting_.find(id);
					if(found == waiting_.end()) return;
					task = std::move(found->second.task);
					waiting_.erase(found);
				}
				task();
			});
			waiting_.at(id).due = std::chrono::steady_clock::time_point::max(); // posted: not again
		}
		changed_.wait_until(lock, stop, first, [this, seen = next_] { return next_ != seen; });
	}
}

} // namespace affresco::net
