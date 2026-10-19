/// @file
/// @brief Unit tests for the channel module: the live protocol (Session) over a Channel kept in memory, and
/// ProcessChannel with small programs run by the system shell.

#include "Helpers.hpp"

#include <affresco/channel/Channels.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::test::MemoryChannel;
using namespace std::chrono_literals;

namespace {

/// @brief A screen under version control, and a peer connected to it.
struct Peer {
	affresco::format::Rules rules;                                                  ///< The screen format.
	affresco::vcs::Repository versions{rules, affresco::test::permissionsScreen()}; ///< The screen.
	MemoryChannel channel;                                                          ///< What the peer receives.
};

/// @brief An Executor whose tasks run when the test asks, on the test's thread: it stands in for the main thread.
class QueueExecutor final : public affresco::Executor {
public:
	/// @brief Queues a task. Called from a channel's reading thread.
	/// @param task The task.
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		ready_.notify_one();
	}

	/// @brief Runs tasks as they arrive until @p done says so, or until @p timeout passes.
	/// @param done Checked before each task.
	/// @param timeout How long to wait in all.
	/// @return Whether @p done became true in time.
	bool runUntil(const std::function<bool()>& done, std::chrono::milliseconds timeout = 10s) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while(!done()) {
			std::unique_lock lock(mutex_);
			if(!ready_.wait_until(lock, deadline, [this] { return !tasks_.empty(); })) return false;
			const std::function<void()> task = std::move(tasks_.front());
			tasks_.pop_front();
			lock.unlock();
			task();
		}
		return true;
	}

private:
	std::mutex mutex_;                        ///< Guards tasks_.
	std::condition_variable ready_;           ///< Signalled when a task is queued.
	std::deque<std::function<void()>> tasks_; ///< Waiting to run, oldest first.
};

/// @brief What a ProcessChannel delivered.
struct Received {
	/// @brief Records the messages and the close of @p channel.
	/// @param channel The channel.
	explicit Received(affresco::Channel& channel) {
		channel.onMessage.connect([this](const std::string& line) { lines.push_back(line); });
		channel.onClosed.connect([this] { closed = true; });
	}

	std::vector<std::string> lines; ///< Every message, oldest first.
	bool closed = false;            ///< Whether onClosed fired.
};

/// @brief Picks the command for this platform's shell.
/// @param posix The command for /bin/sh.
/// @param windows The command for cmd.exe.
/// @return The one this platform runs.
const char* shellCommand([[maybe_unused]] const char* posix, [[maybe_unused]] const char* windows) {
#if defined(_WIN32)
	return windows;
#else
	return posix;
#endif
}

} // namespace

TEST_CASE("channel: a Session sends hello when created") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	REQUIRE(peer.channel.sent.size() == 1);
	const json& hello = peer.channel.sent[0];
	CHECK(hello.at("type") == "hello");
	CHECK(hello.at("format") == "affrescoui-screen");
	CHECK(hello.at("revision") == 0);
	CHECK(hello.at("head").at("id") == peer.versions.head().id);
	CHECK(hello.at("document") == peer.versions.document());
	CHECK(hello.at("state") == json{{"count", 0}});
	CHECK(hello.at("mode") == "default");
	CHECK(hello.at("you") == "agent");
	CHECK(hello.at("canReview") == false);
}

TEST_CASE("channel: a message of an unknown type gets an error") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	peer.channel.receive(R"({"type": "launchRockets", "id": 7})");
	REQUIRE(peer.channel.sent.size() == 2);
	CHECK(peer.channel.sent[1] ==
	      json{{"type", "error"}, {"message", "unknown message type 'launchRockets'"}, {"id", 7}});

	peer.channel.receive("not json");
	REQUIRE(peer.channel.sent.size() == 3);
	CHECK(peer.channel.sent[2]["type"] == "error");
	CHECK(peer.versions.head().number == 0);
}

TEST_CASE("channel: an accept from a peer that may not review is rejected") {
	Peer peer;
	const affresco::channel::Session agent(peer.channel, peer.versions, "agent");

	// The agent proposes a change it may not make directly, then tries to approve it itself
	peer.channel.receive(R"({"type": "revision", "id": 1, "set": {"#title/text": "Mine"}})");
	const auto proposed =
	    std::ranges::find(peer.channel.sent, json("proposed"), [](const json& m) { return m.value("type", json()); });
	REQUIRE(proposed != peer.channel.sent.end());
	const int proposal = (*proposed)["proposal"];
	const std::string head = (*proposed)["commit"];

	peer.channel.sent.clear();
	peer.channel.receive(json{{"type", "accept"}, {"id", 2}, {"proposal", proposal}, {"head", head}}.dump());
	REQUIRE(peer.channel.sent.size() == 1);
	CHECK(peer.channel.sent[0] == json{{"type", "rejected"},
	                                   {"error", "permission denied: 'agent' may not review changes"},
	                                   {"revision", 0},
	                                   {"id", 2}});
	CHECK(peer.versions.proposals().size() == 1); // still waiting for a reviewer
	CHECK(peer.versions.head().number == 0);
}

// Found by fuzzing (tests/fuzz, session): the parser's out_of_range got past Session::read and stopped the app
TEST_CASE("channel: a message with a number too large for a double gets an error") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	peer.channel.receive(R"({"type": "revision", "id": 1, "set": {"/state/count": 1e400}})");
	REQUIRE(peer.channel.sent.size() == 2);
	CHECK(peer.channel.sent[1]["type"] == "error");
	CHECK(peer.channel.sent[1]["message"].get<std::string>().find("number overflow") != std::string::npos);
	CHECK(peer.versions.state() == json{{"count", 0}});
}

// Found by fuzzing (tests/fuzz, session): the parse error quoted the bytes it read, and writing the answer threw
TEST_CASE("channel: a line that isn't UTF-8 gets an error, with the bytes replaced") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	peer.channel.receive("{\"type\": \xff}");
	REQUIRE(peer.channel.sent.size() == 2);
	CHECK(peer.channel.sent[1]["type"] == "error");
	CHECK(peer.channel.sent[1]["message"].get<std::string>().find("\xef\xbf\xbd") != std::string::npos); // U+FFFD
}

// Found by fuzzing (tests/fuzz, session, under UBSan): a limit of 1e20 was converted to an int, which is undefined
TEST_CASE("channel: getHistory takes any number as its limit") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");
	REQUIRE(peer.versions.submit({{"author", "studio"}, {"set", {{"#title/text", "A"}}}}).revision == 1);
	REQUIRE(peer.versions.submit({{"author", "studio"}, {"set", {{"#title/text", "B"}}}}).revision == 2);

	const std::vector<std::pair<std::string, std::size_t>> cases = {{"1", 1},  {"2.5", 2},  {"0", 3},
	                                                                {"-1", 3}, {"1e20", 3}, {"-1e20", 3}};
	for(const auto& [limit, entries] : cases) {
		CAPTURE(limit);
		peer.channel.sent.clear();
		peer.channel.receive(R"({"type": "getHistory", "limit": )" + limit + "}");
		REQUIRE(peer.channel.sent.size() == 1);
		REQUIRE(peer.channel.sent[0]["type"] == "history");
		CHECK(peer.channel.sent[0]["entries"].size() == entries);
		CHECK(peer.channel.sent[0]["entries"].back()["number"] == 2); // the newest
	}
}

TEST_CASE("channel: a ProcessChannel delivers each line of the child's output, then closes") {
	QueueExecutor executor;
	affresco::channel::ProcessChannel channel(executor,
	                                          shellCommand("printf 'one\\n\\ntwo\\n'", "echo one& echo.& echo two"));
	Received received(channel);

	REQUIRE(executor.runUntil([&] { return received.closed; }));
	CHECK(received.lines == std::vector<std::string>{"one", "two"}); // the empty line is skipped
	CHECK_FALSE(channel.isRunning());
}

TEST_CASE("channel: a ProcessChannel sends messages to the child's stdin") {
	QueueExecutor executor;
	// The child echoes its first line and exits
	affresco::channel::ProcessChannel channel(
	    executor, shellCommand("head -n 1", "powershell -NoProfile -Command \"[Console]::In.ReadLine()\""));
	Received received(channel);

	channel.send(R"({"type": "hello"})");
	REQUIRE(executor.runUntil([&] { return received.closed; }));
	CHECK(received.lines == std::vector<std::string>{R"({"type": "hello"})"});
}

TEST_CASE("channel: closing a ProcessChannel ends the child and the programs it started") {
	QueueExecutor executor;
	std::chrono::steady_clock::time_point start;
	{
		// The shell waits for its own child, which ignores stdin and holds stdout open: both must be ended, or the
		// reader never finishes. The child says when it runs, so the channel isn't closed before it exists.
		affresco::channel::ProcessChannel channel(executor,
		                                          shellCommand("/bin/sh -c 'echo started; exec sleep 30'; echo late",
		                                                       "echo started& ping -n 30 127.0.0.1 >nul& echo late"));
		Received received(channel);
		REQUIRE(executor.runUntil([&] { return !received.lines.empty(); }));
		CHECK(received.lines.front() == "started");
		start = std::chrono::steady_clock::now();
	}
	CHECK(std::chrono::steady_clock::now() - start < 10s);
}

#if !defined(_WIN32)
TEST_CASE("channel: a ProcessChannel's child doesn't inherit another channel's pipes") {
	QueueExecutor executor;
	// The descriptors open in a child of this process
	const auto openDescriptors = [&executor] {
		affresco::channel::ProcessChannel listing(executor, "ls /dev/fd/");
		Received received(listing);
		REQUIRE(executor.runUntil([&] { return received.closed; }));
		return received.lines;
	};

	const std::vector<std::string> before = openDescriptors();
	const affresco::channel::ProcessChannel other(executor, "cat");
	CHECK(openDescriptors() == before);
}
#endif
