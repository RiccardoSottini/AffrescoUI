/// @file
/// @brief Connections (RFC 0003): a screen requests the operations it declares from a local server, with loading and
/// error states; revisions may use declared operations but never add them, and a previewed proposal sends nothing
/// that writes.

#include "Headless.hpp"
#include "NetServer.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <atomic>
#include <string>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::test::httpResponse;
using affresco::test::LocalServer;
using affresco::test::readRequest;
using affresco::test::ThreadExecutor;

namespace {

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).at(json::json_pointer(pointer));
}

/// @brief A screen with one connection to a server: "get" reads an item, "miss" a path the server doesn't have, "save"
/// writes without a mock and "saveMock" with one.
/// @param origin The server's origin.
/// @return The screen's text.
std::string connectionScreen(const std::string& origin) {
	const json document = {
	    {"format", "affrescoui-screen"},
	    {"version", 2},
	    {"connections",
	     {{"api",
	       {{"url", origin},
	        {"headers", {{"X-Client", "test"}}},
	        {"operations",
	         {{"getItem", {{"path", "/items/{id}"}, {"result", "/item"}}},
	          {"missing", {{"path", "/missing"}}},
	          {"save", {{"method", "PUT"}, {"path", "/items/{id}"}, {"body", {{"name", "{{name}}"}}}, {"effect", "write"}}},
	          {"saveMock",
	           {{"method", "PUT"}, {"path", "/items/{id}"}, {"effect", "write"}, {"mock", {{"body", {{"ok", true}}}}}}}}}}}}},
	    {"state", {{"busy", false}, {"item", nullptr}, {"error", nullptr}, {"saved", nullptr}, {"title", "a"}}},
	    {"actions",
	     {{"get", json::array({{{"request", "api.getItem"}, {"with", {{"id", "4"}}}, {"into", "state.item"},
	                            {"loading", "state.busy"}}})},
	      {"miss", json::array({{{"request", "api.missing"},
	                             {"onError", json::array({{{"set", "state.error"}, {"to", "{{error}}"}}})}}})},
	      {"save", json::array({{{"request", "api.save"}, {"with", {{"id", "4"}, {"name", "b"}}},
	                             {"onError", json::array({{{"set", "state.error"}, {"to", "{{error.code}}"}}})}}})},
	      {"saveMock", json::array({{{"request", "api.saveMock"}, {"with", {{"id", "4"}}}, {"into", "state.saved"}}})}}},
	    {"root", {{"type", "label"}, {"id", "title"}, {"text", "{{state.title}}"}}}};
	return document.dump();
}

} // namespace

TEST_CASE("connections: a screen requests its declared operations from a local server, with loading and errors") {
	std::vector<std::string> seen;
	LocalServer server([&seen](affresco::net::tcp::Connection& connection) {
		const auto request = readRequest(connection);
		seen.push_back(request.head);
		const bool found = request.head.starts_with("GET /items/4 ");
		CHECK(connection.send(found ? httpResponse("200 OK", R"({"item": {"name": "a"}})")
		                            : httpResponse("404 Not Found", R"({"why": "gone"})"),
		                      std::chrono::seconds{5}));
	});
	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(connectionScreen(server.origin()));
	CHECK_FALSE(screen.network().allow(server.origin()));
	screen.setExecutor(&executor);
	screen.build(*window);

	screen.runAction("get", window->root());
	CHECK(stateAt(screen, "/busy") == true);
	REQUIRE(executor.runUntil([&screen] { return stateAt(screen, "/busy") == false; }));
	CHECK(stateAt(screen, "/item") == json{{"name", "a"}});
	REQUIRE(seen.size() == 1);
	CHECK(seen[0].find("X-Client: test") != std::string::npos);

	screen.runAction("miss", window->root());
	REQUIRE(executor.runUntil([&screen] { return !stateAt(screen, "/error").is_null(); }));
	const json error = stateAt(screen, "/error");
	CHECK(error["status"] == 404);
	CHECK(error["body"] == json{{"why", "gone"}});
	CHECK(error["message"].is_string());
}

TEST_CASE("connections: a previewed proposal answers writes from their mock, or refuses them, and sends nothing") {
	std::atomic<int> requests = 0;
	LocalServer server([&requests](affresco::net::tcp::Connection& connection) {
		static_cast<void>(readRequest(connection));
		++requests;
		CHECK(connection.send(httpResponse("200 OK", "{}"), std::chrono::seconds{5}));
	});
	ThreadExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(connectionScreen(server.origin()));
	CHECK_FALSE(screen.network().allow(server.origin()));
	screen.setExecutor(&executor);
	screen.build(*window);

	// A write the revision adds is reviewed, so it waits as a proposal
	const affresco::RevisionResult proposed = screen.submitRevision(
	    R"({"author": "agent", "add": {"/actions/again": [{"request": "api.save", "with": {"id": "5", "name": "c"}}]}})");
	REQUIRE(proposed.status == RevisionStatus::Proposed);
	REQUIRE(screen.previewProposal(proposed.proposal));
	screen.runAction("saveMock", window->root());
	screen.runAction("save", window->root());
	REQUIRE(executor.runUntil([&screen] { return !stateAt(screen, "/error").is_null(); }));
	CHECK(stateAt(screen, "/saved") == json{{"ok", true}});
	CHECK(stateAt(screen, "/error") == "preview");
	executor.runFor(std::chrono::milliseconds{50});
	CHECK(requests == 0);
}

TEST_CASE("connections: revisions use declared operations, never add or change them") {
	affresco::format::Rules rules;
	affresco::vcs::Repository repository(rules, json::parse(connectionScreen("https://api.example.com")));
	const auto add = [&repository](const json& steps) {
		return repository.submit({{"author", "agent"}, {"add", {{"/actions/go", steps}}}});
	};

	CHECK(add(json::array({{{"request", "api.getItem"}, {"with", {{"id", "1"}}}}})).status == RevisionStatus::Applied);
	CHECK(add(json::array({{{"request", "api.save"}, {"with", {{"id", "1"}}}}})).status == RevisionStatus::Proposed);
	CHECK(add(json::array({{{"request", "api.deleteAll"}}})).status == RevisionStatus::Rejected);
	CHECK(add(json::array({{{"request", "other.getItem"}}})).status == RevisionStatus::Rejected);
	CHECK(add(json::array({{{"request", "{{state.title}}"}}})).status == RevisionStatus::Rejected);
	CHECK(repository.submit({{"author", "agent"}, {"set", {{"/connections/api/url", "https://elsewhere.example.com"}}}})
	          .status == RevisionStatus::Rejected);
	CHECK(repository
	          .submit({{"author", "agent"},
	                   {"add", {{"/connections/api/operations/deleteAll", {{"method", "DELETE"}, {"path", "/"}}}}}})
	          .status == RevisionStatus::Rejected);
}

TEST_CASE("connections: a header's value is one line of text, without control bytes") {
	const affresco::format::Rules rules;
	json document = json::parse(connectionScreen("https://api.example.com"));
	json& header = document["connections"]["api"]["headers"]["X-Client"];
	header = "a\tb";
	CHECK_NOTHROW(rules.validate(document));
	const std::vector<std::string> bad = {"a\r\nb", "a\nb", std::string("a\0b", 3), "a\x1b" "b", "a\x7f" "b"};
	for(const std::string& value : bad) {
		CAPTURE(value.size());
		header = value;
		CHECK_THROWS_WITH_AS(rules.validate(document), doctest::Contains("literal text on one line"),
		                     affresco::DocumentError);
	}
}
