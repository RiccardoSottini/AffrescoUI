/// @file
/// @brief End-to-end tests of the commands as tools, through the real stack: a headless Screen, its version control and
/// run rules, and two peers (an agent and a reviewer, "studio") speaking the live protocol over in-memory channels.
/// The same board app is written in JSON and in Lisp (CommandBoard.hpp), and both must answer alike.

#include "CommandBoard.hpp"

#include <affresco/Screen.hpp>
#include <affresco/automation/Scenarios.hpp>
#include <affresco/render/Input.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <map>
#include <ranges>
#include <string>

using commandboard::Board;
using commandboard::boardJson;
using commandboard::json;
using commandboard::shownTexts;

TEST_CASE("commands: an agent lists the board's commands, each with the verdict a run would meet, and why") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		const std::map<std::string, json> listed = board.commands(board.agent);
		REQUIRE(listed.size() == 6);
		CHECK(listed.at("moveCard")["run"] == "allow");
		CHECK(listed.at("moveCard")["arguments"]["to"] == json{{"enum", {"todo", "doing", "done"}}});
		CHECK(listed.at("addCard")["run"] == "ask");
		CHECK(listed.at("addCard")["reason"] == "no run rule allows 'addCard' for 'agent'; a rule \"agent(addCard)\" "
		                                        "in permissions.run.allow would");
		CHECK(listed.at("clearBoard")["run"] == "deny");
		CHECK(listed.at("clearBoard")["reason"] == "denied by the run rule '*(clearBoard)'");
		CHECK(listed.at("exportBoard")["run"] == "deny");
		const std::string exported = listed.at("exportBoard")["reason"].get<std::string>();
		const std::string accepted = listed.at("acceptAll")["reason"].get<std::string>();
		CHECK(listed.at("acceptAll")["run"] == "deny");
		if(lisp) { // the program's function is what reaches it
			CHECK(exported == "'exportBoard' reaches a control or a review step in the function 'exportBoard', "
			                  "which only the person may run");
			CHECK(accepted.starts_with("'acceptAll' reaches a control or a review step in the function 'acceptAll'"));
		} else {
			CHECK(exported == "'exportBoard' reaches the control step \"call\", which only the person may run");
			CHECK(accepted == "'acceptAll' reaches the control step \"accept\", which only the person may run");
		}
		CHECK(listed.at("saveBoard")["run"] == "person");
		CHECK(listed.at("saveBoard")["reason"].get<std::string>().starts_with(
		    "'saveBoard' needs the person's yes each time: "));
		const std::map<std::string, json> reviewer = board.commands(board.studio);
		CHECK(reviewer.at("addCard")["run"] == "allow");
		CHECK(reviewer.at("clearBoard")["run"] == "deny"); // a deny rule binds reviewers too
	}
}

TEST_CASE("commands: an allowed run runs as the agent, answers what changed, and is kept in the runs log") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		const json ran = board.run("moveCard", {{"card", "c1"}, {"to", "done"}}, "claude");
		CHECK(ran["type"] == "ran");
		CHECK(ran["failed"] == false);
		CHECK(ran["undoable"] == "state");
		CHECK(ran["changed"] ==
		      json::parse(R"([{"op": "replace", "path": "/state/cards/0/column", "value": "done"}])"));
		CHECK(ran["errors"] == json::array());
		CHECK(board.column("c1") == "done");
		const json runs = Board::runs(board.studio);
		REQUIRE(runs.size() == 1);
		CHECK(runs[0]["id"] == ran["run"]);
		CHECK(runs[0]["author"] == "agent/claude");
		CHECK(runs[0]["command"] == "moveCard");
		CHECK(runs[0]["state"] == "ran");
		CHECK(runs[0]["arguments"] == json{{"card", "c1"}, {"to", "done"}});
	}
}

TEST_CASE("commands: wrong arguments are refused before any rule, naming the argument and never its value") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		CHECK(board.run("moveCard", {{"card", "c1"}, {"to", "later"}})["error"] ==
		      "moveCard: to: expected one of todo, doing, done");
		CHECK(board.run("moveCard", {{"card", "c1"}, {"colum", "done"}})["error"] ==
		      "moveCard: no argument 'colum' (it takes card, to)");
		CHECK(board.run("moveCard", {{"to", "done"}})["error"] == "moveCard: card is missing");
		CHECK(board.run("moveCard", {{"card", 7}, {"to", "done"}})["error"].get<std::string>().starts_with(
		    "moveCard: card: expected"));
		CHECK(board.run("moveCards")["error"].get<std::string>().starts_with("no command 'moveCards'"));
		CHECK(board.column("c1") == "todo");
		CHECK(Board::runs(board.studio).empty()); // nothing was begun
	}
}

TEST_CASE("commands: a run no rule allows waits, and runs as the agent once the reviewer allows it, answered again") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		const json waiting = board.run("addCard", {{"title", "Ship"}});
		CHECK(waiting["type"] == "waiting");
		CHECK(waiting["expires"] == 120);
		CHECK(waiting["reason"].get<std::string>().starts_with("no run rule allows 'addCard'"));
		const int id = board.nextId;
		CHECK(board.state("cards").size() == 2);
		REQUIRE(Board::runs(board.studio).size() == 1);
		CHECK(Board::runs(board.studio)[0]["state"] == "waiting");
		// The agent can't allow its own run, by any name
		CHECK(board.decide(board.agent, "allowRun", waiting["run"])["type"] == "rejected");
		CHECK(board.ask(board.agent, {{"type", "allowRun"}, {"run", waiting["run"]}, {"author", "studio"}})["type"] ==
		      "rejected");
		CHECK(board.state("cards").size() == 2);
		CHECK(board.decide(board.studio, "allowRun", waiting["run"])["type"] == "done");
		const json ran = Board::answer(board.agent, id, 1);
		CHECK(ran["type"] == "ran");
		CHECK(ran["run"] == waiting["run"]);
		REQUIRE(board.state("cards").size() == 3);
		CHECK(board.state("cards")[2]["title"] == "Ship");
		CHECK(Board::runs(board.studio)[0]["state"] == "ran");
		CHECK(Board::runs(board.studio)[0]["author"] == "agent");
	}
}

TEST_CASE("commands: a run the reviewer denies is answered rejected with the reason, and changes nothing") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		const json waiting = board.run("addCard", {{"title", "Ship"}});
		const int id = board.nextId;
		CHECK(board.decide(board.studio, "denyRun", waiting["run"])["type"] == "done");
		const json refused = Board::answer(board.agent, id, 1);
		CHECK(refused["type"] == "rejected");
		CHECK(refused["error"] == "not today");
		CHECK(board.state("cards").size() == 2);
		CHECK(Board::runs(board.studio)[0]["state"] == "denied");
		CHECK(board.decide(board.studio, "allowRun", waiting["run"])["type"] == "rejected"); // decided already
	}
}

TEST_CASE("commands: a run nobody answers expires after 120 s on the window's clock") {
	Board board;
	const json waiting = board.run("addCard", {{"title", "Ship"}});
	const int id = board.nextId;
	board.wait(110);
	CHECK(Board::answer(board.agent, id, 1)["type"] == "none");
	board.wait(12);
	const json expired = Board::answer(board.agent, id, 1);
	CHECK(expired["type"] == "rejected");
	CHECK(expired["error"] == "nobody answered in 120 s");
	CHECK(Board::runs(board.studio)[0]["state"] == "expired");
	CHECK(board.state("cards").size() == 2);
}

TEST_CASE("commands: allowed always, the agent runs it again without asking, until the grant is revoked") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		const json waiting = board.run("addCard", {{"title", "One"}});
		CHECK(board.decide(board.studio, "allowRun", waiting["run"], true)["type"] == "done");
		CHECK(board.commands(board.agent).at("addCard")["run"] == "allow");
		CHECK(board.run("addCard", {{"title", "Two"}})["type"] == "ran");
		CHECK(board.state("cards").size() == 4);
		const std::string rule = "agent(run:addCard)";
		json grants;
		for(const json& sent : std::ranges::reverse_view(board.studio.sent))
			if(sent.value("type", "") == "review") {
				grants = sent.value("runGrants", json::array());
				break;
			}
		CAPTURE(grants);
		REQUIRE(grants.size() == 1);
		CHECK(grants[0]["rule"] == rule);
		CHECK(board.ask(board.agent, {{"type", "revokeRun"}, {"rule", rule}})["type"] == "rejected");
		CHECK(board.ask(board.studio, {{"type", "revokeRun"}, {"rule", rule}})["type"] == "done");
		CHECK(board.run("addCard", {{"title", "Three"}})["type"] == "waiting");
	}
}

TEST_CASE("commands: the person decides a waiting run with the screen's own allowRun and denyRun steps") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		board.run("addCard", {{"title", "Yes"}});
		const int first = board.nextId;
		board.key(affresco::Key::F7); // the person's own key: allowLast
		CHECK(Board::answer(board.agent, first, 1)["type"] == "ran");
		CHECK(board.state("cards").size() == 3);
		board.run("addCard", {{"title", "No"}});
		const int second = board.nextId;
		board.key(affresco::Key::F8); // denyLast
		CHECK(Board::answer(board.agent, second, 1)["error"] == "not today");
		CHECK(board.state("cards").size() == 3);
		CHECK(board.errors.empty());
	}
}

TEST_CASE("commands: a deny rule refuses a run for everyone, and a control step's command is refused naming it") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		CHECK(board.run("clearBoard")["error"] == "denied by the run rule '*(clearBoard)'");
		CHECK(board.ask(board.studio,
		                {{"type", "runCommand"}, {"command", "clearBoard"}, {"arguments", json::object()}})["error"] ==
		      "denied by the run rule '*(clearBoard)'");
		CHECK(board.state("cards").size() == 2);
		const json exported = board.run("exportBoard");
		CHECK(exported["type"] == "rejected");
		CHECK(exported["error"].get<std::string>().ends_with("which only the person may run"));
		CHECK(board.exports == 0);
		// Even with a proposal waiting, accept is refused: the agent's own proposal stays open
		const json proposed =
		    board.ask(board.agent, {{"type", "revision"}, {"message", "retitle"}, {"set", {{"/state/note", "mine"}}}});
		CAPTURE(proposed);
		const json accepted = board.run("acceptAll");
		CHECK(accepted["error"].get<std::string>().ends_with("which only the person may run"));
		CHECK(board.screen.proposals().size() == 1);
		CHECK(board.state("note") == "");
	}
}

TEST_CASE("commands: a run that writes through a service asks the person in the window's own question, each time") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		const json waiting = board.run("saveBoard", {{"name", "board.txt"}}, "claude");
		CHECK(waiting["type"] == "waiting");
		const int id = board.nextId;
		REQUIRE(board.window->confirmation() != nullptr);
		const std::string shown = shownTexts(board.window->confirmation());
		CAPTURE(shown);
		CHECK(shown.contains("Run “Save board”?"));
		CHECK(shown.contains("agent/claude asks to run it."));
		CHECK(shown.contains("files.write"));
		CHECK(shown.contains("board.txt"));
		// A reviewer can't say yes for the person, nor can the agent; always is never offered
		CHECK(board.decide(board.studio, "allowRun", waiting["run"])["error"] == "only the person may allow this run");
		CHECK(board.decide(board.agent, "allowRun", waiting["run"])["type"] == "rejected");
		board.press("Don't run");
		CHECK(Board::answer(board.agent, id, 1)["error"] == "the person said no");
		CHECK_FALSE(std::filesystem::exists(board.folder.path / "work" / "board.txt"));
		// Asked again, the next time: a yes is never remembered
		board.run("saveBoard", {{"name", "board.txt"}});
		const int again = board.nextId;
		REQUIRE(board.window->confirmation() != nullptr);
		board.press("Run");
		CHECK(Board::answer(board.agent, again, 1)["type"] == "ran");
		CHECK(board.executor.pumpUntil(
		    [&board] { return std::filesystem::exists(board.folder.path / "work" / "board.txt"); }));
		CHECK(board.run("saveBoard", {{"name", "other.txt"}})["type"] == "waiting");
		CHECK(board.window->confirmation() != nullptr);
	}
}

TEST_CASE("commands: in plan mode an agent's run is denied, saying to describe it instead; a reviewer's runs") {
	Board board;
	board.screen.setMode(affresco::Mode::Plan);
	CHECK(board.commands(board.agent).at("moveCard")["run"] == "deny");
	CHECK(board.run("moveCard", {{"card", "c1"}, {"to", "done"}})["error"] ==
	      "plan mode: commands don't run; say what you would run");
	CHECK(board.column("c1") == "todo");
	CHECK(board.ask(board.studio, {{"type", "runCommand"},
	                               {"command", "moveCard"},
	                               {"arguments", {{"card", "c1"}, {"to", "done"}}}})["type"] == "ran");
}

TEST_CASE("commands: while a proposal is previewed, a run is refused: commands run on main only") {
	Board board;
	board.ask(board.agent, {{"type", "revision"}, {"message", "note"}, {"set", {{"/state/note", "x"}}}});
	REQUIRE(board.screen.proposals().size() == 1);
	REQUIRE(board.screen.previewProposal(board.screen.proposals()[0].id));
	CHECK(board.run("moveCard", {{"card", "c1"}, {"to", "done"}})["error"] ==
	      "a proposal is being previewed: commands run on main only");
	board.screen.endPreview();
	CHECK(board.run("moveCard", {{"card", "c1"}, {"to", "done"}})["type"] == "ran");
}

TEST_CASE("commands: undoRun puts back what a run changed, and is refused once the person changed it since") {
	for(const bool lisp : {false, true}) {
		CAPTURE(lisp);
		Board board(lisp);
		const json ran = board.run("moveCard", {{"card", "c1"}, {"to", "done"}});
		CHECK(board.ask(board.agent, {{"type", "undoRun"}, {"run", ran["run"]}})["type"] == "undone");
		CHECK(board.column("c1") == "todo");
		const json again = board.run("moveCard", {{"card", "c2"}, {"to", "done"}});
		board.act("touch");
		const json refused = board.ask(board.agent, {{"type", "undoRun"}, {"run", again["run"]}});
		CHECK(refused["type"] == "rejected");
		CHECK(refused["error"].get<std::string>().contains("/state/cards"));
		CHECK(board.column("c2") == "todo");
	}
}

TEST_CASE("commands: a run's changes are judged by the runner's data rules: a never path is refused") {
	json document = json::parse(boardJson);
	document["permissions"]["data"] = json::array({{{"author", "agent"}, {"never", {"/state/note"}}}});
	document["actions"]["noteIt"] = {{"title", "Note"}, {"do", json::array({{{"set", "state.note"}, {"to", "x"}}})}};
	document["permissions"]["run"]["allow"].push_back("agent(noteIt)");
	Board board(false, document.dump());
	const json refused = board.run("noteIt");
	CHECK(refused["type"] == "rejected");
	CHECK(refused["error"].get<std::string>().contains("may never touch /state/note"));
	CHECK(json::parse(board.screen.stateJson())["note"] == "");
}

TEST_CASE("commands: without permissions every run is allowed, but a control step still only the person's") {
	json document = json::parse(boardJson);
	document.erase("permissions");
	Board board(false, document.dump());
	CHECK(board.run("addCard", {{"title", "Free"}})["type"] == "ran");
	CHECK(board.run("clearBoard")["type"] == "ran");
	CHECK(board.run("exportBoard")["error"] == "'exportBoard' reaches the control step \"call\", which only the person "
	                                           "may run");
	CHECK(board.exports == 0);
	CHECK(board.run("saveBoard", {{"name", "a.txt"}})["type"] == "waiting");
}

TEST_CASE("commands: a host that switches runs off lists nothing to run and refuses every run") {
	Board board;
	board.screen.allowCommandRuns(false);
	const std::string off = "AffrescoUI screen: runCommand: this host runs no commands";
	CHECK(board.run("moveCard", {{"card", "c1"}, {"to", "done"}})["error"] == off);
	CHECK(board.ask(board.studio, {{"type", "runCommand"},
	                               {"command", "moveCard"},
	                               {"arguments", {{"card", "c1"}, {"to", "done"}}}})["error"] == off);
	CHECK(board.ask(board.agent, {{"type", "getCommands"}})["type"] == "rejected");
	CHECK(board.column("c1") == "todo");
}

TEST_CASE("commands: a scenario's command step runs a command as its author's peer, or checks the verdict it meets") {
	const commandboard::Folder folder;
	commandboard::writeText(folder.path / "board.screen.json", boardJson);
	affresco::automation::Options options;
	options.pictures = false;
	const auto play = [&](const std::string& steps) {
		const std::string text =
		    R"({"version": 2, "screen": "board.screen.json", "scenarios": [{"name": "s", "steps": )" + steps + "}]}";
		const affresco::automation::Report report = affresco::automation::run(
		    affresco::automation::ScenarioFile::parse(text, folder.path / "board.scenarios.json"), options);
		REQUIRE(report.scenarios.size() == 1);
		std::string problems;
		for(const auto& failure : report.scenarios[0].failures)
			problems += std::to_string(failure.step) + ": " + failure.problem + "\n";
		return problems;
	};
	CHECK(play(R"([{ "command": "moveCard", "with": { "card": "c1", "to": "done" } },
		{ "expect": { "state": "cards[0].column", "is": "done" } },
		{ "command": "clearBoard", "verdict": "deny" },
		{ "command": "addCard", "verdict": "ask" },
		{ "command": "addCard", "as": "studio", "with": { "title": "New" } },
		{ "expect": { "state": "cards[2].title", "is": "New" } }])") == "");
	CHECK(play(R"([{ "command": "addCard", "verdict": "allow" }])")
	          .contains("1: 'addCard' would ask (no run rule allows 'addCard'"));
	CHECK(play(R"([{ "command": "addCard", "with": { "title": "x" } }])").starts_with("1: 'addCard' would ask"));
	CHECK(play(R"([{ "command": "moveCard", "with": { "card": "c1" } }])").contains("'moveCard' was refused: "));
	CHECK(play(R"([{ "command": "moveCrad" }])").contains("no command 'moveCrad' that 'agent' sees"));
	CHECK(play(R"([{ "command": "acceptAll" }])").contains("'acceptAll' is denied ("));
}
