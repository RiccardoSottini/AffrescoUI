/// @file
/// @brief Sign-in (OAuth 2 / OpenID Connect) for connections, against the mock provider: the device flow and the
/// browser flow with PKCE, the bearer token on requests, refresh, signing out, kept refresh tokens; and the check that
/// matters most: no token ever shows in the document, the state, the history or the logs.

#include "Headless.hpp"
#include "HistoryFiles.hpp"
#include "MockIdentity.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <variant>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::test::MockIdentity;
using affresco::test::TemporaryFolder;
using affresco::test::ThreadExecutor;

namespace {

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).at(json::json_pointer(pointer));
}

/// @brief A screen with one connection that signs in at the mock: "login" signs in, then reads "me" into state.me
/// and the account into state.who; "me" reads again; "logout" signs out.
/// @param identity The mock.
/// @param flow "browser" or "device".
/// @return The screen's document.
json authScreen(const MockIdentity& identity, const std::string& flow) {
	const json me = {{"request", "api.me"}, {"into", "state.me"},
	                 {"onError", json::array({{{"set", "state.error"}, {"to", "{{error.code}}"}}})}};
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"connections",
	         {{"api",
	           {{"url", identity.origin() + "/api"}, {"auth", identity.provider(flow)},
	            {"operations", {{"me", {{"path", "/me"}}}}}}}}},
	        {"state", {{"me", nullptr}, {"who", nullptr}, {"error", nullptr}, {"failed", nullptr}, {"auth", nullptr}}},
	        {"persist", json::array({"state.me", "state.auth"})},
	        {"actions",
	         {{"login", json::array({{{"signIn", "api"},
	                                  {"onDone", json::array({{{"set", "state.who"}, {"to", "{{auth.api.account}}"}}, me})},
	                                  {"onError", json::array({{{"set", "state.failed"}, {"to", "{{error.message}}"}}})}}})},
	          {"me", json::array({{{"set", "state.error"}, {"to", nullptr}}, me})},
	          {"logout", json::array({{{"signOut", "api"}}})},
	          {"peek", json::array({{{"set", "state.auth"}, {"to", "{{auth.api}}"}}})}}},
	        {"root", {{"type", "label"}, {"id", "who"}, {"text", "{{auth.api.state}} {{auth.api.account}}"}}}};
}

/// @brief A screen shown headless, its network allowed to reach the mock and signing in with a store of the test's.
struct Shown {
	ThreadExecutor executor;                   ///< Runs the results.
	ManualClock clock;                         ///< The window's clock.
	std::unique_ptr<affresco::Window> window;  ///< The window.
	std::optional<affresco::Screen> screen;    ///< The screen.
	std::vector<std::string> opened;           ///< The URLs it asked a browser to open.

	/// @brief Builds the screen.
	/// @param identity The mock.
	/// @param document The screen.
	/// @param store Where it keeps refresh tokens.
	/// @param folder Where its file, history and state are kept (or none: in memory).
	Shown(const MockIdentity& identity, const json& document, std::shared_ptr<affresco::SecretStore> store,
	      const std::filesystem::path& folder = {})
	    : window(createHeadlessWindow(200, 100, clock)) {
		if(folder.empty()) screen.emplace(affresco::Screen::fromString(document.dump()));
		else {
			std::ofstream(folder / "auth.screen.json") << document.dump();
			screen.emplace(affresco::Screen::fromFile(folder / "auth.screen.json", folder / "data"));
		}
		CHECK_FALSE(screen->network().allow(identity.origin()));
		screen->setSignInOptions({std::move(store), [this](const std::string& url) { opened.push_back(url); }});
		screen->setExecutor(&executor);
		screen->build(*window);
	}

	/// @brief Runs an action and the results it gets, until a path of the state holds a value that isn't null.
	/// @param action The action.
	/// @param pointer The path.
	/// @return Whether it came.
	bool run(const char* action, const char* pointer) {
		screen->runAction(action, window->root());
		return executor.runUntil([this, pointer] { return !stateAt(*screen, pointer).is_null(); });
	}

	/// @brief The "auth" variable's entry of the connection, as the screen sees it (the action "peek" copies it into the
	/// state, which the token-leak check then reads too).
	/// @return {state, account, expires, ...}.
	[[nodiscard]] json auth() {
		screen->runAction("peek", window->root());
		return stateAt(*screen, "/auth");
	}
};

/// @brief Every file's bytes under a folder.
/// @param folder The folder.
/// @return The bytes, one file after another.
std::string everyFile(const std::filesystem::path& folder) {
	std::string all;
	for(const auto& entry : std::filesystem::recursive_directory_iterator(folder))
		if(entry.is_regular_file()) all += affresco::test::readText(entry.path());
	return all;
}

} // namespace

TEST_CASE("auth: no token ever shows in the document, the state, the history or the logs") {
	std::vector<std::string> logs;
	std::mutex logMutex;
	affresco::setLogSink([&logs, &logMutex](affresco::LogLevel, std::string_view message) {
		const std::scoped_lock lock(logMutex);
		logs.emplace_back(message);
	});
	{
		MockIdentity identity;
		const TemporaryFolder folder;
		const auto store = std::make_shared<affresco::MemorySecretStore>();
		Shown shown(identity, authScreen(identity, "device"), store, folder.path());
		identity.approve(); // the person enters the code at once: the first poll gets the tokens

		REQUIRE(shown.run("login", "/me"));
		CHECK(stateAt(*shown.screen, "/me") == json{{"account", "tester"}, {"ok", true}});
		CHECK(stateAt(*shown.screen, "/who") == "tester");
		CHECK(shown.auth()["state"] == "signedIn");
		CHECK(shown.auth()["account"] == "tester");
		CHECK(shown.auth()["expires"].is_number());

		// A refused token is refreshed once: the next request carries the new one
		identity.revokeAccess();
		REQUIRE(shown.run("me", "/error"));
		CHECK(shown.executor.runUntil([&identity] { return identity.grants() == 2; }));
		// The mock counts the grant before the screen has read it: a request in between finds no token
		REQUIRE(shown.executor.runUntil([&shown] { return shown.auth()["state"] == "signedIn"; }));
		shown.screen->runAction("me", shown.window->root());
		REQUIRE(shown.executor.runUntil([&shown] {
			return stateAt(*shown.screen, "/error").is_null() && shown.auth()["state"] == "signedIn";
		}));
		// A revision, so that the history has a commit after the sign-in
		CHECK(shown.screen->submitRevision(R"({"author": "agent", "set": {"/root/text": "{{auth.api.account}}"}})")
		          .status == affresco::RevisionStatus::Applied);
		shown.executor.runFor(std::chrono::milliseconds{50});

		const std::string seen = shown.screen->documentJson() + shown.screen->stateJson() +
		                         shown.screen->catalogJson() + everyFile(folder.path());
		std::string logged;
		{
			const std::scoped_lock lock(logMutex);
			for(const std::string& line : logs) logged += line + "\n";
		}
		const auto secrets = identity.secrets();
		REQUIRE(secrets.size() >= 5); // device code, two access and two refresh tokens
		for(const std::string& secret : secrets) {
			INFO(secret.substr(0, secret.find('-'))); // what it is, never the secret itself
			CHECK(seen.find(secret) == std::string::npos);
			CHECK(logged.find(secret) == std::string::npos);
		}
		// The kept secret is the refresh token, and only in the store
		CHECK(store->read("affrescoui/affrescoui/api").has_value());
	}
	affresco::setLogSink(nullptr);
}

TEST_CASE("auth: the browser flow signs in with PKCE through a loopback redirect") {
	MockIdentity identity;
	Shown shown(identity, authScreen(identity, "browser"), std::make_shared<affresco::MemorySecretStore>());
	shown.screen->runAction("login", shown.window->root());
	REQUIRE(shown.executor.runUntil([&shown] { return !shown.opened.empty(); }));
	const std::string url = shown.opened[0];
	CHECK(url.starts_with(identity.origin() + "/authorize?"));
	CHECK(url.contains("code_challenge_method=S256"));
	CHECK(url.contains("redirect_uri=http%3A%2F%2F127.0.0.1%3A"));
	std::string page;
	std::jthread browser([&page, url] { page = MockIdentity::browse(url); });
	REQUIRE(shown.executor.runUntil([&shown] { return !stateAt(*shown.screen, "/me").is_null(); }));
	browser.join();
	CHECK(page.starts_with("HTTP/1.1 200"));
	CHECK(stateAt(*shown.screen, "/who") == "tester");

	shown.screen->runAction("logout", shown.window->root());
	CHECK(shown.auth()["state"] == "signedOut");
	CHECK(shown.auth()["expires"].is_null());
}

TEST_CASE("auth: a redirect with another state is refused, and the sign-in fails") {
	MockIdentity identity;
	Shown shown(identity, authScreen(identity, "browser"), std::make_shared<affresco::MemorySecretStore>());
	shown.screen->runAction("login", shown.window->root());
	REQUIRE(shown.executor.runUntil([&shown] { return !shown.opened.empty(); }));
	std::string url = shown.opened[0];
	const std::size_t state = url.find("&state=") + 7;
	url[state] = url[state] == 'A' ? 'B' : 'A'; // an attacker's state
	std::string page;
	std::jthread browser([&page, url] { page = MockIdentity::browse(url); });
	REQUIRE(shown.executor.runUntil([&shown] { return !stateAt(*shown.screen, "/failed").is_null(); }));
	browser.join();
	CHECK(page.starts_with("HTTP/1.1 400"));
	CHECK(stateAt(*shown.screen, "/me").is_null());
	CHECK(shown.auth()["state"] != "signedIn");
}

TEST_CASE("auth: a request that needs a sign-in fails with signedOut, and sends nothing") {
	MockIdentity identity;
	Shown shown(identity, authScreen(identity, "device"), std::make_shared<affresco::MemorySecretStore>());
	REQUIRE(shown.run("me", "/error"));
	CHECK(stateAt(*shown.screen, "/error") == "signedOut");
	CHECK(shown.auth()["state"] == "signedOut");
	CHECK(identity.grants() == 0);
}

TEST_CASE("auth: a refused refresh signs out cleanly and forgets the kept token") {
	MockIdentity identity;
	const auto store = std::make_shared<affresco::MemorySecretStore>();
	Shown shown(identity, authScreen(identity, "device"), store);
	identity.approve();
	REQUIRE(shown.run("login", "/me"));
	REQUIRE(store->read("affrescoui/affrescoui/api"));

	identity.revokeAccess();
	identity.revokeRefresh();
	REQUIRE(shown.run("me", "/error"));
	REQUIRE(shown.executor.runUntil([&shown] { return shown.auth()["state"] == "signedOut"; }));
	CHECK_FALSE(store->read("affrescoui/affrescoui/api"));
	CHECK(shown.auth()["account"] == "");
}

TEST_CASE("auth: a kept refresh token signs in again at the next run, with no browser") {
	MockIdentity identity;
	const auto store = std::make_shared<affresco::MemorySecretStore>();
	{
		Shown first(identity, authScreen(identity, "device"), store);
		identity.approve();
		REQUIRE(first.run("login", "/me"));
	}
	Shown second(identity, authScreen(identity, "device"), store);
	REQUIRE(second.executor.runUntil([&second] { return second.auth()["state"] == "signedIn"; }));
	CHECK(second.opened.empty());
	REQUIRE(second.run("me", "/me"));
	CHECK(identity.grants() == 2); // the device grant, then the refresh
}

TEST_CASE("auth: revisions can't add a sign-in, nor change a provider") {
	MockIdentity identity;
	const affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, authScreen(identity, "device"));
	CHECK(repository.submit({{"author", "agent"}, {"add", {{"/actions/again", json::array({{{"signIn", "api"}}})}}}})
	          .status != affresco::RevisionStatus::Applied);
	CHECK(repository.submit({{"author", "agent"},
	                         {"set", {{"/connections/api/auth/tokenUrl", "https://elsewhere.example.com/token"}}}})
	          .status == affresco::RevisionStatus::Rejected);
}

TEST_CASE("auth: a provider's declaration is checked") {
	const auto screen = [](const json& auth) {
		return json{{"format", "affrescoui-screen"},
		            {"version", 2},
		            {"connections", {{"api", {{"url", "https://api.example.com"}, {"auth", auth}}}}},
		            {"root", {{"type", "label"}}}}
		    .dump();
	};
	const json good = {{"signInUrl", "https://id.example.com/authorize"},
	                   {"tokenUrl", "https://id.example.com/token"},
	                   {"clientId", "app"}};
	CHECK_NOTHROW(static_cast<void>(affresco::Screen::fromString(screen(good))));
	for(const auto& [key, value] : std::vector<std::pair<std::string, json>>{
	        {"tokenUrl", "http://id.example.com/token"},          // plain HTTP off the machine
	        {"signInUrl", "https://user@id.example.com/a"},       // a user name in the URL
	        {"clientId", "two words"},                            // not a word
	        {"flow", "implicit"},                                 // not a flow it has
	        {"secret", "x"}}) {                                   // a client secret: never in a document
		json bad = good;
		bad[key] = value;
		INFO(key);
		CHECK_THROWS(static_cast<void>(affresco::Screen::fromString(screen(bad))));
	}
	CHECK_THROWS(static_cast<void>(affresco::Screen::fromString(screen("other")))); // no such connection
}

TEST_CASE("auth: a provider's lifetimes and intervals are clamped, however large a number it sends") {
	namespace oauth = affresco::net::oauth;
	const auto now = std::chrono::system_clock::now();
	const auto tokens = oauth::readTokens(R"({"access_token":"a","token_type":"Bearer","expires_in":1e300})",
	                                      affresco::identity::Provider{}, {}, now);
	REQUIRE(std::holds_alternative<oauth::Tokens>(tokens));
	CHECK(std::get<oauth::Tokens>(tokens).expires == now + std::chrono::seconds(oauth::maxLifetime));
	const auto early = oauth::readTokens(R"({"access_token":"a","token_type":"Bearer","expires_in":-1e300})",
	                                     affresco::identity::Provider{}, {}, now);
	REQUIRE(std::holds_alternative<oauth::Tokens>(early));
	CHECK(std::get<oauth::Tokens>(early).expires == now + std::chrono::seconds(1));

	const auto codes = oauth::readDeviceCodes(
	    R"({"device_code":"d","user_code":"U","verification_uri":"https://example.test/device","interval":1e300,)"
	    R"("expires_in":-1e300})");
	REQUIRE(std::holds_alternative<oauth::DeviceCodes>(codes));
	CHECK(std::get<oauth::DeviceCodes>(codes).interval == std::chrono::seconds(60));
	CHECK(std::get<oauth::DeviceCodes>(codes).lifetime == std::chrono::seconds(10));
}
