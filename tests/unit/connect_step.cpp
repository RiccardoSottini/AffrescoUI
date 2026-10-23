/// @file
/// @brief Unit tests for the "connect" step: as declared, or with "to" another address and a "token" (both bounded),
/// which only the runtime's onConnect sees.

#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Window.hpp>
#include <affresco/vcs/Repository.hpp>

#include "Headless.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen with a channel and actions that connect it, as declared and elsewhere.
constexpr const char* connectingScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"state": { "address": "wss://example.test:7420", "token": "t-1", "long": "" },
	"channels": { "host": { "socket": "host.sock" } },
	"actions": {
		"declared": [{ "connect": "host" }],
		"elsewhere": [{ "connect": "host", "to": "{{state.address}}", "token": "{{state.token}}" }],
		"empty": [{ "connect": "host", "to": "" }],
		"tooLong": [{ "connect": "host", "to": "{{state.long}}" }],
		"tokenAlone": [{ "connect": "host", "token": "x" }]
	},
	"root": { "id": "root", "type": "panel" }
})json";

/// @brief The screen, with an address one byte longer than a "to" may be in its state.
/// @return Its document.
json connectingDocument() {
	json document = json::parse(connectingScreen);
	document["state"]["long"] = std::string(2049, 'a');
	return document;
}

/// @brief What onConnect was told.
struct Connected {
	std::string channel; ///< The channel's name.
	std::string address; ///< The address, empty for the declared one.
	std::string token;   ///< The token, empty for none.
};

/// @brief The screen, built in a headless window, with what it connected and the errors it reported.
struct Built {
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(200, 100, clock); ///< Outlives the runtime.
	affresco::format::Rules rules;                                                    ///< The format's rules.
	affresco::vcs::Repository repository{rules, connectingDocument()};                ///< The document.
	affresco::format::Runtime runtime{repository, "."};                               ///< The panels.
	std::vector<Connected> connected;                                                 ///< onConnect's calls.
	std::vector<std::string> errors;                                                  ///< onError's messages.

	/// @brief Builds the screen and listens to it.
	Built() {
		runtime.onConnect.connect([this](const std::string& channel, const std::string& address,
		                                 const std::string& token) { connected.push_back({channel, address, token}); });
		runtime.onError.connect([this](const std::string& message) { errors.push_back(message); });
		runtime.build(*window);
	}

	/// @brief Runs an action.
	/// @param name The action.
	void run(const char* name) { runtime.runAction(name, window->root(), json::object()); }
};

} // namespace

TEST_CASE("connect: as declared, with no address or token") {
	Built built;
	built.run("declared");
	REQUIRE(built.connected.size() == 1);
	CHECK(built.connected[0].channel == "host");
	CHECK(built.connected[0].address.empty());
	CHECK(built.connected[0].token.empty());
}

TEST_CASE("connect: \"to\" and \"token\" are expressions, passed on as they evaluate") {
	Built built;
	built.run("elsewhere");
	REQUIRE(built.connected.size() == 1);
	CHECK(built.connected[0].address == "wss://example.test:7420");
	CHECK(built.connected[0].token == "t-1");
}

TEST_CASE("connect: an empty or too long address, or a token without one, is an error that names the place") {
	Built built;
	built.run("empty");
	built.run("tooLong");
	built.run("tokenAlone");
	CHECK(built.connected.empty());
	REQUIRE(built.errors.size() == 3);
	CHECK(built.errors[0].find(".to") != std::string::npos);
	CHECK(built.errors[1].find("2,048") != std::string::npos);
	CHECK(built.errors[2].find(".token") != std::string::npos);
}
