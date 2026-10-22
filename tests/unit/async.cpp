/// @file
/// @brief Asynchronous native calls: a slow function runs on a worker thread while frames go on, streams its progress
/// and partial results, times out, is cancelled, and changes nothing once its screen is gone; with a fake executor.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using affresco::CallProgress;
using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief An executor whose posted tasks the test runs by hand, on its own thread (the main thread).
class FakeExecutor final : public affresco::Executor {
public:
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		posted_.notify_all();
	}
	/// @brief Waits until a task was posted.
	/// @param most How long at most.
	/// @return Whether one was.
	bool waitForTask(std::chrono::milliseconds most = std::chrono::seconds(5)) {
		std::unique_lock lock(mutex_);
		return posted_.wait_for(lock, most, [this] { return !tasks_.empty(); });
	}
	/// @brief Runs the tasks posted until now.
	/// @return How many ran.
	std::size_t drain() {
		std::vector<std::function<void()>> tasks;
		{
			const std::scoped_lock lock(mutex_);
			tasks.swap(tasks_);
		}
		for(const auto& task : tasks) task();
		return tasks.size();
	}

private:
	std::mutex mutex_;
	std::condition_variable posted_;
	std::vector<std::function<void()>> tasks_;
};

/// @brief A screen whose "start" action calls "slow", into state.out, with its loading flag, progress and timeout;
/// "stop" cancels it.
constexpr const char* calls = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": { "out": null, "busy": false, "done": 0, "error": "", "ran": 0 },
	"actions": {
		"start": [{ "call": "slow", "with": { "n": 2 }, "into": "state.out", "loading": "state.busy",
		            "progress": "state.done", "timeout": 1000,
		            "then": [{ "set": "state.ran", "to": "{{result.n}}" }],
		            "onError": [{ "set": "state.error", "to": "{{error}}" }] }],
		"stop": [{ "cancel": "slow" }]
	},
	"root": { "type": "label", "text": "{{state.busy ? 'loading' : state.out}}" }
})json";

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).at(json::json_pointer(pointer));
}

/// @brief A gate a worker waits at until the test opens it.
struct Gate {
	std::promise<void> open;                             ///< Set to let the worker go.
	std::shared_future<void> opened = open.get_future(); ///< What the worker waits on.
};

} // namespace

TEST_CASE("format: a slow call doesn't hold frames back; its result arrives through the executor") {
	FakeExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Gate gate;
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow", [opened = gate.opened](const std::string& arguments, CallProgress&) {
		opened.wait(); // slow until the test says
		return arguments;
	});
	screen.setExecutor(&executor);
	screen.build(*window);

	screen.runAction("start", window->root());
	CHECK(stateAt(screen, "/busy") == true);
	const auto start = std::chrono::steady_clock::now();
	for(int frame = 0; frame < 10; ++frame) window->render();
	const auto elapsed = std::chrono::steady_clock::now() - start;
	CHECK(elapsed < std::chrono::milliseconds(50)); // ten frames while the call is still running
	CHECK(executor.drain() == 0);

	gate.open.set_value();
	REQUIRE(executor.waitForTask());
	CHECK(executor.drain() == 1);
	CHECK(stateAt(screen, "/out") == json{{"n", 2}});
	CHECK(stateAt(screen, "/ran") == 2);
	CHECK(stateAt(screen, "/busy") == false);
}

TEST_CASE("format: a slow call's progress and partial results arrive coalesced, the latest ones") {
	FakeExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Gate gate;
	std::promise<void> reported;
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow", [&reported, opened = gate.opened](const std::string&, CallProgress& call) {
		call.progress(0.25);
		call.partial(json{{"n", 1}});
		call.progress(0.5);
		call.partial(json{{"n", 1.5}});
		reported.set_value();
		opened.wait();
		call.progress(7.0); // more than 1 counts as 1
		return std::string(R"({"n": 3})");
	});
	screen.setExecutor(&executor);
	screen.build(*window);
	screen.runAction("start", window->root());

	reported.get_future().wait();
	CHECK(executor.drain() == 1); // one delivery for the four reports
	CHECK(stateAt(screen, "/done") == 0.5);
	CHECK(stateAt(screen, "/out") == json{{"n", 1.5}});
	CHECK(stateAt(screen, "/busy") == true);

	gate.open.set_value();
	while(stateAt(screen, "/busy") == true) { // the last progress and the result arrive in one delivery or two
		REQUIRE(executor.waitForTask());
		executor.drain();
	}
	CHECK(stateAt(screen, "/done") == 1.0);
	CHECK(stateAt(screen, "/out") == json{{"n", 3}});
	CHECK(stateAt(screen, "/ran") == 3);
	CHECK(stateAt(screen, "/busy") == false);
}

TEST_CASE("format: a slow call past its timeout ends with the error 'timeout', and what it returns is dropped") {
	FakeExecutor executor;
	ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Gate gate;
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow", [opened = gate.opened](const std::string& arguments, CallProgress&) {
		opened.wait();
		return arguments;
	});
	screen.setExecutor(&executor);
	screen.build(*window);
	screen.runAction("start", window->root());

	clock.advance(1001.0);
	window->render(); // the timer runs
	CHECK(stateAt(screen, "/error") == "timeout");
	CHECK(stateAt(screen, "/busy") == false);

	gate.open.set_value();
	CHECK_FALSE(executor.waitForTask(std::chrono::milliseconds(100))); // a timed-out job posts nothing
	CHECK(stateAt(screen, "/out").is_null());
	CHECK(stateAt(screen, "/ran") == 0);
}

TEST_CASE("format: a slow call that ends before its timeout leaves no timer behind") {
	FakeExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow", [](const std::string& arguments, CallProgress&) { return arguments; });
	screen.setExecutor(&executor);
	screen.build(*window);
	window->render();
	REQUIRE_FALSE(window->secondsToNextTimer());

	for(int call = 0; call < 3; ++call) {
		screen.runAction("start", window->root());
		CHECK(window->secondsToNextTimer()); // its timeout
		REQUIRE(executor.waitForTask());
		executor.drain();
		CHECK(stateAt(screen, "/ran") == 2);
		CHECK_FALSE(window->secondsToNextTimer()); // a call per event left a timer each, every frame looking at them
	}
	CHECK(stateAt(screen, "/error") == "");
}

TEST_CASE("format: a cancel step stops a slow call: the function sees it, and the screen keeps its state") {
	FakeExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	std::promise<void> sawCancel;
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow", [&sawCancel](const std::string& arguments, CallProgress& call) {
		while(!call.cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		sawCancel.set_value();
		return arguments;
	});
	screen.setExecutor(&executor);
	screen.build(*window);
	screen.runAction("start", window->root());
	screen.runAction("stop", window->root());
	CHECK(stateAt(screen, "/busy") == false);

	sawCancel.get_future().wait();
	CHECK(executor.drain() == 0);
	CHECK(stateAt(screen, "/out").is_null());
	CHECK(stateAt(screen, "/error") == "");
}

// The deliveries posted point at the runtime: if they ran after it, they would reach freed memory
// (AddressSanitizer: heap-use-after-free)
TEST_CASE("format: a screen destroyed while a slow call runs waits for it, and its deliveries do nothing") {
	FakeExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	auto screen = std::make_unique<affresco::Screen>(affresco::Screen::fromString(calls));
	screen->onAsyncCall("slow", [](const std::string& arguments, CallProgress& call) {
		call.progress(0.5); // posts a delivery
		while(!call.cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		return arguments;
	});
	screen->setExecutor(&executor);
	screen->build(*window);
	screen->runAction("start", window->root());
	REQUIRE(executor.waitForTask());

	screen.reset(); // cancels the call, joins the worker
	CHECK(executor.drain() == 1);
}

TEST_CASE("format: without an executor a slow call runs at once") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow", [](const std::string& arguments, CallProgress& call) {
		call.progress(0.5);
		return arguments;
	});
	screen.build(*window);
	screen.runAction("start", window->root());
	CHECK(stateAt(screen, "/out") == json{{"n", 2}});
	CHECK(stateAt(screen, "/ran") == 2);
	CHECK(stateAt(screen, "/busy") == false);
}

TEST_CASE("format: a slow call that throws runs onError with its message") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow",
	                   [](const std::string&, CallProgress&) -> std::string { throw std::runtime_error("disk full"); });
	screen.build(*window);
	screen.runAction("start", window->root());
	CHECK(stateAt(screen, "/error") == "disk full");
	CHECK(stateAt(screen, "/busy") == false);
}

// On a worker thread, what isn't a std::exception went past the call and ended the program
TEST_CASE("format: a slow call that throws something else, on a worker, runs onError too") {
	FakeExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow", [](const std::string&, CallProgress&) -> std::string { throw 42; });
	screen.setExecutor(&executor);
	screen.build(*window);
	screen.runAction("start", window->root());
	REQUIRE(executor.waitForTask());
	executor.drain();
	CHECK(stateAt(screen, "/error") == "an unknown error");
	CHECK(stateAt(screen, "/busy") == false);
}

TEST_CASE("format: at most 64 slow calls run at once; one more ends with an error") {
	FakeExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	Gate gate;
	affresco::Screen screen = affresco::Screen::fromString(calls);
	screen.onAsyncCall("slow", [opened = gate.opened](const std::string& arguments, CallProgress&) {
		opened.wait();
		return arguments;
	});
	screen.setExecutor(&executor);
	screen.build(*window);
	for(int call = 0; call < 64; ++call) screen.runAction("start", window->root());
	CHECK(stateAt(screen, "/error") == "");
	screen.runAction("start", window->root());
	CHECK(stateAt(screen, "/error").get<std::string>().starts_with("too many calls running"));
	gate.open.set_value();
}
