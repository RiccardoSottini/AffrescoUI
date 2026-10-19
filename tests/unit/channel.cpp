/// @file
/// @brief Unit tests for the channel module: the live protocol (Session) over a Channel kept in memory, and the
/// line framing of SocketChannel over a socket pair.

#include "Helpers.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/channel/Sockets.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

using affresco::json;
using affresco::test::MemoryChannel;

namespace {

/// @brief An Executor whose tasks wait until the test runs them, on the test's thread (the "main" thread).
class QueueExecutor final : public affresco::Executor {
public:
	/// @brief Keeps a task; called from a channel's reading thread.
	/// @param task The work.
	void post(std::function<void()> task) override {
		const std::lock_guard lock(mutex_);
		tasks_.push_back(std::move(task));
	}

	/// @brief Runs the posted tasks until a condition holds, for at most 10 seconds.
	/// @param done The condition.
	/// @return Whether it held in time.
	bool runUntil(const std::function<bool()>& done) {
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
		while(!done()) {
			if(std::chrono::steady_clock::now() > deadline) return false;
			std::vector<std::function<void()>> ready;
			{
				const std::lock_guard lock(mutex_);
				ready.swap(tasks_);
			}
			for(auto& task : ready) task();
			if(ready.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		return true;
	}

private:
	std::mutex mutex_;                         ///< Guards tasks_.
	std::vector<std::function<void()>> tasks_; ///< Posted, not run yet.
};

/// @brief Writes all of a text to a descriptor, as the other end of a channel.
/// @param fd The descriptor.
/// @param text The bytes.
void writeAll(int fd, std::string_view text) {
	for(std::size_t written = 0; written < text.size();) {
		const ssize_t n = write(fd, text.data() + written, text.size() - written);
		if(n <= 0) return;
		written += static_cast<std::size_t>(n);
	}
}

/// @brief A screen under version control, and a peer connected to it.
struct Peer {
	affresco::format::Rules rules;                                                  ///< The screen format.
	affresco::vcs::Repository versions{rules, affresco::test::permissionsScreen()}; ///< The screen.
	MemoryChannel channel;                                                          ///< What the peer receives.
};

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

TEST_CASE("channel: a peer's text in a rejected revision reaches the log as one line") {
	Peer peer;
	const affresco::channel::Session agent(peer.channel, peer.versions, "agent");
	std::vector<std::string> logged;
	affresco::setLogSink([&](affresco::LogLevel, std::string_view message) { logged.emplace_back(message); });
	peer.channel.receive(R"({"type": "revision", "id": 1, "set": {"#a\nFAKE: approved\u001b[2J/text": "x"}})");
	affresco::setLogSink(nullptr);

	REQUIRE(logged.size() == 1);
	CHECK(logged[0].find("#a\\nFAKE: approved\\x1b[2J") != std::string::npos);
	CHECK(std::ranges::none_of(logged[0], [](char c) { return c == '\n' || c == '\x1b'; }));
	CHECK(peer.channel.sent.back()["type"] == "rejected"); // the peer still gets its own text back, as JSON
}

// Before, a peer that never sent a line break made the app keep everything it sent, without limit, and searched
// it all again after every 4 KiB read
TEST_CASE("channel: a SocketChannel splits lines across reads, and closes on a line longer than maxMessageBytes") {
	int ends[2] = {-1, -1};
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, ends) == 0);
	QueueExecutor executor;
	affresco::channel::SocketChannel channel(executor, ends[0]);
	std::vector<std::string> received;
	bool closed = false;
	channel.onMessage.connect([&](const std::string& line) { received.push_back(line); });
	channel.onClosed.connect([&] { closed = true; });

	std::thread peer([other = ends[1]] {
		writeAll(other, "first\n\nsec");
		std::this_thread::sleep_for(std::chrono::milliseconds(20)); // "second" arrives in two reads
		writeAll(other, "ond\n");
		// One byte too many, so the channel reads it all before it closes (or the writer would wait for it)
		const std::string block(std::size_t{1} << 20, 'x');
		for(std::size_t sent = 0; sent < affresco::maxMessageBytes; sent += block.size()) writeAll(other, block);
		writeAll(other, "x");
	});
	CHECK(executor.runUntil([&] { return closed; }));
	peer.join();
	close(ends[1]);

	CHECK(received == std::vector<std::string>{"first", "second"});
}
