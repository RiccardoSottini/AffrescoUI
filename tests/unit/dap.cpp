/// @file
/// @brief The `dap` service: an adapter's messages are read, bounded and placed against the granted folder; and a
/// screen drives the deterministic fake adapter (tests/dap/fake_adapter.py) through framing, correlation, events, a
/// reverse request (`runInTerminal`), a timeout and a crash said once.

#include "dap/DapProtocol.hpp"
#include "dap/DapTransport.hpp"
#include "rpc/Framing.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/services/ServiceRegistry.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
namespace dap = affresco::services::dap;

namespace {

/// @brief The fake adapter (tests/dap), by an absolute path: it starts in the screen's folder, and __FILE__ may be
/// relative (as the compile cache gives it).
const std::filesystem::path fakeAdapter = AFFRESCOUI_FAKE_ADAPTER;

/// @brief An executor whose posted tasks the test runs by hand, on the main thread.
class PumpedExecutor final : public affresco::Executor {
public:
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		posted_.notify_all();
	}
	/// @brief Runs posted tasks until a condition holds.
	/// @param done The condition, checked after each batch.
	/// @return Whether it held within ten seconds.
	bool pumpUntil(const std::function<bool()>& done) {
		const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while(!done()) {
			std::vector<std::function<void()>> tasks;
			{
				std::unique_lock lock(mutex_);
				if(!posted_.wait_until(lock, end, [this] { return !tasks_.empty(); })) return done();
				tasks.swap(tasks_);
			}
			for(const auto& task : tasks) task();
		}
		return true;
	}

private:
	std::mutex mutex_;                         ///< Guards tasks_.
	std::condition_variable posted_;           ///< Signalled when a task is posted.
	std::vector<std::function<void()>> tasks_; ///< Tasks not yet run.
};

/// @brief A folder of its own under the system's temporary folder, removed at the end.
struct TemporaryFolder {
	std::filesystem::path path = std::filesystem::temp_directory_path() / "affrescoui-dap-test"; ///< The folder.
	TemporaryFolder() {
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path / "work");
		std::ofstream(path / "work" / "main.py") << "print('hello')\n";
	}
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	TemporaryFolder(TemporaryFolder&&) = delete;
	TemporaryFolder& operator=(TemporaryFolder&&) = delete;
};

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).at(json::json_pointer(pointer));
}

/// @brief A screen that drives a debug session step by step, one action per request.
constexpr const char* debugScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"services": { "dbg": { "provider": "dap", "process": "adapter", "root": "work", "timeout": 1500 } },
	"state": { "caps": null, "launched": false, "stops": [], "output": [], "terminal": null, "bps": null,
	           "stack": null, "page": null, "value": "", "error": "", "code": "", "ended": "",
	           "reason": "" },
	"actions": {
		"listen": [{ "subscribe": "dbg.stopped", "append": "state.stops", "id": "stops",
		             "do": [{ "set": "state.reason", "to": "{{event.reason}}" }] },
		           { "subscribe": "dbg.output", "append": "state.output", "id": "output",
		             "onError": [{ "set": "state.ended", "to": "{{error.message}}" }] }],
		"initialize": [{ "invoke": "dbg.initialize", "with": { "adapterID": "fake", "linesStartAt1": true },
		                 "into": "state.caps" }],
		"launch": [{ "invoke": "dbg.launch",
		             "with": { "program": "${workspaceFolder}/main.py", "console": "integratedTerminal" },
		             "onDone": [{ "set": "state.launched", "to": true }] }],
		"crash": [{ "invoke": "dbg.launch", "with": { "program": "main.py", "crash": 3 } }],
		"dbg:runInTerminal": [{ "set": "state.terminal", "to": "{{event}}" }, { "respond": { "processId": 4242 } }],
		"breakpoints": [{ "invoke": "dbg.setBreakpoints",
		                  "with": { "source": { "path": "main.py" }, "breakpoints": [{ "line": 5 }, { "line": 30 }] },
		                  "into": "state.bps" }],
		"run": [{ "invoke": "dbg.configurationDone" }],
		"stack": [{ "invoke": "dbg.stackTrace", "with": { "threadId": 1 }, "into": "state.stack" }],
		"page": [{ "invoke": "dbg.variables", "with": { "variablesReference": 3000, "start": 10, "count": 3 },
		           "into": "state.page" }],
		"evaluate": [{ "invoke": "dbg.evaluate", "with": { "expression": "1 + 2", "context": "repl" },
		               "onDone": [{ "set": "state.value", "to": "{{result.result}}" }] }],
		"unknown": [{ "invoke": "dbg.evaluate", "with": { "expression": "nope" },
		              "onError": [{ "set": "state.error", "to": "{{error.message}}" }] }],
		"hang": [{ "invoke": "dbg.evaluate", "with": { "expression": "hang" },
		           "onError": [{ "set": "state.code", "to": "{{error.code}}" }] }],
		"escape": [{ "invoke": "dbg.setBreakpoints", "with": { "source": { "path": "../secret.py" } },
		             "onError": [{ "set": "state.code", "to": "{{error.code}}" }] }]
	},
	"root": { "type": "label", "text": "debug" }
})json";

/// @brief A screen with the fake adapter granted, the folder added and an executor set.
struct Session {
	TemporaryFolder folder;                                                           ///< The granted folder.
	PumpedExecutor executor;                                                          ///< Runs what the service posts.
	ManualClock clock;                                                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(200, 100, clock); ///< A window with no screen.
	affresco::Screen screen = affresco::Screen::fromString(debugScreen);              ///< The screen.

	Session() {
		screen.services().allowProcess("adapter", "/usr/bin/env", {"python3", fakeAdapter.string()});
		screen.services().addRoot("work", folder.path / "work");
		screen.setExecutor(&executor);
		screen.build(*window);
	}
	/// @brief Runs an action and pumps until a condition on the state holds.
	/// @param action The action.
	/// @param done The condition.
	/// @return Whether it held in time.
	bool run(const char* action, const std::function<bool()>& done) {
		screen.runAction(action, window->root());
		return executor.pumpUntil(done);
	}
	/// @brief The value at a path of the state.
	/// @param pointer The path.
	/// @return The value.
	json at(const char* pointer) const { return stateAt(screen, pointer); }
};

} // namespace

TEST_CASE("dap: an adapter's messages are read, bounded and placed against the folder") {
	CHECK(std::holds_alternative<std::string>(dap::readMessage("not json")));
	CHECK(std::holds_alternative<std::string>(dap::readMessage(R"({"type": "response"})")));
	CHECK(std::holds_alternative<std::string>(dap::readMessage(R"({"seq": 1, "type": "event"})")));
	const dap::Message answer = dap::readMessage(
	    R"({"seq": 4, "type": "response", "request_seq": 2, "command": "evaluate", "success": false, "message": "no"})");
	REQUIRE(std::holds_alternative<dap::Response>(answer));
	CHECK(std::get<dap::Response>(answer).requestSeq == 2);
	CHECK(std::get<dap::Response>(answer).message == "no");

	// A message split anywhere is read whole
	const std::string framed = affresco::services::frameMessage(
	    affresco::services::Framing::ContentLength, R"({"seq": 1, "type": "event", "event": "initialized"})");
	affresco::services::FrameReader reader(affresco::services::Framing::ContentLength, {});
	std::vector<std::string> bodies;
	for(const char byte : framed) REQUIRE_FALSE(reader.feed(std::string_view(&byte, 1), bodies));
	REQUIRE(bodies.size() == 1);
	CHECK(std::holds_alternative<dap::Event>(dap::readMessage(bodies[0])));

	// Lists are cut to their limits, and sources placed
#ifdef _WIN32
	const std::string drive = "C:"; // an absolute path names its drive there
#else
	const std::string drive;
#endif
	const std::filesystem::path root = std::filesystem::path(drive + "/work");
	json threads = json::array();
	for(int index = 0; index < 1500; ++index)
		threads.push_back({{"id", index}, {"name", std::string(dap::longestText + 10, 't')}});
	const json bounded = dap::boundResponse("threads", {{"threads", threads}}, root);
	CHECK(bounded["threads"].size() == dap::mostThreads);
	CHECK(bounded["truncated"] == true);
	CHECK(bounded["threads"][0]["name"].get<std::string>().size() <= dap::longestText); // cut after the list is
	json inside = {{"path", drive + "/work/src/a.py"}};
	dap::place(inside, root);
	CHECK(inside["inside"] == true);
	CHECK(inside["relative"] == "src/a.py");
	json outside = {{"path", drive + "/etc/passwd"}};
	dap::place(outside, root);
	CHECK(outside["inside"] == false);

	// Output is limited per second, and what was dropped is counted
	dap::OutputBudget budget;
	const auto now = dap::OutputBudget::Clock::now();
	for(std::size_t index = 0; index < dap::outputEventsPerSecond; ++index) REQUIRE(budget.admit(10, now) == 0u);
	CHECK_FALSE(budget.admit(10, now));
	CHECK_FALSE(budget.admit(10, now));
	CHECK(budget.admit(10, now + std::chrono::seconds(2)) == 2u);
}

TEST_CASE("dap: a request's arguments are completed, and paths stay inside the folder") {
	const std::filesystem::path root = std::filesystem::temp_directory_path();
	const std::filesystem::path canonical = std::filesystem::canonical(root);
	json launch = {{"program", "${workspaceFolder}/main.py"}};
	CHECK_FALSE(dap::prepareArguments("launch", launch, canonical));
	CHECK(launch["program"] == (canonical.string() + "/main.py"));
	json escape = {{"source", {{"path", "../outside.py"}}}};
	CHECK(dap::prepareArguments("setBreakpoints", escape, canonical));
	json noRoot = {{"source", {{"path", "main.py"}}}};
	CHECK(dap::prepareArguments("setBreakpoints", noRoot, {}));
	json page = {{"variablesReference", 7}, {"count", 1000000}};
	CHECK_FALSE(dap::prepareArguments("variables", page, canonical));
	CHECK(page["count"] == dap::mostVariables);
}

TEST_CASE("dap: a screen debugs the fake adapter: correlation, events, a reverse request") {
	Session session;
	session.screen.runAction("listen", session.window->root());
	REQUIRE(session.run("initialize", [&] { return session.at("/caps").is_object(); }));
	CHECK(session.at("/caps/supportsCancelRequest") == true);
	CHECK(session.at("/caps/exceptionBreakpointFilters").size() == 2);

	// The launch waits on runInTerminal, which the screen answers
	REQUIRE(session.run("launch", [&] { return session.at("/launched") == true; }));
	CHECK(session.at("/terminal/kind") == "integrated");
	CHECK(session.at("/terminal/args/1").get<std::string>().ends_with("/main.py"));
	REQUIRE(session.executor.pumpUntil([&] { return !session.at("/output").empty(); }));
	CHECK(session.at("/output/0/category") == "console");

	REQUIRE(session.run("breakpoints", [&] { return session.at("/bps").is_object(); }));
	CHECK(session.at("/bps/breakpoints/0/verified") == true);
	CHECK(session.at("/bps/breakpoints/0/source/relative") == "main.py");
	CHECK(session.at("/bps/breakpoints/1/verified") == false);

	REQUIRE(session.run("run", [&] { return !session.at("/stops").empty(); }));
	CHECK(session.at("/stops/0/reason") == "breakpoint");
	CHECK(session.at("/reason") == "breakpoint"); // `do` sees the stream's event as `event`
	REQUIRE(session.run("stack", [&] { return session.at("/stack").is_object(); }));
	CHECK(session.at("/stack/stackFrames/0/line") == 5);
	CHECK(session.at("/stack/stackFrames/0/source/inside") == true);
	CHECK(session.at("/stack/stackFrames/1/source/inside") == false);

	REQUIRE(session.run("page", [&] { return session.at("/page").is_object(); }));
	CHECK(session.at("/page/variables").size() == 3);
	CHECK(session.at("/page/variables/0/value") == "100");
	REQUIRE(session.run("evaluate", [&] { return session.at("/value") == "3"; }));
	REQUIRE(session.run("unknown", [&] { return session.at("/error") != ""; }));
	CHECK(session.at("/error") == "name 'nope' is not defined");
	REQUIRE(session.run("escape", [&] { return session.at("/code") != ""; }));
}

TEST_CASE("dap: an unanswered request times out, and a crash is said once") {
	Session session;
	session.screen.runAction("listen", session.window->root());
	REQUIRE(session.run("initialize", [&] { return session.at("/caps").is_object(); }));
	REQUIRE(session.run("hang", [&] { return session.at("/code") != ""; }));
	CHECK(session.at("/code") == "timeout");

	session.screen.runAction("crash", session.window->root());
	REQUIRE(session.executor.pumpUntil([&] { return session.at("/ended") != ""; }));
	CHECK(session.at("/ended") == "the debug adapter ended unexpectedly (exit code 3)");
}

#ifndef _WIN32
TEST_CASE("dap: the connection to an adapter attached to isn't inherited by programs started later") {
	const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
	REQUIRE(listener >= 0);
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	socklen_t size = sizeof(address);
	REQUIRE(::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
	REQUIRE(::listen(listener, 1) == 0);
	REQUIRE(::getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0);
	const std::uint16_t port = ntohs(address.sin_port);
	{
		const auto transport = affresco::services::DapTransport::connect(port);
		int connected = -1; // the descriptor whose peer is the listener's port
		for(int fd = 3; fd < 1024 && connected < 0; ++fd) {
			sockaddr_in peer{};
			socklen_t length = sizeof(peer);
			if(fd != listener && ::getpeername(fd, reinterpret_cast<sockaddr*>(&peer), &length) == 0 &&
			   peer.sin_family == AF_INET && ntohs(peer.sin_port) == port)
				connected = fd;
		}
		REQUIRE(connected >= 0);
		CHECK((fcntl(connected, F_GETFD) & FD_CLOEXEC) != 0);
	}
	::close(listener);
}
#endif
