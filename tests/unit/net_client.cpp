/// @file
/// @brief Unit tests of net::Client against a local server on the loopback: responses, failures, redirects, retries,
/// limits, server-sent events, cancellation and WebSocket connections.

#include "NetServer.hpp"
#include "websocket/WebSocketFrames.hpp"

#include <affresco/net/Client.hpp>

#include <doctest/doctest.h>

#include <atomic>
#include <optional>
#include <thread>

using namespace affresco;
using namespace affresco::net;
using affresco::test::httpResponse;
using affresco::test::LocalServer;
using affresco::test::readRequest;
using affresco::test::ThreadExecutor;

namespace {

/// @brief What one request ended with.
struct Outcome {
	std::optional<network::Response> response;       ///< Its response.
	std::optional<network::Failure> failure;         ///< Or its failure.
	std::vector<network::StreamEvent> events;        ///< The events of a stream.
	[[nodiscard]] bool ended() const noexcept { return response || failure; } ///< Whether it ended.
};

/// @brief Handlers that record into an outcome.
/// @param outcome The outcome.
network::Handlers recordInto(Outcome& outcome) {
	return {[&outcome](network::Response response) { outcome.response = std::move(response); },
	        [&outcome](network::Failure failure) { outcome.failure = std::move(failure); },
	        [&outcome](network::StreamEvent event) { outcome.events.push_back(std::move(event)); }};
}

/// @brief A client allowed to reach one server.
/// @param executor Its executor.
/// @param server The server.
std::unique_ptr<Client> clientFor(ThreadExecutor& executor, const LocalServer& server) {
	auto client = std::make_unique<Client>(executor, ClientOptions{.proxy = std::string{}});
	CHECK_FALSE(client->allow(server.origin()));
	return client;
}

} // namespace

TEST_CASE("net client: a request gets its response, with the headers and the body sent") {
	std::string seen;
	LocalServer server([&](tcp::Connection& connection) {
		auto request = readRequest(connection);
		seen = request.head + request.body;
		CHECK(connection.send(httpResponse("201 Created", "{\"id\":4}", "Content-Type: application/json\r\n"),
		                      std::chrono::seconds{5}));
	});
	ThreadExecutor executor;
	auto client = clientFor(executor, server);
	Outcome outcome;
	network::Request request{.method = "POST", .url = server.origin() + "/items",
	                         .headers = {{"Content-Type", "application/json"}}, .body = "{\"name\":\"a\"}"};
	auto transfer = client->start(std::move(request), recordInto(outcome));
	REQUIRE(executor.runUntil([&] { return outcome.ended(); }));
	REQUIRE(outcome.response);
	CHECK(outcome.response->status == 201);
	CHECK(outcome.response->body == "{\"id\":4}");
	CHECK(seen.starts_with("POST /items HTTP/1.1\r\n"));
	CHECK(seen.find("{\"name\":\"a\"}") != std::string::npos);
	CHECK(seen.find("Content-Type: application/json") != std::string::npos);
}

TEST_CASE("net client: origins not allowed, bad methods and transport headers are refused") {
	ThreadExecutor executor;
	Client client(executor);
	Outcome notAllowed, badMethod, badHeader;
	auto first = client.start({.url = "https://example.com/"}, recordInto(notAllowed));
	CHECK_FALSE(client.allow("https://example.com"));
	auto second = client.start({.method = "TRACE", .url = "https://example.com/"}, recordInto(badMethod));
	auto third = client.start({.url = "https://example.com/", .headers = {{"Host", "other"}}}, recordInto(badHeader));
	REQUIRE(executor.runUntil([&] { return notAllowed.ended() && badMethod.ended() && badHeader.ended(); }));
	CHECK(notAllowed.failure->code == "refused");
	CHECK(badMethod.failure->code == "refused");
	CHECK(badHeader.failure->code == "refused");
}

TEST_CASE("net client: an error status fails with its body; a same-origin redirect is followed") {
	LocalServer server([](tcp::Connection& connection) {
		auto request = readRequest(connection);
		std::string answer = httpResponse("404 Not Found", "missing");
		if(request.head.starts_with("GET /old ")) answer = httpResponse("302 Found", "", "Location: /new\r\n");
		if(request.head.starts_with("GET /new ")) answer = httpResponse("200 OK", "moved");
		if(request.head.starts_with("GET /away ")) answer = httpResponse("302 Found", "", "Location: https://example.com/\r\n");
		CHECK(connection.send(answer, std::chrono::seconds{5}));
	});
	ThreadExecutor executor;
	auto client = clientFor(executor, server);
	Outcome missing, moved, away;
	auto first = client->start({.url = server.origin() + "/nothing"}, recordInto(missing));
	auto second = client->start({.url = server.origin() + "/old"}, recordInto(moved));
	auto third = client->start({.url = server.origin() + "/away"}, recordInto(away));
	REQUIRE(executor.runUntil([&] { return missing.ended() && moved.ended() && away.ended(); }));
	REQUIRE(missing.failure);
	CHECK(missing.failure->code == "status");
	CHECK(missing.failure->status == 404);
	CHECK(missing.failure->body == "missing");
	REQUIRE(moved.response);
	CHECK(moved.response->body == "moved");
	REQUIRE(away.failure); // to another origin: never followed
	CHECK(away.failure->code == "status");
	CHECK(away.failure->status == 302);
}

TEST_CASE("net client: a GET is retried after 503, and a body over its limit fails") {
	std::atomic<int> served{0};
	LocalServer server([&](tcp::Connection& connection) {
		auto request = readRequest(connection);
		const bool big = request.head.starts_with("GET /big ");
		const std::string answer = big ? httpResponse("200 OK", std::string(2048, 'x'))
		                               : (served++ == 0 ? httpResponse("503 Busy", "") : httpResponse("200 OK", "ok"));
		CHECK(connection.send(answer, std::chrono::seconds{5}));
	});
	ThreadExecutor executor;
	auto client = clientFor(executor, server);
	Outcome retried, big;
	auto first = client->start({.url = server.origin() + "/busy", .retries = 2,
	                            .retryAfter = std::chrono::milliseconds{10}},
	                           recordInto(retried));
	auto second = client->start({.url = server.origin() + "/big", .maxBodyBytes = 1024}, recordInto(big));
	REQUIRE(executor.runUntil([&] { return retried.ended() && big.ended(); }));
	REQUIRE(retried.response);
	CHECK(retried.response->body == "ok");
	REQUIRE(big.failure);
	CHECK(big.failure->code == "tooLarge");
}

TEST_CASE("net client: server-sent events arrive one by one, then the end") {
	LocalServer server([](tcp::Connection& connection) {
		static_cast<void>(readRequest(connection));
		const std::string head = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n";
		CHECK(connection.send(head + "data: one\n\n", std::chrono::seconds{5}));
		std::this_thread::sleep_for(std::chrono::milliseconds{20});
		CHECK(connection.send("event: tick\ndata: two\n\n", std::chrono::seconds{5}));
	});
	ThreadExecutor executor;
	auto client = clientFor(executor, server);
	Outcome outcome;
	auto transfer = client->start({.url = server.origin() + "/events", .stream = network::Stream::Events},
	                              recordInto(outcome));
	REQUIRE(executor.runUntil([&] { return outcome.ended(); }));
	REQUIRE(outcome.response);
	REQUIRE(outcome.events.size() == 2);
	CHECK(outcome.events[0].data == "one");
	CHECK(outcome.events[1].type == "tick");
}

TEST_CASE("net client: destroying a transfer cancels it, and a slow answer times out") {
	LocalServer server([](tcp::Connection& connection) {
		static_cast<void>(readRequest(connection));
		std::string ignored;
		static_cast<void>(connection.receive(ignored, std::chrono::milliseconds{1500}));
	});
	ThreadExecutor executor;
	auto client = clientFor(executor, server);
	Outcome cancelled, slow;
	auto transfer = client->start({.url = server.origin() + "/a"}, recordInto(cancelled));
	transfer.reset();
	auto timed = client->start({.url = server.origin() + "/b", .timeout = std::chrono::milliseconds{200}},
	                           recordInto(slow));
	REQUIRE(executor.runUntil([&] { return slow.ended(); }));
	CHECK(slow.failure->code == "timeout");
	executor.runFor(std::chrono::milliseconds{50});
	CHECK_FALSE(cancelled.ended());
}

TEST_CASE("net client: a WebSocket connection opens, echoes and closes") {
	LocalServer server([](tcp::Connection& connection) {
		auto request = readRequest(connection);
		auto block = parseHeaderBlock(request.head);
		const auto key = block ? findHeader(block->headers, "sec-websocket-key") : std::nullopt;
		if(!key) return; // the client sees the connection close, and the test fails
		CHECK(connection.send("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
		                      "Sec-WebSocket-Accept: " + acceptKey(*key) + "\r\n\r\n",
		                      std::chrono::seconds{5}));
		FrameReader reader(true, 1024);
		reader.feed(request.body);
		for(;;) {
			while(auto event = reader.next()) {
				if(event->kind == FrameEvent::Kind::Message)
					CHECK(connection.send(encodeFrame(Opcode::Text, "echo " + event->data), std::chrono::seconds{5}));
				if(event->kind == FrameEvent::Kind::Close || event->kind == FrameEvent::Kind::Error) return;
			}
			std::string bytes;
			if(connection.receive(bytes, std::chrono::seconds{5}) != tcp::Received::Data) return;
			reader.feed(bytes);
		}
	});
	ThreadExecutor executor;
	auto client = clientFor(executor, server);
	bool opened = false;
	std::vector<std::string> messages;
	std::optional<std::optional<network::Failure>> closed;
	std::unique_ptr<network::WebSocket> socket;
	socket = client->connect(server.origin("ws") + "/live", {},
	                         {[&] {
		                          opened = true;
		                          CHECK(socket->send("hi"));
	                          },
	                          [&](std::string message) { messages.push_back(std::move(message)); },
	                          [&](std::optional<network::Failure> failure) { closed = std::move(failure); }});
	REQUIRE(executor.runUntil([&] { return !messages.empty() || closed; }));
	CHECK(opened);
	REQUIRE(messages.size() == 1);
	CHECK(messages[0] == "echo hi");
	CHECK_FALSE(socket->send(std::string_view{"\xc3", 1})); // not UTF-8
	socket.reset(); // closes; no handler afterwards
	executor.runFor(std::chrono::milliseconds{50});
	CHECK_FALSE(closed);
}

TEST_CASE("net client: a WebSocket to a server that doesn't upgrade fails with its status") {
	LocalServer server([](tcp::Connection& connection) {
		static_cast<void>(readRequest(connection));
		CHECK(connection.send(httpResponse("403 Forbidden", "no"), std::chrono::seconds{5}));
	});
	ThreadExecutor executor;
	auto client = clientFor(executor, server);
	std::optional<std::optional<network::Failure>> closed;
	auto socket = client->connect(server.origin("ws") + "/", {},
	                              {{}, {}, [&](std::optional<network::Failure> failure) { closed = std::move(failure); }});
	REQUIRE(executor.runUntil([&] { return closed.has_value(); }));
	REQUIRE(*closed);
	CHECK((*closed)->code == "status");
	CHECK((*closed)->status == 403);
}
