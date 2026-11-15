/// @file
/// @brief What a command run by a peer reports and keeps: its arguments and event reach the steps, its errors and
/// what it changed come back (exactly, or as paths past the limits), a run the data rules refuse is rolled back whole,
/// its records are counted and written under the runner's name, and a run is undone only while what it changed holds.

#include "CommandRunApp.hpp"

#include <doctest/doctest.h>

#include <string>

using commandrun::App;
using commandrun::command;
using commandrun::json;
using commandrun::screen;

namespace {

/// @brief A screen of notes whose commands a test gives, with the state {n: 1, list: [1]}.
/// @param actions The actions.
/// @param extra Other sections.
/// @return The document.
json notesScreen(const json& actions, json extra = json::object()) {
	extra["models"] = commandrun::notesModel();
	return screen({{"n", 1}, {"list", json::array({1})}, {"other", 0}}, actions, extra);
}

/// The steps of a command that writes one note.
const json insertNote = json::array({{{"insert", "notes"}, {"value", {{"title", "a"}}}}});

/// @brief The journal's newest entry's author.
/// @param app The app.
/// @return It, or empty.
std::string newestAuthor(const App& app) {
	const std::vector<affresco::data::JournalEntry> entries = app.runtime->dataStore().journal(1);
	return entries.empty() ? std::string() : entries.front().author;
}

} // namespace

TEST_CASE("format: a command run gives its arguments to a JSON command's steps as variables") {
	App app(screen({{"got", nullptr}}, {{"go", command(json::array({{{"set", "state.got"}, {"to", "{{title}}"}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go", {{"title", "new"}});
	CHECK(outcome.started);
	CHECK_FALSE(outcome.failed);
	CHECK(app.state("got") == "new");
}

TEST_CASE("format: a command run gives its arguments to a Lisp command's parameters by name") {
	App app(App::lisp(R"lisp(
(defcommand add "Adds." :title "Add" [title n] (set "state.got" (str title n)))
(defscreen main :state {:got nil} (panel#root))
)lisp"));
	CHECK_FALSE(app.run("add", {{"n", 2}, {"title", "t"}}).failed);
	CHECK(app.state("got") == "t2");
}

TEST_CASE("format: a command run's event names the run and its runner") {
	App app(screen({{"ev", nullptr}}, {{"go", command(json::array({{{"set", "state.ev"}, {"to", "{{event}}"}}}))}}));
	CHECK_FALSE(app.run("go").failed);
	CHECK(app.state("ev") == json{{"type", "command"}, {"run", 1}, {"author", "agent/x"}});
}

TEST_CASE("format: a command run reports exactly what a set changed, under /state") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 2}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(outcome.changed == json::parse(R"([{"op": "replace", "path": "/state/n", "value": 2}])"));
	CHECK(outcome.changes == 1);
	CHECK_FALSE(outcome.truncated);
	CHECK((outcome.undoable == affresco::Undoable::State));
}

TEST_CASE("format: a command run reports exactly what a push changed") {
	App app(notesScreen({{"go", command(json::array({{{"push", "state.list"}, {"value", 2}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(outcome.changed == json::parse(R"([{"op": "add", "path": "/state/list/-", "value": 2}])"));
	CHECK(outcome.changes == 1);
}

TEST_CASE("format: a command run reports exactly what a remove changed") {
	App app(notesScreen({{"go", command(json::array({{{"remove", "state.other"}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(outcome.changed == json::parse(R"([{"op": "remove", "path": "/state/other"}])"));
	CHECK(app.state("other").is_null());
}

TEST_CASE("format: a command run that changes nothing reports nothing, and can't be undone") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 1}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(outcome.changed.empty());
	CHECK(outcome.changes == 0);
	CHECK((outcome.undoable == affresco::Undoable::None));
	CHECK(app.undo(1).refusal == "run 1 changed nothing");
}

TEST_CASE("format: a command run past 64 operations reports only the paths, and counts them all") {
	App app(screen(
	    {{"n", 0}, {"m", json::object()}},
	    {{"go", command(json::array({{{"forEach", "{{range(100)}}"},
		                              {"as", "i"},
		                              {"do", json::array({{{"set", "state.m.k{{i}}"}, {"to", "{{i}}"}}})}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	REQUIRE_FALSE(outcome.failed);
	CHECK(outcome.truncated);
	CHECK(outcome.changes == 100);
	REQUIRE_FALSE(outcome.changed.empty());
	for(const json& operation : outcome.changed) {
		CHECK_FALSE(operation.contains("value"));
		CHECK(operation["path"].get<std::string>().starts_with("/state/m/"));
	}
}

TEST_CASE("format: a command run past 16 KiB reports only the paths") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", std::string(20000, 'x')}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(outcome.truncated);
	CHECK(outcome.changed == json::parse(R"([{"op": "replace", "path": "/state/n"}])"));
	CHECK(outcome.changes == 1);
}

TEST_CASE("format: a command run the data rules refuse by effect is rolled back whole, and the screen shows it") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 5}},
	                                                 {{"insert", "notes"}, {"value", {{"title", "a"}}}}}))}}));
	app.control.denial = [](std::string_view, const json&, const json& after) -> std::optional<std::string> {
		if(after["n"] == 5) return "data rule: 'agent/x' may not write /state/n";
		return std::nullopt;
	};
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(outcome.started);
	CHECK(outcome.failed);
	CHECK(outcome.refusal == "data rule: 'agent/x' may not write /state/n");
	CHECK(app.state("n") == 1);
	CHECK(app.records("notes") == 0);
	const auto* shown = dynamic_cast<const affresco::Label*>(app.runtime->find("shown"));
	REQUIRE(shown != nullptr);
	CHECK(shown->text() == "1");
}

TEST_CASE("format: a command run is judged by effect with its runner and the state before and after") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 2}}}))}}));
	std::string author;
	json before;
	json after;
	app.control.denial = [&](std::string_view who, const json& was, const json& is) -> std::optional<std::string> {
		author = who;
		before = was;
		after = is;
		return std::nullopt;
	};
	CHECK_FALSE(app.run("go", json::object(), "agent/y").failed);
	CHECK(author == "agent/y");
	CHECK(before["n"] == 1);
	CHECK(after["n"] == 2);
}

TEST_CASE("format: a command run ended by an error is rolled back whole, and the error comes back") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 7}},
	                                                 {{"insert", "notes"}, {"value", {{"title", "a"}}}},
	                                                 {{"run", "nothing-of-that-name"}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(outcome.failed);
	REQUIRE(outcome.refusal);
	CHECK(app.state("n") == 1);
	CHECK(app.records("notes") == 0);
	REQUIRE_FALSE(outcome.errors.empty());
	CHECK(outcome.errors.back() == *outcome.refusal);
}

TEST_CASE("format: a command run counts the records it wrote per collection, never giving one") {
	App app(notesScreen({{"go", command(json::array({{{"insert", "notes"}, {"value", {{"title", "a"}}}},
	                                                 {{"insert", "notes"}, {"value", {{"title", "b"}}}},
	                                                 {{"update", "notes"}, {"id", "1"}, {"fields", {{"title", "c"}}}},
	                                                 {{"delete", "notes"}, {"id", "2"}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	REQUIRE_FALSE(outcome.failed);
	CHECK(outcome.records == json::parse(R"({"notes": {"inserted": 2, "updated": 1, "deleted": 1}})"));
	CHECK(outcome.changes == 4);
	CHECK((outcome.undoable == affresco::Undoable::State));
}

TEST_CASE("format: a command run writes its records under the runner's name") {
	App app(notesScreen({{"go", command(insertNote)}}));
	CHECK_FALSE(app.run("go", json::object(), "agent/y").failed);
	CHECK(newestAuthor(app) == "agent/y");
}

TEST_CASE("format: a command run's timer writes its records later under the runner's name too") {
	App app(notesScreen({{"go", command(json::array({{{"after", 50}, {"do", insertNote}}}))}}));
	CHECK_FALSE(app.run("go").failed);
	app.wait(100);
	CHECK(app.records("notes") == 1);
	CHECK(newestAuthor(app) == "agent/x");
}

TEST_CASE("format: a command run is refused a record write the data rules keep from its runner") {
	App app(
	    notesScreen({{"go", command(insertNote)}},
		            {{"permissions", {{"data", json::array({{{"author", "agent"}, {"never", {"/data/notes"}}}})}}}}));
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(app.records("notes") == 0);
	CHECK(outcome.records.empty());
	REQUIRE_FALSE(outcome.errors.empty());
	CHECK(outcome.errors.front().contains("may never touch /data/notes"));
	app.act("go"); // the person may
	CHECK(app.records("notes") == 1);
}

TEST_CASE("format: a command run keeps at most 8 errors, each at most 512 bytes") {
	App app(screen(
	    {{"n", 0}},
	    {{"go", command(json::array({{{"forEach", "{{range(20)}}"},
		                              {"do", json::array({{{"insert", "nothing"},
		                                                   {"value", {{"title", std::string(900, 'y')}}}}})}}}))}}));
	const affresco::CommandOutcome outcome = app.run("go");
	CHECK(outcome.errors.size() == 8);
	for(const std::string& error : outcome.errors) CHECK(error.size() <= 512);
}

TEST_CASE("format: a command run that reaches outside the state is partly undoable") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 2}}, {{"copy", "text"}}}))}}));
	CHECK((app.run("go").undoable == affresco::Undoable::Partial));
}

TEST_CASE("format: a command run of a Lisp command reports what it changed") {
	App app(App::lisp(R"lisp(
(defcommand bump "Bumps." :title "Bump" [] (set "state.n" (inc state.n)))
(defscreen main :state {:n 1} (panel#root))
)lisp"));
	const affresco::CommandOutcome outcome = app.run("bump");
	CHECK(outcome.changed == json::parse(R"([{"op": "replace", "path": "/state/n", "value": 2}])"));
}

TEST_CASE("format: a command run runs one at a time") {
	App app(notesScreen({{"go", command(json::array({{{"run", "nothing-of-that-name"}}}))}}));
	affresco::CommandOutcome nested;
	app.runtime->onError.connect([&](const std::string&) { // while the run reports its error, another one starts
		if(!nested.refusal) nested = app.runtime->runCommandAs({99, "agent/x", "go", json::object()});
	});
	CHECK(app.run("go").failed);
	REQUIRE(nested.refusal);
	CHECK(nested.refusal->contains("a command is running already"));
	CHECK_FALSE(nested.started);
}

TEST_CASE("format: a command run is undone: the state as it was") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 2}}}))}}));
	CHECK_FALSE(app.run("go").failed);
	const affresco::CommandOutcome undone = app.undo(1);
	CHECK_FALSE(undone.refusal);
	CHECK(undone.changes == 1);
	CHECK(app.state("n") == 1);
	REQUIRE(app.control.finished.size() == 1);
	CHECK(app.control.finished[0].run == 1);
	CHECK((app.control.finished[0].state == affresco::RunState::Undone));
}

TEST_CASE("format: a command run is undone whole: an array it grew, a key it added") {
	App app(notesScreen({{"go", command(json::array({{{"push", "state.list"}, {"value", 2}},
	                                                 {{"push", "state.list"}, {"value", 3}},
	                                                 {{"set", "state.added"}, {"to", true}}}))}}));
	CHECK_FALSE(app.run("go").failed);
	CHECK_FALSE(app.undo(1).refusal);
	CHECK(app.state("list") == json::array({1}));
	CHECK_FALSE(app.repository.state().contains("added"));
}

TEST_CASE("format: a command run's undo puts back a key it removed") {
	App app(notesScreen({{"go", command(json::array({{{"remove", "state.other"}}}))}}));
	CHECK_FALSE(app.run("go").failed);
	CHECK_FALSE(app.undo(1).refusal);
	CHECK(app.state("other") == 0);
}

TEST_CASE("format: a command run's undo is refused once what it changed changed since, changing nothing") {
	App app(
	    notesScreen({{
	                     "go",
	                     command(json::array({{{"set", "state.n"}, {"to", 2}}, {{"set", "state.other"}, {"to", 5}}})),
	                 },
		             {"mine", json::array({{{"set", "state.n"}, {"to", 3}}})}}));
	CHECK_FALSE(app.run("go").failed);
	app.act("mine");
	CHECK(app.undo(1).refusal == "run 1 changed /state/n, which changed since");
	CHECK(app.state("n") == 3);
	CHECK(app.state("other") == 5);
	CHECK(app.control.finished.empty());
}

TEST_CASE("format: a command run's undo takes its records away") {
	App app(notesScreen({{"go", command(insertNote)}}));
	CHECK_FALSE(app.run("go").failed);
	CHECK(app.records("notes") == 1);
	CHECK_FALSE(app.undo(1).refusal);
	CHECK(app.records("notes") == 0);
}

TEST_CASE("format: a command run's undo is refused once later writes came after its records") {
	App app(notesScreen({{"go", command(insertNote)}, {"mine", insertNote}}));
	CHECK_FALSE(app.run("go").failed);
	app.act("mine");
	CHECK(app.undo(1).refusal == "run 1's records can't be undone: later writes came after");
	CHECK(app.records("notes") == 2);
}

TEST_CASE("format: a command run's records are a group of their own, apart from the person's event before it") {
	App app(notesScreen({{"go", command(insertNote)}, {"mine", insertNote}}));
	app.runtime->runAction("mine", app.window->root(), json::object()); // its group still open
	CHECK_FALSE(app.run("go").failed);
	CHECK_FALSE(app.undo(1).refusal);
	CHECK(app.records("notes") == 1); // the person's stays
}

TEST_CASE("format: a command run is undone once") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 2}}}))}}));
	CHECK_FALSE(app.run("go").failed);
	CHECK_FALSE(app.undo(1).refusal);
	CHECK(app.undo(1).refusal == "run 1 is undone already");
}

TEST_CASE("format: a command run that never ran can't be undone") {
	App app(notesScreen(json::object()));
	CHECK(app.undo(9).refusal == "there is no run 9");
	CHECK(app.undo(0).refusal == "there is no run 0");
}

TEST_CASE("format: a command run past the newest 32 is too old to undo") {
	App app(notesScreen({{"go", command(json::array({{{"increment", "state.n"}}}))}}));
	for(int at = 0; at < 33; ++at) REQUIRE_FALSE(app.run("go").failed);
	CHECK(app.undo(1).refusal == "run 1 is too old to undo");
	CHECK_FALSE(app.undo(33).refusal);
	CHECK(app.state("n") == 33);
}

TEST_CASE("format: a command run refused while it ran can't be undone") {
	App app(notesScreen({{"go", command(json::array({{{"run", "nothing-of-that-name"}}}))}}));
	CHECK(app.run("go").failed);
	CHECK(app.undo(1).refusal == "there is no run 1");
}

TEST_CASE("format: a command run is undone by the person's undoRun step") {
	App app(notesScreen({{"go", command(json::array({{{"set", "state.n"}, {"to", 2}}}))},
	                     {"undoIt", json::array({{{"undoRun", 1}}})}}));
	CHECK_FALSE(app.run("go").failed);
	app.act("undoIt");
	CHECK(app.errors.empty());
	CHECK(app.state("n") == 1);
}
