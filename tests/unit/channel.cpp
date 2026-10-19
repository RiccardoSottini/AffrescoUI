/// @file
/// @brief Unit tests for the channel module: the live protocol (Session) over a Channel kept in memory.

#include "Helpers.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/core/Log.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <string_view>
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

TEST_CASE("channel: a peer's text in a rejected revision reaches the log as one line") {
	Peer peer;
	const affresco::channel::Session agent(peer.channel, peer.versions, "agent");
	std::vector<std::string> logged;
	affresco::setLogSink([&](affresco::LogLevel, std::string_view message) { logged.emplace_back(message); });
	peer.channel.receive(R"({"type": "revision", "id": 1, "set": {"#a\nFAKE: approved\u001b[2J/text": "x"}})");
	affresco::setLogSink(nullptr);

	REQUIRE(logged.size() == 1);
	CHECK(logged[0].find("#a\\nFAKE: approved\\x1b[2J") != std::string::npos);
	CHECK(std::ranges::none_of(logged[0], [](char c) { return c == '\n' || c == '\x1b'; }));
	CHECK(peer.channel.sent.back()["type"] == "rejected"); // the peer still gets its own text back, as JSON
}
