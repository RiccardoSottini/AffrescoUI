/// @file
/// @brief Tests that a peer's command list (getCommands) after each kind of change is the list a fresh screen of the
/// same document gives: a revision that changes what a command reaches, adds or removes one, a change to the run
/// rules, a grant given and revoked, a program's definition changed, and a feature installed. The list's entries, their
/// `run` and `reason` and their order must all match, so nothing kept from an earlier list can survive a change.

#include "CommandBoard.hpp"

#include <doctest/doctest.h>

#include <string>

using commandboard::Board;
using commandboard::json;
using commandboard::MemoryChannel;

namespace {

/// @brief A peer's whole command list, in its order.
/// @param board The app.
/// @param peer Who asks.
/// @return The `commands` of its getCommands answer, `all` given.
json listed(Board& board, MemoryChannel& peer) {
	const json answer = board.ask(peer, {{"type", "getCommands"}, {"all", true}});
	REQUIRE(answer["type"] == "commands");
	return answer["commands"];
}

/// @brief Checks that both peers list what they would list on a fresh screen of the board's document now.
/// @param board The app, changed.
void sameAsFresh(Board& board) {
	Board fresh(false, board.screen.documentJson());
	CHECK(listed(board, board.agent) == listed(fresh, fresh.agent));
	CHECK(listed(board, board.studio) == listed(fresh, fresh.studio));
}

/// @brief Puts a reviewer's revision on main: applied, or proposed and then accepted by the person (program code and
/// service uses wait for them).
/// @param board The app.
/// @param revision The revision message: `set` or `remove`.
void revise(Board& board, json revision) {
	revision["type"] = "revision";
	revision["message"] = "a change";
	const json answer = board.ask(board.studio, std::move(revision));
	if(answer["type"] == "proposed") {
		std::string error;
		REQUIRE_MESSAGE(
		    board.screen.acceptProposal(answer["proposal"].get<int>(), answer["commit"].get<std::string>(), &error),
		    error);
	} else {
		REQUIRE_MESSAGE(answer["type"] == "applied", answer.dump());
	}
	board.window->render();
	board.executor.pumpUntil();
}

/// @brief A peer's entry for a command.
/// @param list The list.
/// @param name The command.
/// @return Its entry, or null.
json entry(const json& list, const std::string& name) {
	for(const json& command : list)
		if(command["name"] == name) return command;
	return nullptr;
}

} // namespace

TEST_CASE(
    "commands: a list after a revision is the list of a fresh screen: reach changed, a command added or removed") {
	Board board;
	CHECK(entry(listed(board, board.agent), "addCard")["run"] == "ask"); // listed once before every change
	revise(board,
	       {{"set", {{"/actions/addCard/do", json::array({{{"invoke", "files.write"}, {"with", {{"path", "a"}}}}})}}}});
	CHECK(entry(listed(board, board.agent), "addCard")["run"] == "person"); // it writes through a service now
	sameAsFresh(board);
	revise(board, {{"set", {{"/actions/addCard/do", json::array({{{"set", "state.note"}, {"to", "added"}}})}}}});
	CHECK(entry(listed(board, board.agent), "addCard")["run"] == "ask"); // and no longer does
	sameAsFresh(board);
	revise(board, {{"set",
	                {{"/actions/pinCard",
	                  {{"title", "Pin card"}, {"do", json::array({{{"set", "state.note"}, {"to", "pinned"}}})}}}}}});
	CHECK(entry(listed(board, board.agent), "pinCard")["run"] == "ask");
	sameAsFresh(board);
	revise(board, {{"remove", {"/actions/pinCard"}}});
	CHECK(entry(listed(board, board.agent), "pinCard").is_null());
	CHECK(board.run("pinCard")["type"] == "rejected");
	sameAsFresh(board);
}

TEST_CASE("commands: a list after the mode changes is the list of a fresh screen in that mode") {
	Board board;
	CHECK(entry(listed(board, board.agent), "moveCard")["run"] == "allow");
	CHECK(board.ask(board.studio, {{"type", "setMode"}, {"mode", "plan"}})["type"] != "rejected");
	CHECK(entry(listed(board, board.agent), "moveCard")["run"] == "deny");
	CHECK(board.run("moveCard", {{"card", "c1"}, {"to", "done"}})["type"] == "rejected");
	{
		Board fresh(false, board.screen.documentJson());
		CHECK(fresh.ask(fresh.studio, {{"type", "setMode"}, {"mode", "plan"}})["type"] != "rejected");
		CHECK(listed(board, board.agent) == listed(fresh, fresh.agent));
	}
	CHECK(board.ask(board.studio, {{"type", "setMode"}, {"mode", "default"}})["type"] != "rejected");
	CHECK(entry(listed(board, board.agent), "moveCard")["run"] == "allow");
	sameAsFresh(board);
}

TEST_CASE("commands: a list after a grant is given and revoked is the list of a fresh screen again") {
	Board board;
	const json before = listed(board, board.agent);
	const json waiting = board.run("addCard", {{"title", "One"}});
	CHECK(board.decide(board.studio, "allowRun", waiting["run"], true)["type"] == "done");
	CHECK(entry(listed(board, board.agent), "addCard")["run"] == "allow");
	CHECK(board.ask(board.studio, {{"type", "revokeRun"}, {"rule", "agent(run:addCard)"}})["type"] == "done");
	CHECK(listed(board, board.agent) == before);
	sameAsFresh(board);
}

TEST_CASE("commands: a list after a program's definition changes is the list of a fresh screen") {
	Board board(true);
	const json program = json::parse(board.screen.documentJson())["program"];
	std::string file;
	for(const auto& [name, definitions] : program.items())
		if(definitions.contains("addCard")) file = name;
	REQUIRE_FALSE(file.empty());
	CHECK(entry(listed(board, board.agent), "addCard")["run"] == "ask");
	revise(board,
	       {{"set",
	         {{"/program/" + file + "/addCard",
	           R"((defcommand addCard "Adds a card to do." :title "Add card" :arguments {:title {:type string}} [title]
  (invoke "files.write" :with {:path title :text "card"})))"}}}});
	CHECK(entry(listed(board, board.agent), "addCard")["run"] == "person"); // it writes through a service now
	sameAsFresh(board);
}

TEST_CASE("commands: a list after a feature is installed is the list of a fresh screen") {
	Board board;
	const json before = listed(board, board.agent);
	revise(board, {{"set",
	                {{"/features",
	                  {{"pins",
	                    {{"release", "1.0.0"},
	                     {"source", "revision"},
	                     {"enabled", true},
	                     {"order", 0},
	                     {"provides",
	                      {{"state", {{"list", json::array()}}},
	                       {"actions",
	                        {{"clear",
	                          {{"title", "Clear pins"},
	                           {"do", json::array({{{"set", "state.pins.list"}, {"to", json::array()}}})}}}}}}}}}}}}}});
	const json after = listed(board, board.agent);
	CHECK(after.size() == before.size() + 1);
	CHECK_FALSE(entry(after, "pins.clear").is_null());
	sameAsFresh(board);
}
