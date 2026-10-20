/// @file
/// @brief Unit tests for the limits on peers: the token bucket, a Session's limits on size, messages and changes, the
/// user check on accepted connections, and a SocketChannel's bounded send queue.

#include "Helpers.hpp"

#include <affresco/channel/Limits.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/channel/Sockets.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/core/ScreenImages.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

using affresco::json;
using affresco::channel::PeerLimits;
using affresco::channel::TokenBucket;
using namespace std::chrono_literals;

namespace {

/// @brief A screen under version control, a peer connected to it through a Session with the given limits, and what
/// the log said.
struct LimitedPeer {
	/// @brief Connects the peer, as the author "studio" (who may change anything), and catches the log.
	/// @param limits Its limits.
	/// @param images What draws its pictures, if anything; must outlive the peer.
	explicit LimitedPeer(const PeerLimits& limits, affresco::ScreenImages* images = nullptr)
	    : session(channel, versions, "studio", {}, images, limits) {
		affresco::setLogSink([this](affresco::LogLevel, std::string_view message) { logged.emplace_back(message); });
		channel.sent.clear(); // hello
	}
	/// @brief Restores the default log.
	~LimitedPeer() { affresco::setLogSink(nullptr); }
	LimitedPeer(const LimitedPeer&) = delete;
	LimitedPeer& operator=(const LimitedPeer&) = delete;

	/// @brief Sends a message and returns its answer: the last message the peer got (an applied change is first
	/// announced as "committed", to every peer).
	/// @param line The message.
	/// @return The answer.
	json answer(const std::string& line) {
		channel.sent.clear();
		channel.receive(line);
		REQUIRE_FALSE(channel.sent.empty());
		return channel.sent.back();
	}

	affresco::format::Rules rules;                                                  ///< The screen format.
	affresco::vcs::Repository versions{rules, affresco::test::permissionsScreen()}; ///< The screen.
	affresco::test::MemoryChannel channel;                                          ///< What the peer receives.
	affresco::channel::Session session;                                             ///< The peer's session.
	std::vector<std::string> logged;                                                ///< Every log message.
};

/// @brief Pictures that cost nothing: counts how many were drawn.
class CountingImages final : public affresco::ScreenImages {
public:
	[[nodiscard]] std::optional<std::string> render(const affresco::ImageRequest& /*request*/,
	                                                affresco::Image& image) override {
		++drawn;
		image = {"png", 1, 1};
		return std::nullopt;
	}
	[[nodiscard]] bool rendering() const noexcept override { return false; }

	int drawn = 0; ///< Pictures drawn so far.
};

/// @brief An Executor whose tasks run on the test's thread, inside runUntil().
class QueueExecutor final : public affresco::Executor {
public:
	/// @brief Queues a task. Safe from any thread.
	/// @param task The task.
	void post(std::function<void()> task) override {
		{
			const std::scoped_lock lock(mutex_);
			tasks_.push_back(std::move(task));
		}
		ready_.notify_one();
	}

	/// @brief Runs queued tasks until @p done says so, or five seconds pass.
	/// @param done Checked before each task.
	/// @return Whether @p done became true.
	bool runUntil(const std::function<bool()>& done) {
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		while(!done()) {
			std::unique_lock lock(mutex_);
			if(!ready_.wait_until(lock, deadline, [this] { return !tasks_.empty(); })) return done();
			const std::function<void()> task = std::move(tasks_.front());
			tasks_.pop_front();
			lock.unlock();
			task();
		}
		return true;
	}

private:
	std::mutex mutex_;                        ///< Guards tasks_.
	std::condition_variable ready_;           ///< Notified when a task is queued.
	std::deque<std::function<void()>> tasks_; ///< Queued, oldest first.
};

/// @brief Log messages from any thread (a server logs on its own).
class SharedLog {
public:
	/// @brief Catches the log.
	SharedLog() {
		affresco::setLogSink([this](affresco::LogLevel, std::string_view message) {
			const std::scoped_lock lock(mutex_);
			messages_.emplace_back(message);
		});
	}
	/// @brief Restores the default log.
	~SharedLog() { affresco::setLogSink(nullptr); }
	SharedLog(const SharedLog&) = delete;
	SharedLog& operator=(const SharedLog&) = delete;

	/// @brief Every message so far.
	/// @return A copy.
	[[nodiscard]] std::vector<std::string> messages() const {
		const std::scoped_lock lock(mutex_);
		return messages_;
	}

private:
	mutable std::mutex mutex_;          ///< Guards messages_.
	std::vector<std::string> messages_; ///< Oldest first.
};

/// @brief A temporary folder with a short name (socket paths are limited to about 100 bytes), removed at the end.
struct TemporaryFolder {
	/// @brief Creates the folder.
	TemporaryFolder() { std::filesystem::create_directories(path); }
	/// @brief Removes it and what is in it.
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;

	std::filesystem::path path = std::filesystem::temp_directory_path() /
	                             ("aup-" + std::to_string(std::random_device{}())); ///< The folder.
};

} // namespace

TEST_CASE("channel: a TokenBucket allows a burst, then refills at its rate up to the burst") {
	const TokenBucket::Clock::time_point start{};
	TokenBucket bucket(2.0, 3.0, start); // 2 a second, 3 at once
	for(int i = 0; i < 3; ++i) CHECK(bucket.take(start));
	CHECK_FALSE(bucket.take(start));
	CHECK_FALSE(bucket.take(start + 400ms)); // 0.8 tokens
	CHECK(bucket.take(start + 500ms));       // 1
	CHECK_FALSE(bucket.take(start + 500ms));
	CHECK_FALSE(bucket.take(start + 100ms)); // a time before the last adds nothing
	// A long quiet time refills it to the burst, not beyond
	for(int i = 0; i < 3; ++i) CHECK(bucket.take(start + 1h));
	CHECK_FALSE(bucket.take(start + 1h));
	// A burst under 1 still lets one event through
	TokenBucket single(1.0, 0.0, start);
	CHECK(single.take(start));
	CHECK_FALSE(single.take(start));
}

TEST_CASE("channel: a Session refuses a message larger than its limit without reading it") {
	PeerLimits limits;
	limits.maxMessageBytes = 64;
	LimitedPeer peer(limits);

	const std::string large = R"({"type": "getHistory", "id": 1, "padding": ")" + std::string(64, 'x') + "\"}";
	CHECK(peer.answer(large) == json{{"type", "error"},
	                                 {"message", "message too large: " + std::to_string(large.size()) +
	                                                 " bytes, at most 64"}}); // no id: it wasn't read
	CHECK(peer.answer(R"({"type": "getHistory", "id": 2})")["type"] == "history");
	REQUIRE(peer.logged.size() == 1);
	CHECK(peer.logged[0] == "AffrescoUI: peer 'studio' is over its limits: message too large: " +
	                            std::to_string(large.size()) + " bytes, at most 64");
}

TEST_CASE("channel: a Session refuses messages over its rate, and logs it once") {
	PeerLimits limits;
	limits.messagesPerSecond = 0.001; // nothing comes back during the test
	limits.messageBurst = 3;
	LimitedPeer peer(limits);

	for(int id = 1; id <= 3; ++id)
		CHECK(peer.answer(json{{"type", "getHistory"}, {"id", id}}.dump())["type"] == "history");
	for(int id = 4; id <= 6; ++id)
		CHECK(peer.answer(json{{"type", "getHistory"}, {"id", id}}.dump()) ==
		      json{{"type", "error"}, {"message", "too many messages: at most 0.001 a second"}});
	CHECK(peer.logged ==
	      std::vector<std::string>{"AffrescoUI: peer 'studio' is over its limits: too many messages: at most 0.001 a second"});
}

TEST_CASE("channel: a Session refuses changes over their own rate, and takes them again once it refills") {
	PeerLimits limits;
	limits.revisionsPerMinute = 600; // one every 100 ms
	limits.revisionBurst = 2;
	LimitedPeer peer(limits);
	const auto change = [&](int id) {
		return peer.answer(json{{"type", "revision"}, {"id", id}, {"set", {{"/state/count", id}}}}.dump());
	};

	CHECK(change(1)["type"] == "applied");
	CHECK(change(2)["type"] == "applied");
	CHECK(change(3) ==
	      json{{"type", "rejected"}, {"error", "too many changes: at most 600 a minute"}, {"revision", 2}, {"id", 3}});
	CHECK(peer.answer(R"({"type": "undo", "id": 4})")["type"] == "rejected"); // undo is a change too
	CHECK(peer.answer(R"({"type": "getHistory", "id": 5})")["type"] == "history"); // reads aren't
	CHECK(peer.versions.state()["count"] == 2);

	std::this_thread::sleep_for(150ms);
	CHECK(change(6)["type"] == "applied");
	CHECK(peer.versions.state()["count"] == 6);
}

TEST_CASE("channel: a Session refuses pictures over the rate of changes, which they cost as much as") {
	PeerLimits limits;
	limits.revisionsPerMinute = 600; // one every 100 ms
	limits.revisionBurst = 2;
	CountingImages images;
	LimitedPeer peer(limits, &images);
	const auto picture = [&](int id) { return peer.answer(json{{"type", "render"}, {"id", id}}.dump()); };

	CHECK(picture(1)["type"] == "image");
	CHECK(picture(2)["type"] == "image");
	CHECK(picture(3) ==
	      json{{"type", "rejected"}, {"error", "too many pictures: at most 600 a minute"}, {"revision", 0}, {"id", 3}});
	CHECK(images.drawn == 2); // refused before drawing
	CHECK(peer.answer(json{{"type", "revision"}, {"id", 4}, {"set", {{"/state/count", 4}}}}.dump())["type"] ==
	      "applied"); // a rate of their own: changes still go through
	CHECK(peer.logged == std::vector<std::string>{
	                         "AffrescoUI: peer 'studio' is over its limits: too many pictures: at most 600 a minute"});

	std::this_thread::sleep_for(150ms);
	CHECK(picture(5)["type"] == "image");
	CHECK(images.drawn == 3);
}

TEST_CASE("channel: a SocketServer refuses and logs a connection its peer check refuses") {
	QueueExecutor executor;
	const TemporaryFolder folder;
	const SharedLog log;
	std::vector<std::unique_ptr<affresco::Channel>> accepted;
	std::atomic<bool> refuse = true; // read by the server's thread
	const affresco::channel::SocketServer server(
	    executor, folder.path / "s", [&](std::unique_ptr<affresco::Channel> c) { accepted.push_back(std::move(c)); },
	    [&](affresco::channel::SocketHandle) {
		    if(refuse) return false;
		    throw std::runtime_error("the check failed"); // refuses too
	    });

	for(int attempt = 0; attempt < 2; ++attempt) {
		const auto client = affresco::channel::SocketChannel::connect(executor, folder.path / "s");
		bool closed = false;
		client->onClosed.connect([&] { closed = true; });
		CHECK(executor.runUntil([&] { return closed; }));
		CHECK_FALSE(client->isOpen());
		refuse = false;
	}
	CHECK(accepted.empty());
	const std::vector<std::string> messages = log.messages();
	CHECK(std::ranges::count(messages, "AffrescoUI: refused a connection to " + (folder.path / "s").string() +
	                                       ": the program on the other end runs as another user") == 2);
}

TEST_CASE("channel: a SocketServer accepts a program of this user by default") {
	QueueExecutor executor;
	const TemporaryFolder folder;
	std::vector<std::unique_ptr<affresco::Channel>> accepted;
	const affresco::channel::SocketServer server(
	    executor, folder.path / "s", [&](std::unique_ptr<affresco::Channel> c) { accepted.push_back(std::move(c)); });
	const auto client = affresco::channel::SocketChannel::connect(executor, folder.path / "s");
	CHECK(executor.runUntil([&] { return accepted.size() == 1; }));
}

#ifndef _WIN32
TEST_CASE("channel: peerIsThisUser asks the system who is on the other end") {
	std::array<int, 2> ends = {-1, -1};
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, ends.data()) == 0);
	CHECK(affresco::channel::peerIsThisUser(ends[0]));
	CHECK(affresco::channel::peerIsThisUser(ends[1]));
	close(ends[0]);
	close(ends[1]);
	CHECK_FALSE(affresco::channel::peerIsThisUser(ends[0])); // closed: the system can't say
}

// Before, send() wrote on the main thread, and a peer that stopped reading blocked the app once the socket's buffer
// was full
TEST_CASE("channel: a SocketChannel whose peer stops reading is closed once its queue is full, without blocking") {
	std::array<int, 2> ends = {-1, -1};
	REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, ends.data()) == 0);
	QueueExecutor executor;
	const SharedLog log;
	affresco::channel::SocketChannel channel(executor, ends[0], std::size_t{1} << 20);
	bool closed = false;
	channel.onClosed.connect([&] { closed = true; });

	// The other end never reads: the socket's buffer fills, then the queue
	const std::string block(std::size_t{64} * 1024, 'x');
	const auto start = std::chrono::steady_clock::now();
	std::size_t sent = 0;
	for(; channel.isOpen() && sent < (std::size_t{64} << 20); sent += block.size()) channel.send(block);
	CHECK(std::chrono::steady_clock::now() - start < 5s);
	CHECK_FALSE(channel.isOpen());
	CHECK(sent < (std::size_t{64} << 20));
	CHECK(executor.runUntil([&] { return closed; }));
	const std::vector<std::string> messages = log.messages();
	CHECK(std::ranges::count(messages, "AffrescoUI: a peer fell more than 1 MiB behind in reading: closing its channel") ==
	      1);
	channel.send("ignored"); // closed: neither queued nor written
	close(ends[1]);
}
#endif

TEST_CASE("channel: a SocketChannel sends what is queued before it is destroyed") {
	QueueExecutor executor;
	const TemporaryFolder folder;
	std::vector<std::unique_ptr<affresco::Channel>> accepted;
	std::vector<std::string> received;
	const affresco::channel::SocketServer server(executor, folder.path / "s",
	                                             [&](std::unique_ptr<affresco::Channel> c) {
		                                             c->onMessage.connect(
		                                                 [&](const std::string& m) { received.push_back(m); });
		                                             accepted.push_back(std::move(c));
	                                             });
	{
		const auto client = affresco::channel::SocketChannel::connect(executor, folder.path / "s");
		for(const char* message : {"one", "two", "three"}) client->send(message);
	}
	CHECK(executor.runUntil([&] { return received.size() == 3; }));
	CHECK(received == std::vector<std::string>{"one", "two", "three"});
}
