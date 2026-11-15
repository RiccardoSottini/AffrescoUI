/// @file
/// @brief Unit tests of the editor server's wire (modules/net/src/nrepl/EditorConnection.hpp), over real loopback
/// TCP with a peer that records what it receives: framing, the limits that close a connection, the connection cap,
/// a client that never reads, and shutting down with connections open.

#include "NetServer.hpp"
#include "nrepl/EditorConnection.hpp"

#include <affresco/net/EditorServer.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace affresco;
using namespace affresco::net;
using namespace std::chrono_literals;

namespace {

/// @brief What one connection's peer saw.
struct Record {
	std::vector<bencode::Value> messages; ///< Every message received, in order.
	int closed = 0;                       ///< How many times closed() was called.
	EditorLink* link = nullptr;           ///< Where the peer's answers go.
};

/// @brief A peer that records what it receives.
class RecordingPeer final : public EditorPeer {
public:
	/// @brief Records into a record. @param record It; outlives the peer.
	explicit RecordingPeer(Record& record) : record_(record) {}
	/// @brief Keeps a message. @param message It.
	void receive(const bencode::Value& message) override { record_.messages.push_back(message); }
	/// @brief Counts the close.
	void closed() override { ++record_.closed; }

private:
	Record& record_; ///< See the constructor.
};

/// @brief A listener on 127.0.0.1 whose connections record what they receive, and the main thread that runs them.
struct Wire {
	/// @brief Listens. @param givenOptions The limits.
	explicit Wire(EditorServerOptions givenOptions = {}) : options(std::move(givenOptions)) {
		listener = std::make_unique<EditorListener>(executor, options, [this](EditorLink& link) {
			Record& record = records.emplace_back();
			record.link = &link;
			return std::make_unique<RecordingPeer>(record);
		});
	}

	/// @brief Connects a client. @return It.
	[[nodiscard]] std::unique_ptr<tcp::Connection> connect() const {
		auto client = tcp::Connection::open("127.0.0.1", listener->port());
		REQUIRE(client != nullptr);
		return client;
	}

	test::ThreadExecutor executor;            ///< The main thread's tasks.
	EditorServerOptions options;              ///< The limits.
	std::deque<Record> records;               ///< One per connection, in the order they began.
	std::unique_ptr<EditorListener> listener; ///< The wire under test (destroyed before the records).
};

/// @brief Sends bytes from a client. @param client It. @param bytes They.
void send(tcp::Connection& client, std::string_view bytes) { REQUIRE(client.send(bytes, 5s)); }

/// @brief Reads from a client until the server closes it. @param client It. @return Whether it closed within 5 s.
bool closedByServer(tcp::Connection& client) {
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	std::string ignored;
	while(std::chrono::steady_clock::now() < deadline) {
		const tcp::Received got = client.receive(ignored, 100ms);
		if(got == tcp::Received::Closed || got == tcp::Received::Failed) return true;
		ignored.clear();
	}
	return false;
}

} // namespace

TEST_CASE("nrepl_wire: a message sent one byte at a time arrives whole") {
	Wire wire;
	auto client = wire.connect();
	const std::string message = "d2:id1:12:op8:describee";
	for(const char byte : message) {
		send(*client, std::string_view(&byte, 1));
		std::this_thread::sleep_for(1ms);
	}
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && !wire.records[0].messages.empty(); }));
	CHECK(wire.records[0].messages.size() == 1);
	CHECK(wire.records[0].messages[0].find("op")->string() == "describe");
}

TEST_CASE("nrepl_wire: two messages in one write arrive as two") {
	Wire wire;
	auto client = wire.connect();
	send(*client, "d2:op5:clonee"
	              "d2:op8:describee");
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && wire.records[0].messages.size() == 2; }));
	CHECK(wire.records[0].messages[0].find("op")->string() == "clone");
	CHECK(wire.records[0].messages[1].find("op")->string() == "describe");
	CHECK(wire.records[0].closed == 0);
}

TEST_CASE("nrepl_wire: an answer reaches the client") {
	Wire wire;
	auto client = wire.connect();
	send(*client, "d2:op5:clonee");
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && !wire.records[0].messages.empty(); }));
	wire.records[0].link->send(
	    bencode::Value(bencode::Value::Dictionary{{"status", bencode::Value(std::string("done"))}}));
	std::string received;
	while(received.size() < 16 && client->receive(received, 5s) == tcp::Received::Data) {
	}
	CHECK(received == "d6:status4:donee");
}

TEST_CASE("nrepl_wire: a message over the limit closes the connection") {
	EditorServerOptions options;
	options.maxMessageBytes = 64;
	Wire wire(options);
	auto client = wire.connect();
	send(*client, "d4:code100:");
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && wire.records[0].closed == 1; }));
	CHECK(closedByServer(*client)); // the socket goes once the main thread has destroyed the connection
	CHECK(wire.records[0].messages.empty());
}

TEST_CASE("nrepl_wire: a first byte that isn't d, as in an HTTP request, closes the connection") {
	Wire wire;
	auto client = wire.connect();
	send(*client, "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && wire.records[0].closed == 1; }));
	CHECK(closedByServer(*client)); // the socket goes once the main thread has destroyed the connection
	CHECK(wire.records[0].messages.empty());
}

TEST_CASE("nrepl_wire: a malformed message closes the connection, after the messages before it") {
	Wire wire;
	auto client = wire.connect();
	send(*client, "d2:op5:clonee"
	              "d1:ai1e1:ai2ee");
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && wire.records[0].closed == 1; }));
	CHECK(closedByServer(*client)); // the socket goes once the main thread has destroyed the connection
	CHECK(wire.records[0].messages.size() == 1);
}

TEST_CASE("nrepl_wire: a flood over the rate closes the connection") {
	EditorServerOptions options;
	options.messagesPerSecond = 10;
	Wire wire(options);
	auto client = wire.connect();
	std::string flood;
	for(int index = 0; index < 100; ++index) flood += "d2:op8:describee";
	send(*client, flood);
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && wire.records[0].closed == 1; }));
	CHECK(closedByServer(*client)); // the socket goes once the main thread has destroyed the connection
	CHECK(wire.records[0].messages.size() <= 11);
}

TEST_CASE("nrepl_wire: a connection over maxConnections is refused") {
	Wire wire;
	std::vector<std::unique_ptr<tcp::Connection>> clients;
	clients.reserve(4);
	for(int index = 0; index < 4; ++index) clients.push_back(wire.connect());
	REQUIRE(wire.executor.runUntil([&] { return wire.records.size() == 4; }));
	auto fifth = wire.connect();
	CHECK(closedByServer(*fifth));
	wire.executor.runFor(100ms);
	CHECK(wire.records.size() == 4);
	CHECK(wire.listener->connections() == 4);
	clients.front().reset(); // one goes, and its place is free again
	REQUIRE(wire.executor.runUntil([&] { return wire.listener->connections() == 3; }));
	const auto another = wire.connect();
	REQUIRE(wire.executor.runUntil([&] { return wire.records.size() == 5; }));
}

TEST_CASE("nrepl_wire: a client that never reads is dropped once the queue is full, without the main thread waiting") {
	EditorServerOptions options;
	options.maxQueuedBytes = std::size_t{1} << 20;
	Wire wire(options);
	auto client = wire.connect();
	send(*client, "d2:op5:clonee");
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && !wire.records[0].messages.empty(); }));
	const bencode::Value big(std::string(std::size_t{256} << 10, 'x'));
	const auto started = std::chrono::steady_clock::now();
	for(int index = 0; index < 256; ++index) wire.records[0].link->send(big); // 64 MiB, far over any socket buffer
	CHECK(std::chrono::steady_clock::now() - started < 2s);
	CHECK(wire.executor.runUntil([&] { return wire.records[0].closed == 1; }));
}

TEST_CASE("nrepl_wire: the reader stops reading while the main thread holds too much of what it read, then goes on") {
	EditorServerOptions options;
	options.maxQueuedBytes = std::size_t{1} << 20;
	options.messagesPerSecond = 1e6; // only the bytes hold it back here
	Wire wire(options);
	auto client = wire.connect();
	REQUIRE(wire.executor.runUntil([&] { return wire.records.size() == 1; }));
	constexpr std::size_t count = 64;
	const std::string message = bencode::encode(
	    bencode::Value(bencode::Value::Dictionary{{"op", bencode::Value(std::string("eval"))},
		                                          {"code", bencode::Value(std::string(std::size_t{256} << 10, 'x'))}}));
	std::thread sender([&] { // 16 MiB: it waits for room while the server holds back
		for(std::size_t index = 0; index < count; ++index)
			if(!client->send(message, 20s)) return;
	});
	std::this_thread::sleep_for(1s); // the main thread is busy: nothing it was posted runs
	REQUIRE(wire.executor.runUntil([&] { return !wire.records[0].messages.empty(); }));
	CHECK(wire.records[0].messages.size() <= 8U); // about 1 MiB of them waited, not all 16
	CHECK(wire.executor.runUntil([&] { return wire.records[0].messages.size() == count; }, 20s));
	sender.join();
	CHECK(wire.records[0].closed == 0);
}

TEST_CASE("nrepl_wire: close() sends what was queued, then ends the connection") {
	Wire wire;
	auto client = wire.connect();
	send(*client, "d2:op5:clonee");
	REQUIRE(wire.executor.runUntil([&] { return !wire.records.empty() && !wire.records[0].messages.empty(); }));
	wire.records[0].link->send(bencode::Value(std::int64_t{7}));
	wire.records[0].link->close("the test is done");
	REQUIRE(wire.executor.runUntil([&] { return wire.records[0].closed == 1; }));
	CHECK(wire.listener->connections() == 0);
	std::string received; // what was sent before the close is still there to read
	while(client->receive(received, 5s) == tcp::Received::Data) {
	}
	CHECK(received == "i7e");
}

TEST_CASE("nrepl_wire: destroying the listener with connections open returns promptly, and tells each peer") {
	Wire wire;
	std::vector<std::unique_ptr<tcp::Connection>> clients;
	clients.reserve(3);
	for(int index = 0; index < 3; ++index) clients.push_back(wire.connect());
	send(*clients[0], "d2:op4:eval"); // a message cut in the middle
	REQUIRE(wire.executor.runUntil([&] { return wire.records.size() == 3; }));
	const auto started = std::chrono::steady_clock::now();
	wire.listener.reset();
	CHECK(std::chrono::steady_clock::now() - started < 1s);
	for(const Record& record : wire.records) CHECK(record.closed == 1);
	for(auto& client : clients) CHECK(closedByServer(*client));
}

TEST_CASE("nrepl_wire: an EditorServer listens on a port the system picked, and shuts down promptly") {
	test::ThreadExecutor executor;
	std::vector<std::unique_ptr<tcp::Connection>> clients;
	std::optional<EditorServer> server(std::in_place, executor, EditorHost{});
	CHECK(server->port() != 0);
	clients.reserve(2);
	for(int index = 0; index < 2; ++index) clients.push_back(tcp::Connection::open("127.0.0.1", server->port()));
	executor.runFor(100ms);
	const auto started = std::chrono::steady_clock::now();
	server.reset();
	CHECK(std::chrono::steady_clock::now() - started < 1s);
	for(auto& client : clients) CHECK(closedByServer(*client));
}
