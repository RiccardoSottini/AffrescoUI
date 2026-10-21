/// @file
/// @brief Unit tests of the network module's parsers: URLs and the origin allowlist, HTTP headers, server-sent events,
/// lines, and WebSocket frames.

#include "Http.hpp"
#include "Streams.hpp"
#include "WebSocketFrames.hpp"

#include <affresco/net/Client.hpp>
#include <affresco/net/Url.hpp>

#include <doctest/doctest.h>

#include <array>
#include <vector>

using namespace affresco;
using namespace affresco::net;

TEST_CASE("net: URLs are parsed, and only allowed origins are reached") {
	auto url = parseUrl("https://API.example.com/v1/items?q=1");
	REQUIRE(url);
	CHECK(url->host == "api.example.com");
	CHECK(url->port == 443);
	CHECK(url->target == "/v1/items?q=1");
	CHECK(url->origin() == "https://api.example.com");
	CHECK_FALSE(parseUrl("https://user@example.com/"));
	CHECK_FALSE(parseUrl("ftp://example.com/"));
	CHECK_FALSE(parseUrl("https://example.com:99999/"));
	CHECK(isLoopback("127.0.0.1"));
	CHECK(isLoopback("localhost"));
	CHECK_FALSE(isLoopback("example.com"));

	OriginAllowlist allowlist;
	CHECK_FALSE(allowlist.allow("https://api.example.com"));
	CHECK_FALSE(allowlist.allow("https://*.cdn.example.com"));
	CHECK(allowlist.allow("https://example.com/path"));
	CHECK_FALSE(allowlist.allow("http://example.com"));
	CHECK_FALSE(allowlist.check("https://api.example.com/x"));
	CHECK_FALSE(allowlist.check("wss://api.example.com/socket"));
	CHECK_FALSE(allowlist.check("https://eu.cdn.example.com/x"));
	CHECK(allowlist.check("https://cdn.example.com/x"));
	CHECK(allowlist.check("https://api.example.com:8443/x"));
	CHECK(allowlist.check("https://other.example.com/"));
	CHECK(allowlist.check("http://example.com/")); // plain http only to the loopback
	CHECK(encodeSegment("a b/c") == "a%20b%2Fc");
}

TEST_CASE("net: response headers are parsed and bounded") {
	CHECK(parseStatusLine("HTTP/1.1 204 No Content") == 204);
	CHECK_FALSE(parseStatusLine("HTTP/1.1 abc"));
	CHECK_FALSE(parseStatusLine("SSH-2.0"));
	auto header = parseHeaderLine("Content-Type:  text/plain ");
	REQUIRE(header);
	CHECK(header->first == "content-type");
	CHECK(header->second == "text/plain");
	CHECK_FALSE(parseHeaderLine("no colon"));
	CHECK(parseRetryAfter("3") == std::chrono::milliseconds{3000});
	CHECK_FALSE(parseRetryAfter("-1"));
	CHECK(hasToken("keep-alive, Upgrade", "upgrade"));
	CHECK_FALSE(hasToken("upgraded", "upgrade"));
	auto block = parseHeaderBlock("HTTP/1.1 101 Switching\r\nUpgrade: websocket\r\n\r\n");
	REQUIRE(block);
	CHECK(block->firstLine == "HTTP/1.1 101 Switching");
	CHECK(findHeader(block->headers, "upgrade") == "websocket");
}

TEST_CASE("net: request headers that the transport sets are refused") {
	CHECK_FALSE(checkHeaders({{"Accept", "application/json"}, {"X-Request-Id", "7"}}));
	CHECK(checkHeaders({{"Host", "example.com"}}));
	CHECK(checkHeaders({{"Content-Length", "3"}}));
	CHECK(checkHeaders({{"Sec-WebSocket-Key", "x"}}));
	CHECK(checkHeaders({{"Proxy-Authorization", "x"}}));
	CHECK(checkHeaders({{"X-Split", "a\r\nInjected: 1"}}));
	CHECK(checkHeaders({{"Bad Name", "x"}}));
	network::Headers many(101, {"X-A", "1"});
	CHECK(checkHeaders(many));
}

TEST_CASE("net: server-sent events are read across chunks, and bounded") {
	std::vector<network::StreamEvent> events;
	EventStreamParser parser(64, [&](network::StreamEvent event) { events.push_back(std::move(event)); });
	CHECK(parser.feed(": a comment\ndata: one\nda"));
	CHECK(parser.feed("ta: two\r"));
	CHECK(parser.feed("\n\r\nevent: update\nid: 7\ndata:{\"n\":1}\n\nretry: 5\n\n"));
	REQUIRE(events.size() == 2);
	CHECK(events[0].type == "message");
	CHECK(events[0].data == "one\ntwo");
	CHECK(events[1].type == "update");
	CHECK(events[1].id == "7");
	CHECK(events[1].data == "{\"n\":1}");
	CHECK_FALSE(parser.feed("data: " + std::string(100, 'x') + "\n"));
}

TEST_CASE("net: line streams deliver each line, and the last one at the end") {
	std::vector<std::string> lines;
	LineStreamParser parser(16, [&](network::StreamEvent event) { lines.push_back(event.data); });
	CHECK(parser.feed("{\"a\":1}\n{\"b\""));
	CHECK(parser.feed(":2}\r\n\n{\"c\":3}"));
	parser.finish();
	CHECK(lines == std::vector<std::string>{"{\"a\":1}", "{\"b\":2}", "{\"c\":3}"});
	CHECK_FALSE(parser.feed(std::string(20, 'x')));
}

TEST_CASE("net: WebSocket frames round-trip, fragments join, and limits close") {
	CHECK(acceptKey("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="); // RFC 6455's example
	CHECK(base64("hi") == "aGk=");

	FrameReader server(true, 1024);
	server.feed(encodeFrame(Opcode::Text, "hello", std::array<std::uint8_t, 4>{1, 2, 3, 4}));
	server.feed(encodeFrame(Opcode::Ping, "p", std::array<std::uint8_t, 4>{9, 9, 9, 9}));
	auto message = server.next();
	REQUIRE(message);
	CHECK(message->kind == FrameEvent::Kind::Message);
	CHECK(message->data == "hello");
	auto ping = server.next();
	REQUIRE(ping);
	CHECK(ping->kind == FrameEvent::Kind::Ping);
	CHECK_FALSE(server.next());

	FrameReader unmaskedToServer(true, 1024);
	unmaskedToServer.feed(encodeFrame(Opcode::Text, "x"));
	auto error = unmaskedToServer.next();
	REQUIRE(error);
	CHECK(error->kind == FrameEvent::Kind::Error);

	FrameReader client(false, 8);
	client.feed(encodeFrame(Opcode::Text, std::string(9, 'x')));
	auto tooLarge = client.next();
	REQUIRE(tooLarge);
	CHECK(tooLarge->kind == FrameEvent::Kind::Error);
	CHECK(tooLarge->code == 1009);

	FrameReader close(false, 64);
	close.feed(encodeFrame(Opcode::Close, closePayload(1001, "bye")));
	auto closing = close.next();
	REQUIRE(closing);
	CHECK(closing->kind == FrameEvent::Kind::Close);
	CHECK(closing->code == 1001);

	CHECK(isUtf8("caffè"));
	CHECK_FALSE(isUtf8("\xc3"));
	CHECK_FALSE(isUtf8("\xed\xa0\x80")); // a surrogate
}
