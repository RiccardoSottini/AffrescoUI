/// @file
/// @brief runProgram() (affresco/services/Program.hpp), on ChildProcess.

#include "process/Process.hpp"

#include <affresco/services/Program.hpp>

#include <array>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>

namespace affresco::services {

ProgramRun runProgram(const Launch& launch, std::string_view input, std::chrono::milliseconds timeout,
                      const std::function<bool()>& stopped, std::size_t maxOutput) {
	ProgramRun run;
	std::unique_ptr<ChildProcess> child;
	try {
		child = ChildProcess::start(launch);
	} catch(const std::exception&) {
		return run;
	}

	// A watchdog ends the child at the time limit or when stopped; ending it makes a blocked read return
	std::mutex mutex;
	std::condition_variable finished;
	bool done = false;
	std::atomic<ProgramRun::End> ended = ProgramRun::End::Exited;
	std::thread watchdog([&] {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		std::unique_lock lock(mutex);
		while(!done) {
			const bool late = std::chrono::steady_clock::now() >= deadline;
			if(late || (stopped && stopped())) {
				ended = late ? ProgramRun::End::TimedOut : ProgramRun::End::Stopped;
				lock.unlock();
				child->end();
				return;
			}
			finished.wait_for(lock, std::chrono::milliseconds(100));
		}
	});

	const bool written = child->write(input);
	child->closeInput();
	std::array<char, 65536> buffer{};
	bool tooLarge = false;
	// A program it started that left its group may keep its stdout open after it ended: the read never waits for
	// more than 100 ms, so the watchdog's end is always seen
	while(written && ended == ProgramRun::End::Exited) {
		if(!child->readable(std::chrono::milliseconds(100))) continue;
		const std::ptrdiff_t count = child->read(buffer.data(), buffer.size());
		if(count <= 0) break;
		if(run.output.size() + static_cast<std::size_t>(count) > maxOutput) {
			tooLarge = true;
			break;
		}
		run.output.append(buffer.data(), static_cast<std::size_t>(count));
	}
	if(tooLarge) child->end();
	run.status = child->wait();
	{
		const std::scoped_lock lock(mutex);
		done = true;
	}
	finished.notify_all();
	watchdog.join();
	run.end = ended != ProgramRun::End::Exited ? ended.load() : (tooLarge ? ProgramRun::End::TooLarge : ended.load());
	return run;
}

} // namespace affresco::services
