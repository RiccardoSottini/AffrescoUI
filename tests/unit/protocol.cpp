/// @file
/// @brief The live protocol as a table: every message a peer may send, from the table in SCREEN_FORMAT.md ("Peers and
/// the live protocol"), paired with the answer it gets, or the error. A message added to the format without a row
/// here, or a row for a message the format doesn't list, fails the test.

#include "Helpers.hpp"

#include <affresco/channel/Session.hpp>
#include <affresco/core/ScreenImages.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using affresco::json;

namespace {

/// @brief Pictures without a window: a few bytes, the size the scale asks for.
class FakeImages final : public affresco::ScreenImages {
public:
	[[nodiscard]] std::optional<std::string> render(const affresco::ImageRequest& request,
	                                                affresco::Image& image) override {
		image = {"png", 4 * request.scale, 3 * request.scale};
		return std::nullopt;
	}
	[[nodiscard]] bool rendering() const noexcept override { return false; }
};

/// @brief The test screen, an agent (may propose) and the studio (a reviewer), each with its own session.
struct Peers {
	affresco::format::Rules rules;                                                  ///< The screen format.
	affresco::vcs::Repository versions{rules, affresco::test::permissionsScreen()}; ///< The screen.
	FakeImages images;                                                              ///< For "render".
	affresco::test::MemoryChannel agentChannel, studioChannel;                      ///< What each peer is sent.
	affresco::channel::Session agent{agentChannel, versions, "agent", [] { return json{{"types", json::array()}}; },
	                                 &images}; ///< May change #board, proposes the rest.
	affresco::channel::Session studio{studioChannel, versions, "studio", {}, &images}; ///< Changes anything, reviews.
	std::vector<json> relayed; ///< "request" and "reply" messages the sessions handed on.

	/// @brief Records what the sessions relay.
	Peers() {
		agent.onRelay.connect([this](const json& message) { relayed.push_back(message); });
		studio.onRelay.connect([this](const json& message) { relayed.push_back(message); });
	}

	/// @brief Proposes a change as the agent.
	/// @return The proposal's id and head commit.
	std::pair<int, std::string> propose() {
		const affresco::RevisionResult result =
		    versions.submit({{"author", "agent"}, {"set", {{"#title/text", "Proposed"}}}});
		REQUIRE(result.status == affresco::RevisionStatus::Proposed);
		return {result.proposal, result.commit};
	}
};

/// @brief One row: a message, who sends it, and the answer it gets.
struct Row {
	std::string type;                              ///< The message's "type", as SCREEN_FORMAT.md lists it.
	bool reviewer;                                 ///< Sent by the studio (true) or the agent.
	std::function<json(Peers&)> message;           ///< Builds the message (after any setup it needs).
	std::string answer;                            ///< The answer's "type"; "" for one relayed to the other peers.
	std::string contains;                          ///< Text the answer's error or message holds ("" for none).
};

/// @brief A message without setup.
/// @param message The message.
/// @return A builder that returns it.
std::function<json(Peers&)> plain(json message) {
	return [message = std::move(message)](Peers&) { return message; };
}

/// @brief The message types of SCREEN_FORMAT.md's protocol table, in the rows sent by peers ("peer →",
/// "reviewer →", "peer → other peers").
/// @return The types.
std::set<std::string> documentedMessages() {
	// By an absolute path, as the build gives it: __FILE__ is relative when the compile cache rewrites the paths
	const std::filesystem::path file =
	    std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY).parent_path() / "SCREEN_FORMAT.md";
	std::ifstream in(file);
	REQUIRE(in);
	std::set<std::string> types;
	const std::regex row(R"(^\| (peer|reviewer) →[^|]* \| (.*) \|$)");
	const std::regex name(R"(`(\w+)`(?:\s*\(([^)]*)\))?)");
	for(std::string line; std::getline(in, line);) {
		std::smatch match;
		if(!std::regex_match(line, match, row)) continue;
		std::string cell = match[2];
		cell = cell.substr(0, cell.find(", relayed")); // "relayed with `from`" names a field, not a message
		for(auto it = std::sregex_iterator(cell.begin(), cell.end(), name); it != std::sregex_iterator(); ++it)
			types.insert((*it)[1]); // the message's name; its fields, in parentheses, are skipped
	}
	return types;
}

/// @brief The rows of convert: text in the Lisp notation into its value and back, and its errors.
/// @return The rows.
std::vector<Row> convertRows() {
	return {
	    {"convert", false, plain({{"type", "convert"}, {"notation", "lisp"}, {"text", "(label #a)"}}), "converted", ""},
	    {"convert", false, plain({{"type", "convert"}, {"notation", "lisp"}, {"value", {{"id", "a"}}}}), "converted",
	     ""},
	    {"convert", false, plain({{"type", "convert"}, {"notation", "lisp"}, {"text", "(label"}}), "rejected",
	     "line 1, column 7"},
	    {"convert", false, plain({{"type", "convert"}, {"notation", "yaml"}, {"text", "a"}}), "rejected",
	     "convert.notation"},
	};
}

/// @brief The table: every message, and its answer or error.
/// @return The rows.
std::vector<Row> protocolTable() {
	const auto proposal = [](Peers& peers) { return peers.propose(); };
	std::vector<Row> rows = {
	    {"revision", false, plain({{"type", "revision"}, {"set", {{"#board/color", "#00ff00"}}}}), "applied", ""},
	    {"revision", false, plain({{"type", "revision"}, {"set", {{"#title/text", "Mine"}}}}), "proposed", ""},
	    {"revision", false, plain({{"type", "revision"}, {"set", {{"#statusBar/color", "#000000"}}}}), "rejected",
	     "permission denied"},
	    {"revision", false, plain({{"type", "revision"}, {"set", {{"#nowhere/text", "x"}}}}), "rejected",
	     "no node with id 'nowhere'"},
	    {"revert", true,
	     [](Peers& peers) {
		     REQUIRE(peers.versions.submit({{"author", "studio"}, {"set", {{"#title/text", "A"}}}}).revision == 1);
		     return json{{"type", "revert"}, {"revision", "HEAD"}};
	     },
	     "applied", ""},
	    {"revert", true, plain({{"type", "revert"}}), "rejected", "revert.revision"},
	    {"restore", true,
	     [](Peers& peers) {
		     REQUIRE(peers.versions.submit({{"author", "studio"}, {"set", {{"#title/text", "A"}}}}).revision == 1);
		     return json{{"type", "restore"}, {"revision", 0}};
	     },
	     "applied", ""},
	    {"restore", true, plain({{"type", "restore"}, {"revision", 0}}), "rejected", "nothing to restore"},
	    {"undo", true,
	     [](Peers& peers) {
		     REQUIRE(peers.versions.submit({{"author", "studio"}, {"set", {{"#title/text", "A"}}}}).revision == 1);
		     return json{{"type", "undo"}};
	     },
	     "applied", ""},
	    {"getDocument", false, plain({{"type", "getDocument"}}), "document", ""},
	    {"getHistory", false, plain({{"type", "getHistory"}, {"limit", 3}}), "history", ""},
	    {"diff", false, plain({{"type", "diff"}, {"from", 0}, {"to", "HEAD"}}), "diff", ""},
	    {"diff", false, plain({{"type", "diff"}, {"proposal", 9}}), "rejected", "no proposal 9"},
	    {"fileChanges", false, plain({{"type", "fileChanges"}}), "rejected", "can't tell a change's files"},
	    {"blame", false, plain({{"type", "blame"}, {"target", "#title"}}), "blame", ""},
	    {"blame", false, plain({{"type", "blame"}}), "rejected", "blame.target"},
	    {"exportChanges", false, plain({{"type", "exportChanges"}, {"from", 0}, {"name", "mine"}}), "changes", ""},
	    {"getCatalog", false, plain({{"type", "getCatalog"}}), "catalog", ""},
	    {"render", false, plain({{"type", "render"}, {"scale", 2}}), "image", ""},
	    {"render", false, plain({{"type", "render"}, {"scale", 9}}), "rejected", "render.scale"},
	    {"accept", true,
	     [&](Peers& peers) {
		     const auto [id, head] = proposal(peers);
		     return json{{"type", "accept"}, {"proposal", id}, {"head", head}};
	     },
	     "done", ""},
	    {"accept", true,
	     [&](Peers& peers) { return json{{"type", "accept"}, {"proposal", proposal(peers).first}}; }, "rejected",
	     "accept.head"},
	    {"accept", false,
	     [&](Peers& peers) {
		     const auto [id, head] = proposal(peers);
		     return json{{"type", "accept"}, {"proposal", id}, {"head", head}};
	     },
	     "rejected", "may not review"},
	    {"reject", true,
	     [&](Peers& peers) { return json{{"type", "reject"}, {"proposal", proposal(peers).first}, {"reason", "No"}}; },
	     "done", ""},
	    {"reject", true, plain({{"type", "reject"}, {"proposal", 4}}), "rejected", "no proposal 4"},
	    {"preview", true, [&](Peers& peers) { return json{{"type", "preview"}, {"proposal", proposal(peers).first}}; },
	     "done", ""},
	    {"endPreview", true, plain({{"type", "endPreview"}}), "done", ""},
	    {"setMode", true, plain({{"type", "setMode"}, {"mode", "plan"}}), "done", ""},
	    {"setMode", true, plain({{"type", "setMode"}, {"mode", "anything"}}), "rejected", "setMode.mode"},
	    {"revoke", true,
	     [&](Peers& peers) {
		     const auto [id, head] = proposal(peers);
		     REQUIRE_FALSE(peers.versions.accept(id, {"studio", head, true}));
		     return json{{"type", "revoke"}, {"rule", peers.versions.grants().at(0).rule}};
	     },
	     "done", ""},
	    {"revoke", true, plain({{"type", "revoke"}, {"rule", "agent(#title)"}}), "rejected", "no grant"},
	    {"request", false, plain({{"type", "request"}, {"id", "r1"}, {"text", "Make it blue"}}), "", ""},
	    {"reply", true, plain({{"type", "reply"}, {"request", "r1"}, {"text", "Done"}}), "", ""},
	};
	std::ranges::move(convertRows(), std::back_inserter(rows));
	return rows;
}

/// @brief The answer a row's message gets: the last message its sender received, which carries the message's id.
/// @param peers The peers.
/// @param row The row.
/// @return The answer, or null if the sender got none.
json answerTo(Peers& peers, const Row& row) {
	json message = row.message(peers);
	message["id"] = 42;
	affresco::test::MemoryChannel& channel = row.reviewer ? peers.studioChannel : peers.agentChannel;
	channel.sent.clear();
	channel.receive(message.dump());
	return channel.sent.empty() ? json() : channel.sent.back();
}

} // namespace

TEST_CASE("channel: every message the format lists has a row in the protocol table, and no other") {
	std::set<std::string> tested;
	for(const Row& row : protocolTable()) tested.insert(row.type);
	CHECK(documentedMessages() == tested);
}

TEST_CASE("channel: every message in the protocol table gets its answer, or its error") {
	for(const Row& row : protocolTable()) {
		CAPTURE(row.type);
		CAPTURE(row.answer);
		Peers peers;
		const json answer = answerTo(peers, row);
		if(row.answer.empty()) { // relayed to the other peers, with "from" set by the channel; no answer
			REQUIRE(peers.relayed.size() == 1);
			CHECK(peers.relayed[0]["type"] == row.type);
			CHECK(peers.relayed[0]["from"] == (row.reviewer ? "studio" : "agent"));
			CHECK(answer.is_null());
			continue;
		}
		REQUIRE(answer.is_object());
		CHECK(answer["type"] == row.answer);
		CHECK(answer["id"] == 42);
		if(!row.contains.empty()) {
			const std::string text = answer.value("error", answer.value("message", std::string()));
			CHECK(text.find(row.contains) != std::string::npos);
		}
	}
}

TEST_CASE("channel: a message of a type the protocol doesn't have gets an error with its id") {
	Peers peers;
	const json answer = answerTo(peers, {"launch", false, plain({{"type", "launch"}}), "error", ""});
	CHECK(answer == json{{"type", "error"}, {"message", "unknown message type 'launch'"}, {"id", 42}});
}
