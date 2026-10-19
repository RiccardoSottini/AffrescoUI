/// @file
/// @brief Unit tests for the channel module: the live protocol (Session) over a Channel kept in memory.

#include "Helpers.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::test::MemoryChannel;

namespace {

/// @brief A screen under version control, and a peer connected to it.
struct Peer {
	affresco::format::Rules rules;                                                  ///< The screen format.
	affresco::vcs::Repository versions{rules, affresco::test::permissionsScreen()}; ///< The screen.
	MemoryChannel channel;                                                          ///< What the peer receives.
};

} // namespace

TEST_CASE("channel: a Session sends hello when created") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	REQUIRE(peer.channel.sent.size() == 1);
	const json& hello = peer.channel.sent[0];
	CHECK(hello.at("type") == "hello");
	CHECK(hello.at("format") == "affrescoui-screen");
	CHECK(hello.at("revision") == 0);
	CHECK(hello.at("head").at("id") == peer.versions.head().id);
	CHECK(hello.at("document") == peer.versions.document());
	CHECK(hello.at("state") == json{{"count", 0}});
	CHECK(hello.at("mode") == "default");
	CHECK(hello.at("you") == "agent");
	CHECK(hello.at("canReview") == false);
}

TEST_CASE("channel: a message of an unknown type gets an error") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	peer.channel.receive(R"({"type": "launchRockets", "id": 7})");
	REQUIRE(peer.channel.sent.size() == 2);
	CHECK(peer.channel.sent[1] ==
	      json{{"type", "error"}, {"message", "unknown message type 'launchRockets'"}, {"id", 7}});

	peer.channel.receive("not json");
	REQUIRE(peer.channel.sent.size() == 3);
	CHECK(peer.channel.sent[2]["type"] == "error");
	CHECK(peer.versions.head().number == 0);
}

TEST_CASE("channel: an accept from a peer that may not review is rejected") {
	Peer peer;
	const affresco::channel::Session agent(peer.channel, peer.versions, "agent");

	// The agent proposes a change it may not make directly, then tries to approve it itself
	peer.channel.receive(R"({"type": "revision", "id": 1, "set": {"#title/text": "Mine"}})");
	const auto proposed =
	    std::ranges::find(peer.channel.sent, json("proposed"), [](const json& m) { return m.value("type", json()); });
	REQUIRE(proposed != peer.channel.sent.end());
	const int proposal = (*proposed)["proposal"];
	const std::string head = (*proposed)["commit"];

	peer.channel.sent.clear();
	peer.channel.receive(json{{"type", "accept"}, {"id", 2}, {"proposal", proposal}, {"head", head}}.dump());
	REQUIRE(peer.channel.sent.size() == 1);
	CHECK(peer.channel.sent[0] == json{{"type", "rejected"},
	                                   {"error", "permission denied: 'agent' may not review changes"},
	                                   {"revision", 0},
	                                   {"id", 2}});
	CHECK(peer.versions.proposals().size() == 1); // still waiting for a reviewer
	CHECK(peer.versions.head().number == 0);
}

// Found by fuzzing (tests/fuzz, session): the parser's out_of_range got past Session::read and stopped the app
TEST_CASE("channel: a message with a number too large for a double gets an error") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	peer.channel.receive(R"({"type": "revision", "id": 1, "set": {"/state/count": 1e400}})");
	REQUIRE(peer.channel.sent.size() == 2);
	CHECK(peer.channel.sent[1]["type"] == "error");
	CHECK(peer.channel.sent[1]["message"].get<std::string>().find("number overflow") != std::string::npos);
	CHECK(peer.versions.state() == json{{"count", 0}});
}

// Found by fuzzing (tests/fuzz, session): the parse error quoted the bytes it read, and writing the answer threw
TEST_CASE("channel: a line that isn't UTF-8 gets an error, with the bytes replaced") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");

	peer.channel.receive("{\"type\": \xff}");
	REQUIRE(peer.channel.sent.size() == 2);
	CHECK(peer.channel.sent[1]["type"] == "error");
	CHECK(peer.channel.sent[1]["message"].get<std::string>().find("\xef\xbf\xbd") != std::string::npos); // U+FFFD
}

// Found by fuzzing (tests/fuzz, session, under UBSan): a limit of 1e20 was converted to an int, which is undefined
TEST_CASE("channel: getHistory takes any number as its limit") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");
	REQUIRE(peer.versions.submit({{"author", "studio"}, {"set", {{"#title/text", "A"}}}}).revision == 1);
	REQUIRE(peer.versions.submit({{"author", "studio"}, {"set", {{"#title/text", "B"}}}}).revision == 2);

	const std::vector<std::pair<std::string, std::size_t>> cases = {{"1", 1},  {"2.5", 2},  {"0", 3},
	                                                                {"-1", 3}, {"1e20", 3}, {"-1e20", 3}};
	for(const auto& [limit, entries] : cases) {
		CAPTURE(limit);
		peer.channel.sent.clear();
		peer.channel.receive(R"({"type": "getHistory", "limit": )" + limit + "}");
		REQUIRE(peer.channel.sent.size() == 1);
		REQUIRE(peer.channel.sent[0]["type"] == "history");
		CHECK(peer.channel.sent[0]["entries"].size() == entries);
		CHECK(peer.channel.sent[0]["entries"].back()["number"] == 2); // the newest
	}
}
