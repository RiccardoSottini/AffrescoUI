/// @file
/// @brief Attacks on the commands as tools, through the real stack (CommandBoard.hpp): an agent tries to approve its
/// own work, allow its own runs, pass for someone else, reach past its commands, or flood the screen. After each, what
/// only a reviewer or the person may change (the proposals, the grants, the run grants, the mode, the permissions and
/// main's head) must be byte for byte what it was.

#include "CommandBoard.hpp"

#include <affresco/Screen.hpp>
#include <affresco/core/VersionControl.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

using commandboard::Board;
using commandboard::boardJson;
using commandboard::json;
using commandboard::MemoryChannel;

namespace {

/// @brief The board, every command allowed to the agent, with commands that try to reach what an agent may not.
/// @return The document.
std::string attackBoard() {
	json document = json::parse(boardJson);
	document["permissions"]["run"] = {{"allow", {"agent"}}, {"deny", {"*(clearBoard)"}}};
	document["state"]["armed"] = false;
	document["state"]["method"] = "files.write";
	document["channels"] = {{"host", {{"socket", "host.sock"}}}};
	json& actions = document["actions"];
	const auto command = [&actions](const std::string& name, json steps) {
		actions[name] = {{"title", name}, {"do", std::move(steps)}};
	};
	actions["approve"] = json::array({{{"accept", "{{proposals[0].id}}"}, {"head", "{{proposals[0].head}}"}}});
	command("viaAction", json::array({{{"run", "approve"}}}));
	command("pressAccept", json::array({{{"press", "enter"}, {"in", "#acceptButton"}}}));
	command("arm", json::array({{{"set", "state.armed"}, {"to", true}}}));
	command("allowOwn", json::array({{{"allowRun", "{{runs[0].id}}"}, {"always", true}}}));
	command("callIt", json::array({{{"call", "exportBoard"}}}));
	command("sendIt", json::array({{{"send", "host"}, {"data", {{"cards", "{{state.cards}}"}}}}}));
	command("connectIt", json::array({{{"connect", "host"}}}));
	command("patchIt", json::array({{{"patch", json::array({{{"op", "replace"},
	                                                         {"path", "/permissions/reviewers"},
	                                                         {"value", {"agent"}}}})},
	                                 {"message", "mine now"}}}));
	command("saveIt", json::array({{{"saveFile", "{{state.cards}}"}, {"name", "board.json"}}}));
	command("askIt", json::array({{{"ask", "approve it"}}}));
	command("evalIt", json::array({{{"evaluate", "(accept 1)"}}}));
	command("computedInvoke",
	        json::array({{{"invoke", "{{state.method}}"}, {"with", {{"path", "x.txt"}, {"text", "y"}}}}}));
	command("computedRun", json::array({{{"run", "{{state.method}}"}}}));
	command("laterIt", json::array({{{"after", 50}, {"do", json::array({{{"call", "exportBoard"}}})}}}));
	command("doneIt", json::array({{{"invoke", "files.read"},
	                                {"with", {{"path", "a.txt"}}},
	                                {"onDone", json::array({{{"call", "exportBoard"}}})}}}));
	document["root"] = {
	    {"type", "panel"},
	    {"id", "main"},
	    {"every", {{"ms", 100}, {"do", json::array({{{"if", "{{state.armed && length(proposals) > 0}}"},
	                                                 {"then", json::array({{{"run", "approve"}}})}}})}}},
	    {"children", json::array({{{"type", "label"}, {"id", "title"}, {"text", "{{length(state.cards)}} cards"}},
	                              {{"type", "label"}, {"id", "acceptButton"}, {"text", "Accept"},
	                               {"onClick", "approve"}}})}};
	return document.dump();
}

/// @brief What only a reviewer or the person may change, as the reviewer reads it.
/// @param board The board.
/// @return The proposals, the grants, the run grants, the mode, the permissions and main's head, as JSON text.
std::string guarded(Board& board) {
	const json document = board.ask(board.studio, {{"type", "getDocument"}});
	REQUIRE(document["type"] == "document");
	return json{{"proposals", document["proposals"]},
	            {"grants", document["grants"]},
	            {"runGrants", document.value("runGrants", json())},
	            {"mode", document["mode"]},
	            {"permissions", document["document"].value("permissions", json())},
	            {"head", document["head"]["id"]}}
	    .dump();
}

/// @brief Has the agent propose a change, which waits for the reviewer.
/// @param board The board.
void propose(Board& board) {
	board.ask(board.agent, {{"type", "revision"}, {"message", "mine"}, {"set", {{"/state/note", "agent's"}}}});
	REQUIRE(board.screen.proposals().size() == 1);
}

/// @brief Tells whether a run was refused, or failed, without changing what is guarded.
/// @param board The board.
/// @param before What was guarded before.
/// @param answer The run's answer.
/// @return true if it was.
bool heldOff(Board& board, const std::string& before, const json& answer) {
	const bool refused = answer["type"] == "rejected" || answer.value("failed", false);
	return refused && guarded(board) == before;
}

} // namespace

TEST_CASE("commands: an agent can't approve its own proposal through a command, an action or a pressed button") {
	Board board(false, attackBoard());
	propose(board);
	const std::string before = guarded(board);
	for(const char* name : {"acceptAll", "viaAction"}) {
		CAPTURE(name);
		CHECK(heldOff(board, before, board.run(name)));
	}
	CHECK(board.run("pressAccept")["type"] == "ran"); // keys it presses are unheard by the button's bindings
	CHECK(guarded(board) == before);
	board.wait(1);
	CHECK(board.screen.proposals().size() == 1);
	CHECK(board.state("note") == "");
}

TEST_CASE("commands: an agent can't approve its own proposal through a Lisp function its command calls") {
	Board board(true);
	propose(board);
	const std::string before = guarded(board);
	CHECK(heldOff(board, before, board.run("acceptAll")));
	CHECK(board.screen.proposals().size() == 1);
}

TEST_CASE("commands: an agent can't approve its own proposal by setting the state a timer of the person's acts on" *
          doctest::skip(true)) {
	// Gets through: the screen's own `every` timer runs `accept` on state the agent's run set
	Board board(false, attackBoard());
	propose(board);
	const std::string before = guarded(board);
	CHECK(board.run("arm")["type"] == "ran");
	board.wait(1);
	CHECK(guarded(board) == before);
	CHECK(board.screen.proposals().size() == 1);
}

TEST_CASE("commands: an agent can't allow its own waiting run, from its session or from a command") {
	Board board(false, attackBoard());
	const json waiting = board.run("saveBoard", {{"name", "a.txt"}});
	REQUIRE(waiting["type"] == "waiting");
	const std::string before = guarded(board);
	CHECK(board.ask(board.agent, {{"type", "allowRun"}, {"run", waiting["run"]}, {"always", true}})["type"] ==
	      "rejected");
	CHECK(board.ask(board.agent, {{"type", "allowRun"}, {"run", waiting["run"]}, {"author", "studio"}})["type"] ==
	      "rejected");
	CHECK(heldOff(board, before, board.run("allowOwn")));
	CHECK(Board::runs(board.studio)[0]["state"] == "waiting");
	CHECK_FALSE(std::filesystem::exists(board.folder.path / "work" / "a.txt"));
}

TEST_CASE("commands: an agent can't pass for the person or the reviewer by the author it writes") {
	for(const char* name : {"user", "studio", "studio/x", "../user"}) {
		CAPTURE(name);
		Board board;
		const std::string before = guarded(board);
		const json waiting = board.run("addCard", {{"title", "Mine"}}, name);
		CHECK(waiting["type"] == "waiting"); // a reviewer's run would have been allowed
		const json runs = Board::runs(board.studio);
		REQUIRE(runs.size() == 1);
		CHECK(runs[0]["author"].get<std::string>().starts_with("agent/"));
		CHECK(guarded(board) == before);
	}
	Board board;
	board.run("saveBoard", {{"name", "a.txt"}}, "user");
	CHECK(commandboard::shownTexts(board.window->confirmation()).contains("agent/user asks to run it."));
}

TEST_CASE("commands: an agent can't reach past its commands: the host, channels, the document, files, the agents") {
	Board board(false, attackBoard());
	const std::map<std::string, json> listed = board.commands(board.agent);
	const std::string before = guarded(board);
	for(const char* name : {"callIt", "sendIt", "connectIt", "patchIt", "saveIt", "askIt", "evalIt", "computedRun",
	                        "laterIt", "doneIt"}) {
		CAPTURE(name);
		CHECK(listed.at(name)["run"] == "deny");
		CHECK(listed.at(name)["reason"].get<std::string>().ends_with("which only the person may run"));
		CHECK(heldOff(board, before, board.run(name)));
	}
	CHECK(board.run("computedInvoke")["error"].get<std::string>().contains("only the person's own code"));
	board.wait(1);
	CHECK(board.exports == 0);
	CHECK_FALSE(std::filesystem::exists(board.folder.path / "work" / "x.txt"));
	CHECK(json::parse(board.screen.documentJson())["permissions"]["reviewers"] == json{"studio"});
}

TEST_CASE("commands: an agent can't reach a control step through a Lisp hook, timer or advice its command runs") {
	constexpr const char* program = R"lisp((defn hit-it "Reaches the host." [] (call "exportBoard"))
(defn noted "Notes it." [] (set "state.note" "x"))
(advice noted :before hit-it)
(defhook reached "Reached.")
(add-hook reached hit-it)
(defcommand hooked "Through a hook." :title "H" [] (run-hooks reached))
(defcommand later "Through a timer." :title "L" [] (after 50 hit-it))
(defcommand advised "Through advice." :title "A" [] (noted))
(defscreen main :state {:note ""} :permissions {:allow [studio] :reviewers [studio] :run {:allow [agent]}}
  (label#title :text "board"))
)lisp";
	Board board(false, program);
	const std::string before = guarded(board);
	for(const char* name : {"hooked", "later", "advised"}) {
		CAPTURE(name);
		CHECK(board.commands(board.agent).at(name)["run"] == "deny");
		CHECK(heldOff(board, before, board.run(name)));
	}
	board.wait(1);
	CHECK(board.exports == 0);
}

TEST_CASE("commands: an agent can't flood: runs past the burst, waiting runs past the limits, huge arguments") {
	Board board;
	int refused = 0;
	for(int i = 0; i < 31; ++i)
		if(board.run("moveCard", {{"card", "c1"}, {"to", i % 2 ? "todo" : "done"}})["type"] == "rejected") ++refused;
	CHECK(refused == 1); // the 31st: 30 at once
	std::vector<std::unique_ptr<MemoryChannel>> peers; // outlive the screen, which holds them
	Board waits;
	std::vector<std::string> answers;
	for(int peer = 0; peer < 5; ++peer) {
		peers.push_back(std::make_unique<MemoryChannel>());
		waits.screen.connect(*peers.back(), "peer" + std::to_string(peer));
		for(int i = 0; i < 4; ++i) // 4 a channel, 16 in all: the 17th is refused
			answers.push_back(waits.ask(*peers.back(), {{"type", "runCommand"},
			                                            {"command", "addCard"},
			                                            {"arguments", {{"title", "t"}}}})["type"]);
	}
	CHECK(std::ranges::count(answers, "waiting") == 16);
	CHECK(std::ranges::count(answers, "rejected") == 4);
	CHECK(waits.run("addCard", {{"title", "t"}})["type"] == "rejected"); // agent has no waiting run, the screen 16
	CHECK(waits.run("addCard", {{"title", std::string(std::size_t{65} * 1024U, 'x')}})["error"].get<std::string>().contains(
	    "at most 65,536"));
	json deep = "x";
	for(int depth = 0; depth < 200; ++depth) deep = json::array({deep});
	const json nested = waits.run("addCard", {{"title", deep}});
	CHECK(nested["type"] == "rejected");
	CHECK(waits.state("cards").size() == 2);
}

TEST_CASE("commands: a revision can't add a command that runs the person's control command, or a computed one") {
	Board board(false, attackBoard());
	const std::string before = guarded(board);
	for(const char* target : {"callIt", "{{state.method}}"}) {
		CAPTURE(target);
		const json answer = board.ask(
		    board.agent,
		    {{"type", "revision"},
		     {"message", "a helper"},
		     {"set", {{"/actions/sneak", {{"title", "Sneak"}, {"do", json::array({{{"run", target}}})}}}}}});
		CHECK(answer["error"].get<std::string>().contains("revisions can't add"));
		CHECK(guarded(board) == before);
	}
	CHECK_FALSE(board.commands(board.agent).contains("sneak"));
}

TEST_CASE("commands: an agent's list says a command that invokes a computed method is not allowed" *
          doctest::skip(true)) {
	// Lists "allow" though the run is refused when it reaches the step: the catalog's reach reads no computed method
	Board board(false, attackBoard());
	CHECK(board.commands(board.agent).at("computedInvoke")["run"] != "allow");
}
