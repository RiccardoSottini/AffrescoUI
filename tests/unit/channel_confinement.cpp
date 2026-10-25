/// @file
/// @brief A screen's channels as the stock host gives them (SECURITY.md, "Channels"): only with the app's "channels"
/// capability (Screen::allowChannels()), and only to sockets inside the host's own socket folder
/// (Screen::confineChannels()), so that an app reaches its own host, never the Launcher's or another app's.

#include "Headless.hpp"
#include "NetServer.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/App.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::test::ThreadExecutor;

namespace {

/// @brief A screen with one channel "host" at an address, whose "closed" message keeps why in its state.
/// @param address The channel's "socket".
/// @return The screen.
std::unique_ptr<affresco::Screen> channelScreen(const std::string& address) {
	const json document = {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"state", {{"closed", ""}}},
	    {"channels", {{"host", {{"socket", address}}}}},
	    {"actions", {{"host:closed", json::array({{{"set", "state.closed"}, {"to", "{{event.message}}"}}})}}},
	    {"root", {{"type", "panel"}}}};
	return std::make_unique<affresco::Screen>(affresco::Screen::fromString(document.dump()));
}

/// @brief Why a screen's channel closed, once it has, after starting it confined to a folder.
/// @param address The channel's "socket".
/// @param folder The socket folder.
/// @param allowed Whether its channels are allowed.
/// @return The "closed" message.
std::string closedWhy(const std::string& address, const std::filesystem::path& folder, bool allowed = true) {
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	ThreadExecutor executor;
	const auto screen = channelScreen(address);
	screen->setSocketDirectory(folder);
	screen->confineChannels(true);
	screen->allowChannels(allowed);
	screen->build(*window);
	screen->startChannels(executor);
	REQUIRE(executor.runUntil([&] { return json::parse(screen->stateJson())["closed"] != ""; }));
	return json::parse(screen->stateJson())["closed"].get<std::string>();
}

} // namespace

TEST_CASE("channel confinement: a confined screen's socket stays inside its host's folder") {
	const std::filesystem::path root = std::filesystem::absolute("out/confined");
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root / "app");
	std::filesystem::create_directories(root / "other");
	std::filesystem::create_directory_symlink(root / "other", root / "app" / "link");

	// Its own folder's socket is tried (nobody listens here), never refused
	CHECK_FALSE(closedWhy("studio.sock", root / "app").contains("socket folder"));
	CHECK_FALSE(closedWhy("sub/studio.sock", root / "app").contains("socket folder"));
	// Another app's or the Launcher's: refused, whatever the spelling
	CHECK(closedWhy("../studio.sock", root / "app").contains("isn't a path inside"));
	CHECK(closedWhy("../other/studio.sock", root / "app").contains("isn't a path inside"));
	CHECK(closedWhy((root / "studio.sock").string(), root / "app").contains("isn't a path inside"));
	CHECK(closedWhy("link/studio.sock", root / "app").contains("leads out"));
	std::filesystem::remove_all(root);
}

TEST_CASE("channel confinement: without the channels capability no channel connects, and the screen is told why") {
	CHECK(closedWhy("studio.sock", "out/confined-unused", false).contains("\"channels\" capability"));
}

TEST_CASE("channel confinement: a screen with channels uses the channels capability, declared with a reason") {
	const json screen = json::parse(R"({ "channels": { "team": { "socket": "studio.sock" } } })");
	REQUIRE(affresco::format::usedCapabilities(screen).size() == 1);
	CHECK(affresco::format::usedCapabilities(screen)[0].key() == "channels");
	CHECK(affresco::format::usedCapabilities(json::parse(R"({ "channels": {} })")).empty());

	const json manifest = {{"format", "affrescoui-app"},
	                       {"version", 1},
	                       {"id", "team"},
	                       {"name", "Team"},
	                       {"entry", "index.screen.json"},
	                       {"capabilities", {{"channels", "Shows who else is on the board."}}}};
	const affresco::format::AppManifest app = affresco::format::parseApp(manifest);
	REQUIRE(app.capabilities.size() == 1);
	CHECK(app.capabilities[0].kind == affresco::format::AppCapability::Kind::Channels);
	CHECK(app.capabilities[0].why == "Shows who else is on the board.");
	CHECK(affresco::format::undeclaredCapabilities(app, screen).empty());
}
