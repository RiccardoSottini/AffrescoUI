/// @file
/// @brief Unit tests of remote peers (modules/net, RFC 0003): the token on the upgrade or in a first message, the
/// Origin allowlist, the caps, the loopback default, and connectPeer() against a PeerServer.

#include "NetServer.hpp"
#include "WebSocketFrames.hpp"

#include <affresco/net/RemotePeers.hpp>

#include <doctest/doctest.h>

#include <array>
#include <stdexcept>

using namespace affresco;
using namespace affresco::net;
using namespace std::chrono_literals;

namespace {

/// @brief Accepts the token "good" as the author "agent/test" (an obviously fake token).
class FakeVerifier final : public TokenVerifier {
public:
	/// @brief Checks a token.
	/// @param token The token.
	/// @return "agent/test" for "good", else nullopt.
	std::optional<std::string> verify(std::string_view token) override {
		if(token == "good") return "agent/test";
		return std::nullopt;
	}
};

/// @brief A raw client's upgrade: the connection and what the server answered so far.
struct RawPeer {
	std::unique_ptr<tcp::Connection> connection; ///< The socket.
	std::string answer;                          ///< The response head, and any bytes after it.
};

/// @brief Opens a connection to the server and sends an upgrade request, then reads the response head.
/// @param port The server's port.
/// @param headers Extra header lines, each ending in "\r\n".
/// @return The connection and the answer.
RawPeer upgrade(std::uint16_t port, const std::string& headers = {}) {
	RawPeer peer{tcp::Connection::open("127.0.0.1", port), {}};
	REQUIRE(peer.connection);
	REQUIRE(peer.connection->send("GET /live HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
	                              "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" +
	                                  headers + "\r\n",
	                              1s));
	while(peer.answer.find("\r\n\r\n") == std::string::npos)
		if(peer.connection->receive(peer.answer, 5s) != tcp::Received::Data) break;
	return peer;
}

/// @brief Sends a text message as a client does (masked).
/// @param peer The peer.
/// @param message The message.
void sendText(RawPeer& peer, std::string_view message) {
	CHECK(peer.connection->send(encodeFrame(Opcode::Text, message, std::array<std::uint8_t, 4>{1, 2, 3, 4}), 1s));
}

/// @brief Reads the next frame the server sends.
/// @param peer The peer (its answer's bytes after the head are read first).
/// @return The frame, or nullopt if none came.
std::optional<FrameEvent> nextFrame(RawPeer& peer) {
	FrameReader reader(false, std::size_t{1} << 20);
	const auto end = peer.answer.find("\r\n\r\n");
	if(end != std::string::npos) peer.answer.erase(0, end + 4);
	reader.feed(peer.answer);
	peer.answer.clear();
	for(;;) {
		if(auto event = reader.next()) return event;
		std::string bytes;
		if(peer.connection->receive(bytes, 5s) != tcp::Received::Data) return std::nullopt;
		reader.feed(bytes);
	}
}

/// @brief A server on a free loopback port that keeps the peers it accepts.
struct Fixture {
	test::ThreadExecutor executor;               ///< The main thread (first: outlives the rest).
	std::vector<std::unique_ptr<Channel>> peers; ///< Accepted peers.
	std::vector<std::string> authors;            ///< Their authors.
	std::unique_ptr<PeerServer> server;          ///< The server.

	/// @brief Starts the server.
	/// @param options Its options.
	explicit Fixture(PeerServerOptions options = {}) {
		server = std::make_unique<PeerServer>(
		    executor, "127.0.0.1:0", std::make_shared<FakeVerifier>(),
		    [this](std::unique_ptr<Channel> channel, std::string author) {
			    peers.push_back(std::move(channel));
			    authors.push_back(std::move(author));
		    },
		    std::move(options));
	}
};

} // namespace

TEST_CASE("remote peers: a bearer token is accepted, and messages go both ways") {
	Fixture fixture;
	RawPeer peer = upgrade(fixture.server->port(), "Authorization: Bearer good\r\n");
	CHECK(peer.answer.starts_with("HTTP/1.1 101"));
	REQUIRE(fixture.executor.runUntil([&] { return !fixture.peers.empty(); }));
	CHECK(fixture.authors.front() == "agent/test");
	std::string received;
	fixture.peers.front()->onMessage.connect([&](const std::string& message) { received = message; });
	sendText(peer, R"({"type":"hello"})");
	CHECK(fixture.executor.runUntil([&] { return !received.empty(); }));
	CHECK(received == R"({"type":"hello"})");
	fixture.peers.front()->send(R"({"type":"welcome"})");
	const auto frame = nextFrame(peer);
	REQUIRE(frame);
	CHECK(frame->kind == FrameEvent::Kind::Message);
	CHECK(frame->data == R"({"type":"welcome"})");
}

TEST_CASE("remote peers: bad tokens and origins are refused with a reason") {
	PeerServerOptions options;
	options.allowedOrigins = {"https://studio.example.com"};
	Fixture fixture(options);
	CHECK(upgrade(fixture.server->port(), "Authorization: Bearer bad\r\n").answer.starts_with("HTTP/1.1 401"));
	CHECK(upgrade(fixture.server->port(), "Origin: https://evil.example.com\r\nAuthorization: Bearer good\r\n")
	          .answer.starts_with("HTTP/1.1 403"));
	CHECK(upgrade(fixture.server->port(), "Origin: https://studio.example.com\r\nAuthorization: Bearer good\r\n")
	          .answer.starts_with("HTTP/1.1 101"));
	CHECK(upgrade(fixture.server->port(), "Sec-WebSocket-Version: 13\r\n").answer.starts_with("HTTP/1.1 400"));
	CHECK(fixture.executor.runUntil([&] { return fixture.peers.size() == 1; }));
}

TEST_CASE("remote peers: the token may come as the first message, in time") {
	PeerServerOptions options;
	options.authTimeout = 300ms;
	Fixture fixture(options);
	RawPeer first = upgrade(fixture.server->port());
	CHECK(first.answer.starts_with("HTTP/1.1 101"));
	sendText(first, R"({"type": "auth", "token": "good"})");
	REQUIRE(fixture.executor.runUntil([&] { return !fixture.peers.empty(); }));
	CHECK(fixture.authors.front() == "agent/test");

	RawPeer wrong = upgrade(fixture.server->port());
	sendText(wrong, R"({"type": "auth", "token": "bad"})");
	auto frame = nextFrame(wrong);
	REQUIRE(frame);
	CHECK(frame->kind == FrameEvent::Kind::Close);

	RawPeer silent = upgrade(fixture.server->port());
	CHECK(silent.answer.starts_with("HTTP/1.1 101"));
	frame = nextFrame(silent); // nothing sent: closed once the time is up
	REQUIRE(frame);
	CHECK(frame->kind == FrameEvent::Kind::Close);
	fixture.executor.runFor(50ms);
	CHECK(fixture.peers.size() == 1);
}

TEST_CASE("remote peers: connections are capped, and the server stays on the loopback") {
	PeerServerOptions options;
	options.maxPeers = 1;
	Fixture fixture(options);
	RawPeer held = upgrade(fixture.server->port(), "Authorization: Bearer good\r\n");
	CHECK(held.answer.starts_with("HTTP/1.1 101"));
	CHECK(upgrade(fixture.server->port(), "Authorization: Bearer good\r\n").answer.starts_with("HTTP/1.1 503"));

	PeerServerOptions burst;
	burst.connectionBurst = 1;
	burst.connectionsPerSecond = 0.01;
	Fixture limited(burst);
	CHECK(upgrade(limited.server->port(), "Authorization: Bearer good\r\n").answer.starts_with("HTTP/1.1 101"));
	CHECK(upgrade(limited.server->port(), "Authorization: Bearer good\r\n").answer.starts_with("HTTP/1.1 429"));

	test::ThreadExecutor executor;
	const auto verifier = std::make_shared<FakeVerifier>();
	CHECK_THROWS_AS(PeerServer(executor, "0.0.0.0:0", verifier, {}), std::runtime_error);
	CHECK_THROWS_AS(PeerServer(executor, "127.0.0.1", verifier, {}), std::runtime_error);
	CHECK_THROWS_AS(PeerServer(executor, "127.0.0.1:0", nullptr, {}), std::runtime_error);
}

TEST_CASE("remote peers: connectPeer() reaches a server with its token") {
	Fixture fixture;
	CHECK_FALSE(connectPeer(fixture.executor, "http://127.0.0.1/", "good"));
	CHECK_FALSE(connectPeer(fixture.executor, "ws://example.com/live", "good"));
	CHECK_FALSE(connectPeer(fixture.executor, "wss://example.com/live", "two words"));
	CHECK_FALSE(connectPeer(fixture.executor, "wss://example.com/live", ""));
	auto client =
	    connectPeer(fixture.executor, "ws://127.0.0.1:" + std::to_string(fixture.server->port()) + "/live", "good");
	REQUIRE(client);
	client->send(R"({"type":"hello"})");
	REQUIRE(fixture.executor.runUntil([&] { return !fixture.peers.empty(); }));
	CHECK(fixture.authors.front() == "agent/test");
	std::string received;
	fixture.peers.front()->onMessage.connect([&](const std::string& message) { received = message; });
	std::string answer;
	client->onMessage.connect([&](const std::string& message) { answer = message; });
	fixture.peers.front()->send(R"({"type":"welcome"})");
	CHECK(fixture.executor.runUntil([&] { return !received.empty() && !answer.empty(); }));
	CHECK(received == R"({"type":"hello"})");
	CHECK(answer == R"({"type":"welcome"})");
}
