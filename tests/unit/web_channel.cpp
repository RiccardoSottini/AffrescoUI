/// @file
/// @brief A screen's channels at a ws:// or wss:// address (6.2-web, SCREEN_FORMAT.md "Channels and persistence"):
/// they connect to a remote peer through net::connectPeer(), natively here and through the browser's WebSocket on
/// the web, with the token from the host (Screen::connectChannel()), never from the document.

#include "Headless.hpp"
#include "NetServer.hpp"

#include <affresco/affresco.hpp>
#include <affresco/net/RemotePeers.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::test::ThreadExecutor;

namespace {

/// @brief Accepts the token "good" as the author "agent/test" (an obviously fake token).
class FakeVerifier final : public affresco::net::TokenVerifier {
public:
	/// @brief Checks a token.
	/// @param token The token.
	/// @return "agent/test" for "good", else nullopt.
	std::optional<std::string> verify(std::string_view token) override {
		if(token == "good") return "agent/test";
		return std::nullopt;
	}
};

/// @brief A screen with one channel "host" at an address, whose "hello" and "closed" messages set its state.
/// @param address The channel's "socket".
/// @return The screen.
std::unique_ptr<affresco::Screen> channelScreen(const std::string& address) {
	const json document = {{"format", "affrescoui-screen"},
	                       {"version", 1},
	                       {"state", {{"greeted", false}, {"closed", ""}}},
	                       {"channels", {{"host", {{"socket", address}}}}},
	                       {"actions",
	                        {{"host:hello", json::array({{{"set", "state.greeted"}, {"to", true}}})},
	                         {"host:closed", json::array({{{"set", "state.closed"}, {"to", "{{event.message}}"}}})}}},
	                       {"root", {{"type", "panel"}}}};
	return std::make_unique<affresco::Screen>(affresco::Screen::fromString(document.dump()));
}

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).at(json::json_pointer(pointer));
}

} // namespace

TEST_CASE("web channel: connectChannel() names only the channels the document declares") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	ThreadExecutor executor;
	const auto screen = channelScreen("ws://127.0.0.1:1/live");
	screen->build(*window);
	screen->startChannels(executor);
	CHECK_FALSE(screen->connectChannel("elsewhere", "ws://127.0.0.1:1/live", "good"));
}

TEST_CASE("web channel: a ws:// address off the loopback is refused, and the screen is told why") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	ThreadExecutor executor;
	const auto screen = channelScreen("ws://example.com/live");
	screen->build(*window);
	screen->startChannels(executor);
	CHECK(executor.runUntil([&] { return stateAt(*screen, "/closed") != ""; }));
	CHECK(stateAt(*screen, "/closed").get<std::string>().find("wss://") != std::string::npos);
	CHECK(stateAt(*screen, "/greeted") == false);
}

TEST_CASE("web channel: the host connects a channel to a peer server with its token, and messages arrive") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	ThreadExecutor executor;
	std::vector<std::unique_ptr<affresco::Channel>> peers;
	std::vector<std::string> authors;
	affresco::net::PeerServer server(executor, "127.0.0.1:0", std::make_shared<FakeVerifier>(),
	                                 [&](std::unique_ptr<affresco::Channel> channel, std::string author) {
		                                 peers.push_back(std::move(channel));
		                                 authors.push_back(std::move(author));
	                                 });
	const auto screen = channelScreen("studio.sock"); // the document's own address: the host replaces it
	screen->setSocketDirectory("out/web-channel-unused");
	screen->build(*window);
	screen->startChannels(executor);
	REQUIRE(screen->connectChannel("host", "ws://127.0.0.1:" + std::to_string(server.port()) + "/live", "good"));
	REQUIRE(executor.runUntil([&] { return !peers.empty(); }));
	CHECK(authors.front() == "agent/test");
	peers.front()->send(R"({"type":"hello"})");
	CHECK(executor.runUntil([&] { return stateAt(*screen, "/greeted") == true; }));
}
