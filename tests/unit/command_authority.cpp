/// @file
/// @brief A command a peer runs executes under the runner's authority, never the person's: every control and review
/// step, and every native action, is refused wherever the run reaches it (another action, a program's function,
/// advice, a hook, a pressed button, a data step's callback) and whenever (a timer, a debounce, a service's answer),
/// even with no check before the run; the same commands still work for the person.

#include "CommandRunApp.hpp"
#include "Programs.hpp"
#include "StepRegistry.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using commandrun::App;
using commandrun::command;
using commandrun::json;
using commandrun::screen;

namespace {

/// The words a refusal of the runner's code holds.
constexpr const char* ledHere = "code by \"agent/x\" led here";

/// @brief Whether a run was refused while it ran, with a reason holding a text, and rolled back.
/// @param outcome What happened.
/// @param text The text.
/// @return true if so.
bool refusedWith(const affresco::CommandOutcome& outcome, const std::string& text) {
	return outcome.started && outcome.failed && outcome.refusal && outcome.refusal->contains(text);
}

/// @brief A JSON screen with one command, "go", doing some steps, and a state counter.
/// @param steps What "go" does.
/// @param actions Other actions.
/// @return The document.
json goScreen(const json& steps, json actions = json::object()) {
	actions["go"] = command(steps);
	return screen({{"n", 0}, {"after", false}}, actions);
}

/// The person's program: what the peer's runs try to reach through it.
constexpr const char* program = R"lisp(
(defn hit-it "Reaches the application." [] (call "hit"))
(defn noted "Notes it." [] (set "state.noted" true))
(advice noted :before hit-it)
(defhook reached "Reached.")
(add-hook reached hit-it)
(defcommand direct "Calls the application itself." :title "D" [] (call "hit"))
(defcommand through "Through a function." :title "T" [] (hit-it))
(defcommand advised "Through advice." :title "A" [] (noted))
(defcommand hooked "Through a hook." :title "H" [] (run-hooks reached))
(defcommand later "Through a timer." :title "L" [] (after 50 hit-it))
(defcommand judge "Through a review step." :title "J" [] (evaluate "(+ 1 2)"))
(defkey "ctrl+d" direct)
(defkey "ctrl+t" through)
(defscreen main :state {:noted false} (panel#root))
)lisp";

} // namespace

TEST_CASE("format: a command run by a peer is refused every control and review step, as the runner's") {
	std::vector<std::string> tried;
	for(const affresco::format::StepGroup& group : affresco::format::builtInSteps()->groups())
		for(const affresco::format::StepType& type : group.steps) {
			const affresco::format::StepKind kind = affresco::format::stepKind(type.name);
			if(kind != affresco::format::StepKind::Control && kind != affresco::format::StepKind::Review) continue;
			CAPTURE(type.name);
			App app(goScreen(json::array({{{type.name, 1}}, {{"set", "state.after"}, {"to", true}}})), false);
			const affresco::CommandOutcome outcome = app.run("go");
			CHECK(refusedWith(outcome, ledHere));
			CHECK(outcome.refusal->contains("\"" + type.name + "\""));
			CHECK(app.state("after") == false);
			tried.push_back(type.name);
		}
	for(const char* step : {"accept", "reject", "undo", "revert", "call", "saveFile", "signIn", "respond", "evaluate"})
		CHECK(std::ranges::find(tried, step) != tried.end());
}

TEST_CASE("format: a command run by a peer can't allow a run: allowRun is refused, and the run control never hears") {
	App app(goScreen(json::array({{{"allowRun", 1}, {"always", true}}})));
	CHECK(refusedWith(app.run("go"), "\"allowRun\""));
	CHECK(app.control.allowed == 0);
}

TEST_CASE("format: a command run by a peer can't deny a run: denyRun is refused") {
	App app(goScreen(json::array({{{"denyRun", 1}}})));
	CHECK(refusedWith(app.run("go"), "\"denyRun\""));
	CHECK(app.control.denied == 0);
}

TEST_CASE("format: a command run by a peer can't undo a run: undoRun is refused") {
	App app(goScreen(json::array({{{"set", "state.n"}, {"to", 1}}}),
	                 {{"again", command(json::array({{{"undoRun", 1}}}))}}));
	CHECK_FALSE(app.run("go").failed);
	CHECK(refusedWith(app.run("again"), "\"undoRun\""));
	CHECK(app.state("n") == 1);
	CHECK(app.control.finished.empty());
}

TEST_CASE("format: a command run by a peer can't run a native action, and the native code never runs") {
	App app(goScreen(json::array({{{"run", "native"}}})));
	CHECK(refusedWith(app.run("go"), "\"native\""));
	CHECK(app.natives == 0);
	app.act("go"); // the person's own run of it
	CHECK(app.natives == 1);
}

TEST_CASE("format: a command run by a peer is refused a control step in another action it runs") {
	App app(goScreen(json::array({{{"run", "inner"}}}), {{"inner", json::array({{{"call", "hit"}}})}}));
	CHECK(refusedWith(app.run("go"), ledHere));
	CHECK(app.hits == 0);
}

TEST_CASE("format: a command run by a peer is refused a control step in another command it runs") {
	App app(goScreen(json::array({{{"run", "other"}}}), {{"other", command(json::array({{{"call", "hit"}}}))}}));
	CHECK(refusedWith(app.run("go"), ledHere));
	CHECK(app.hits == 0);
}

TEST_CASE("format: a command run by a peer is refused a control step of a command whose do names an action") {
	json document = goScreen(json::array());
	document["actions"]["go"]["do"] = "inner";
	document["actions"]["inner"] = json::array({{{"call", "hit"}}});
	App app(document);
	CHECK(refusedWith(app.run("go"), ledHere));
	CHECK(app.hits == 0);
}

TEST_CASE("format: a command run by a peer is refused a control step within if, forEach and while") {
	App app(
	    goScreen(json::array({{{"forEach", "{{[1, 2]}}"},
		                       {"do", json::array({{{"if", true}, {"then", json::array({{{"call", "hit"}}})}}})}}})));
	CHECK(refusedWith(app.run("go"), ledHere));
	CHECK(app.hits == 0);
}

TEST_CASE("format: a command run by a peer is refused a control step in a record write's then") {
	App app(screen({{"n", 0}},
	               {{"go", command(json::array({{{"insert", "notes"},
	                                             {"value", {{"title", "a"}}},
	                                             {"then", json::array({{{"call", "hit"}}})}}}))}},
	               {{"models", commandrun::notesModel()}}));
	CHECK(refusedWith(app.run("go"), ledHere));
	CHECK(app.hits == 0);
	CHECK(app.records("notes") == 0); // rolled back with the run
}

TEST_CASE("format: a command run by a peer is refused a control step its after step runs later") {
	App app(goScreen(json::array({{{"after", 100}, {"do", json::array({{{"call", "hit"}}})}}})));
	CHECK_FALSE(app.run("go").failed); // after itself is free
	app.wait(150);
	CHECK(app.hits == 0);
	CHECK(app.reported(ledHere));
}

TEST_CASE("format: a command run by a peer is refused a control step its debounce runs later") {
	App app(goScreen(json::array({{{"debounce", 100}, {"do", json::array({{{"call", "hit"}}})}}})));
	CHECK_FALSE(app.run("go").failed);
	app.wait(150);
	CHECK(app.hits == 0);
	CHECK(app.reported(ledHere));
}

TEST_CASE("format: a command run by a peer is refused a control step a service's answer runs after it returned") {
	const commandrun::TemporaryFolder folder;
	App app(screen({{"n", 0}, {"local", ""}},
	               {{"go",
	                 command(json::array({{{"invoke", "files.read"},
	                                       {"with", {{"path", "note.txt"}}},
	                                       {"onDone", json::array({{{"set", "state.local"}, {"to", "{{result.text}}"}},
	                                                               {{"call", "hit"}}})}}}))}},
	               {{"services", {{"files", {{"provider", "fs"}, {"root", "work"}, {"readOnly", true}}}}}}),
	        true, folder.path);
	CHECK_FALSE(app.run("go").failed);
	REQUIRE(app.executor.pumpUntil([&app] { return app.state("local") == "inside"; }));
	CHECK(app.hits == 0);
	CHECK(app.reported(ledHere));
}

TEST_CASE("format: a command run by a peer is refused a control step a service's error runs after it returned") {
	const commandrun::TemporaryFolder folder;
	App app(screen({{"n", 0}, {"failed", false}},
	               {{"go", command(json::array({{{"invoke", "files.read"},
	                                             {"with", {{"path", "missing.txt"}}},
	                                             {"onError", json::array({{{"set", "state.failed"}, {"to", true}},
	                                                                      {{"call", "hit"}}})}}}))}},
	               {{"services", {{"files", {{"provider", "fs"}, {"root", "work"}, {"readOnly", true}}}}}}),
	        true, folder.path);
	CHECK_FALSE(app.run("go").failed);
	REQUIRE(app.executor.pumpUntil([&app] { return app.state("failed") == true; }));
	CHECK(app.hits == 0);
	CHECK(app.reported(ledHere));
}

TEST_CASE("format: a command run by a peer is refused a control step of a Lisp command") {
	App app(App::lisp(program));
	CHECK(refusedWith(app.run("direct"), ledHere));
	CHECK(app.hits == 0);
}

TEST_CASE("format: a command run by a peer is refused a control step of the person's function it calls") {
	App app(App::lisp(program));
	CHECK(refusedWith(app.run("through"), "led here"));
	CHECK(app.hits == 0);
}

TEST_CASE("format: a command run by a peer is refused a control step of advice on what it calls") {
	App app(App::lisp(program));
	static_cast<void>(app.run("advised")); // failed advice is reported, and what it advises goes on
	CHECK(app.hits == 0);
	CHECK(app.reported("led here"));
	CHECK(app.state("noted") == true);
}

TEST_CASE("format: a command run by a peer is refused a control step of a hook it runs") {
	App app(App::lisp(program));
	static_cast<void>(app.run("hooked")); // a failed hook is reported, and the others go on
	CHECK(app.hits == 0);
	CHECK(app.reported("led here"));
}

TEST_CASE("format: a command run by a peer is refused a control step a Lisp timer runs later") {
	App app(App::lisp(program));
	CHECK_FALSE(app.run("later").failed);
	app.wait(100);
	CHECK(app.hits == 0);
	CHECK(app.reported("led here"));
}

TEST_CASE("format: a command run by a peer is refused a review step of a Lisp command") {
	App app(App::lisp(program));
	CHECK(refusedWith(app.run("judge"), "\"evaluate\""));
}

TEST_CASE("format: a command run by a peer runs a command another agent wrote, its free steps working") {
	App app(App::lisp(program));
	const std::optional<std::string> refused = app.repository.commit(
	    {{"author", "agent/y"},
		 {"message", "m"},
		 {"set", {{"/program/app.lisp/by-y", "(defcommand by-y \"\" :title \"Y\" [] (set \"state.noted\" true))"}}}});
	INFO(refused.value_or(""));
	REQUIRE_FALSE(refused.has_value());
	const affresco::CommandOutcome outcome = app.run("by-y");
	CHECK_FALSE(outcome.failed);
	CHECK(app.state("noted") == true);
}

TEST_CASE("format: a command run by a peer is refused a Lisp function a JSON command calls") {
	json document = App::lisp(program);
	document["actions"]["go"] = command(json::array({{{"lisp", "hit-it"}}}));
	App app(document);
	CHECK(refusedWith(app.run("go"), "led here"));
	CHECK(app.hits == 0);
}

TEST_CASE("format: a command run by a peer ends, and the person's own commands work again at once") {
	App app(goScreen(json::array({{{"call", "hit"}}})));
	CHECK(refusedWith(app.run("go"), ledHere));
	CHECK(app.hits == 0);
	app.act("go");
	CHECK(app.hits == 1);
	CHECK(app.errors.size() == 1); // only the run's refusal
}

TEST_CASE("format: a command run by a peer leaves the person's keys to the same Lisp commands working") {
	App app(App::lisp(program));
	CHECK(app.run("direct").failed);
	app.press(affresco::Key::D);
	app.press(affresco::Key::T);
	CHECK(app.hits == 2);
}

TEST_CASE("format: a command run by a peer never runs under the person's name") {
	App app(goScreen(json::array({{{"call", "hit"}}})));
	for(const char* author : {"user", "file", "user/someone", ""}) {
		CAPTURE(author);
		const affresco::CommandOutcome outcome = app.run("go", json::object(), author);
		CHECK_FALSE(outcome.started);
		REQUIRE(outcome.refusal);
		CHECK(outcome.refusal->contains("never the person's"));
	}
	CHECK(app.hits == 0);
}

TEST_CASE("format: a command run by a peer runs only a command of the screen, not a plain action or a function") {
	App app(goScreen(json::array(), {{"plain", json::array({{{"call", "hit"}}})}}));
	for(const char* name : {"plain", "native", "nothing"}) {
		CAPTURE(name);
		const affresco::CommandOutcome outcome = app.run(name);
		CHECK_FALSE(outcome.started);
		REQUIRE(outcome.refusal);
		CHECK(outcome.refusal->contains("is not a command of this screen"));
	}
	CHECK(app.hits == 0);
	CHECK(app.natives == 0);
}

TEST_CASE("format: a command run by a peer needs a window") {
	App app(goScreen(json::array({{{"set", "state.n"}, {"to", 1}}})), true, {}, false);
	const affresco::CommandOutcome outcome = app.runtime->runCommandAs({1, "agent/x", "go", json::object()});
	CHECK_FALSE(outcome.started);
	CHECK(outcome.refusal == "this screen has no window");
	CHECK(app.state("n") == 0);
}
