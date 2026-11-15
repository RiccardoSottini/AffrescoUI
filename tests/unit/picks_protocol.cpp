/// @file
/// @brief Unit tests for the live protocol's "getPicks" and "getSource" (channel::Session): the answers carry what
/// the host's callbacks give, a host without them refuses, a malformed "at" is refused before the host is asked, and a
/// peer whose reads are limited is refused both.

#include "Helpers.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>
#include <doctest/doctest.h>

#include <array>
#include <string>

using affresco::json;
using affresco::test::MemoryChannel;

namespace {

/// @brief A screen under version control, and a peer connected to it.
struct Peer {
	affresco::format::Rules rules;                                                  ///< The screen format.
	affresco::vcs::Repository versions{rules, affresco::test::permissionsScreen()}; ///< The screen.
	MemoryChannel channel;                                                          ///< What the peer receives.
};

/// @brief A pick record as the engine makes one.
/// @return The record.
json pickRecord() {
	return {{"id", "pick-3"},
	        {"revision", 41},
	        {"node", "#cardTitle"},
	        {"type", "label"},
	        {"pointer", "/root/children/2"},
	        {"bounds", {412, 236, 180, 20}},
	        {"bindings", {{"text", "{{card.title}}"}}},
	        {"exact", true},
	        {"source", {{"address", "#cardTitle"}, {"file", "board.json"}, {"shared", false}}}};
}

/// @brief Sends one message and gives its answer.
/// @param channel The peer's channel.
/// @param message The message, as JSON text.
/// @return The last message the peer received.
json answerTo(MemoryChannel& channel, const std::string& message) {
	channel.receive(message);
	REQUIRE_FALSE(channel.sent.empty());
	return channel.sent.back();
}

/// @brief Tells whether an answer is a rejection whose error says @p text.
/// @param answer The answer.
/// @param text Part of the error.
/// @return true if it is.
bool rejectedWith(const json& answer, const std::string& text) {
	return answer.value("type", "") == "rejected" && answer.value("error", "").contains(text);
}

} // namespace

TEST_CASE("picks protocol: getPicks answers the host's picks, with the message's id") {
	Peer peer;
	affresco::channel::Session session(peer.channel, peer.versions, "agent");
	const json picks = json::array({pickRecord()});
	session.setPicks([&picks] { return json(picks); }); // a copy: the host keeps its own
	const json answer = answerTo(peer.channel, R"({"type": "getPicks", "id": 7})");
	CHECK(answer == json{{"type", "picks"}, {"picks", picks}, {"id", 7}});
}

TEST_CASE("picks protocol: getSource asks the host for the node named by at, and answers its source") {
	Peer peer;
	affresco::channel::Session session(peer.channel, peer.versions, "agent");
	std::string asked;
	session.setSource([&asked](const std::string& at) {
		asked = at;
		return json{{"address", "/program/app.lisp/card-title"}, {"notation", "lisp"}, {"text", "(label #cardTitle)"}};
	});
	const json answer = answerTo(peer.channel, R"({"type": "getSource", "at": "#cardTitle", "id": 8})");
	CHECK(asked == "#cardTitle");
	CHECK(answer["type"] == "source");
	CHECK(answer["source"]["text"] == "(label #cardTitle)");
	CHECK(answer["id"] == 8);
}

TEST_CASE("picks protocol: a host without the callbacks refuses both reads") {
	Peer peer;
	const affresco::channel::Session session(peer.channel, peer.versions, "agent");
	const json picks = answerTo(peer.channel, R"({"type": "getPicks", "id": 1})");
	CHECK(rejectedWith(picks, "getPicks: this host keeps no picks"));
	CHECK(picks["id"] == 1);
	const json source = answerTo(peer.channel, R"({"type": "getSource", "at": "#title", "id": 2})");
	CHECK(rejectedWith(source, "getSource: this host can't tell a node's source"));
	CHECK(source["id"] == 2);
}

TEST_CASE("picks protocol: a getSource without a text of 1 to 512 bytes in at is refused before the host is asked") {
	Peer peer;
	affresco::channel::Session session(peer.channel, peer.versions, "agent");
	bool asked = false;
	session.setSource([&asked](const std::string&) {
		asked = true;
		return json::object();
	});
	const json tooLong = {{"type", "getSource"}, {"at", std::string(513, 'a')}};
	const std::array<std::string, 4> malformed = {R"({"type": "getSource"})", R"({"type": "getSource", "at": 3})",
	                                              R"({"type": "getSource", "at": ""})", tooLong.dump()};
	for(const std::string& message : malformed) {
		CAPTURE(message.substr(0, 60));
		CHECK(rejectedWith(answerTo(peer.channel, message), "getSource.at: expected a node (\"#id\")"));
	}
	CHECK_FALSE(asked);
	const json longest = {{"type", "getSource"}, {"at", std::string(512, 'a')}};
	CHECK(answerTo(peer.channel, longest.dump())["type"] == "source");
	CHECK(asked);
}

TEST_CASE("picks protocol: what the host throws for a node that isn't there is answered rejected with its message") {
	Peer peer;
	affresco::channel::Session session(peer.channel, peer.versions, "agent");
	session.setSource([](const std::string& at) -> json { throw affresco::DocumentError("no node " + at); });
	const json answer = answerTo(peer.channel, R"({"type": "getSource", "at": "#missing", "id": 3})");
	CHECK(answer == json{{"type", "rejected"}, {"error", "no node #missing"}, {"revision", 0}, {"id", 3}});
}

TEST_CASE("picks protocol: a peer whose reads are limited is refused both, and the host is never asked") {
	json screen = affresco::test::permissionsScreen();
	screen["permissions"]["data"] = json::array({{{"author", "agent"}, {"read", json::array({"/state/secret"})}}});
	const affresco::format::Rules rules;
	affresco::vcs::Repository versions(rules, screen);
	MemoryChannel channel;
	affresco::channel::Session session(channel, versions, "agent");
	bool asked = false;
	session.setPicks([&asked] {
		asked = true;
		return json::array();
	});
	session.setSource([&asked](const std::string&) {
		asked = true;
		return json::object();
	});
	CHECK(rejectedWith(answerTo(channel, R"({"type": "getPicks", "id": 1})"), "permission denied"));
	CHECK(rejectedWith(answerTo(channel, R"({"type": "getSource", "at": "#title", "id": 2})"), "permission denied"));
	CHECK_FALSE(asked);
}
