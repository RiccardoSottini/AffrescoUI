/// @file
/// @brief Derived values as tracked values: a computed value, a watched path and a query are kept until a place they
/// read changes, evaluated again only then, and their readers are evaluated only when the new value differs; a changed
/// record updates only what reads it, its collection or a query over it.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/format/Runtime.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A document's runtime, built in a headless window.
struct Built {
	ManualClock clock;                                                                ///< The window's time.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(480, 320, clock); ///< Outlives the runtime.
	affresco::format::Rules rules;                                                    ///< The format's rules.
	affresco::vcs::Repository repository;                                             ///< The document.
	affresco::format::Runtime runtime{repository, "."};                               ///< The panels.

	/// @brief Builds a document.
	/// @param document The document.
	explicit Built(const json& document) : repository(rules, document) {
		runtime.build(*window);
		window->render();
	}

	/// @brief Runs an action, then the updates and handlers it queued.
	/// @param action The action.
	/// @param event Its event.
	void run(const char* action, const json& event = json::object()) {
		runtime.runAction(action, window->root(), event);
		window->render();
	}

	/// @brief What the last update cost.
	/// @return It.
	[[nodiscard]] const affresco::format::UpdateStatistics& last() const { return runtime.lastUpdate(); }

	/// @brief A label's text.
	/// @param id Its id.
	/// @return The text.
	std::string text(std::string_view id) {
		auto* label = dynamic_cast<affresco::Label*>(runtime.find(id));
		REQUIRE(label);
		return label->text();
	}

	/// @brief The state.
	/// @return It.
	[[nodiscard]] json state() const { return repository.state(); }
};

/// @brief A computed list of 400 items, shown and named by a watch, and a state key neither reads.
/// @return The document.
json computedScreen() {
	json document = json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "other": 0, "seen": 0 },
		"computed": { "doubled": "{{map(state.items, merge(it, { w: it.v * 2 }))}}" },
		"watch": { "computed.doubled": "noticed" },
		"actions": {
			"noticed": [{ "set": "state.seen", "to": "{{length(event.value)}}" }],
			"other": [{ "set": "state.other", "to": "{{event.v}}" }]
		},
		"root": { "type": "panel", "children": [{ "type": "column", "children": [
			{ "type": "label", "repeat": { "for": "item", "in": "computed.doubled", "key": "{{item.id}}" }, "text": "{{item.w}}" }
		] }] }
	})json");
	for(int item = 0; item < 400; ++item) document["state"]["items"].push_back({{"id", item}, {"v", item}});
	return document;
}

/// @brief Notes and tags, with queries over the notes and one over the tags: generated test data only.
/// @return The document.
json recordsScreen() {
	return json::parse(R"json({
	"format": "affrescoui-screen", "version": 2,
	"models": {
		"notes": { "version": 1, "fields": {
			"title": { "type": "string", "required": true },
			"done": { "type": "boolean", "default": false } },
			"seed": [{ "id": "n1", "title": "First" }, { "id": "n2", "title": "Second", "done": true }] },
		"tags": { "version": 1, "fields": { "name": { "type": "string", "required": true } },
			"seed": [{ "id": "t1", "name": "home" }] } },
	"queries": {
		"open": { "from": "notes", "where": { "done": false } },
		"finished": { "from": "notes", "where": { "done": true } },
		"total": { "from": "notes", "count": true },
		"tagCount": { "from": "tags", "count": true } },
	"state": { "other": 0 },
	"actions": {
		"rename": [{ "update": "notes", "id": "n2", "fields": { "title": "Renamed" } }],
		"add": [{ "insert": "notes", "value": { "title": "Third" } }]
	},
	"root": { "type": "column", "children": [
		{ "type": "label", "id": "first", "text": "{{data.notes[0].title}}" },
		{ "type": "label", "id": "open", "text": "{{length(data.open)}} open" },
		{ "type": "label", "id": "finished", "text": "{{data.finished[0].title}}" },
		{ "type": "label", "id": "total", "text": "{{data.total}} notes" },
		{ "type": "label", "id": "tags", "text": "{{data.tagCount}} tags" },
		{ "type": "label", "id": "other", "text": "{{state.other}}" }
	] }
})json");
}

} // namespace

TEST_CASE("derived values: a state change no computed value reads evaluates none") {
	Built built(computedScreen());
	built.run("other", {{"v", 1}});
	CHECK_FALSE(built.last().full);
	CHECK(built.last().computedEvaluated == 0);
	CHECK(built.state()["other"] == 1);
}

TEST_CASE("derived values: a computed value that stays equal evaluates none of its readers") {
	Built built(json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "a": 1 },
		"computed": { "parity": "{{state.a % 2}}" },
		"actions": { "three": [{ "set": "state.a", "to": 3 }] },
		"root": { "type": "column", "children": [{ "type": "label", "id": "parity", "text": "{{computed.parity}}" }] }
	})json"));
	CHECK(built.text("parity") == "1");
	built.run("three");
	CHECK_FALSE(built.last().full);
	CHECK(built.last().computedEvaluated == 1);
	CHECK(built.last().nodesEvaluated == 0);
	CHECK(built.text("parity") == "1");
}

TEST_CASE("derived values: in a chain of three computed values each is evaluated once") {
	Built built(json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "x": 1 },
		"computed": { "a": "{{state.x + 1}}", "b": "{{computed.a * 2}}", "c": "{{computed.b + 1}}" },
		"actions": { "bump": [{ "increment": "state.x" }] },
		"root": { "type": "column", "children": [{ "type": "label", "id": "c", "text": "{{computed.c}}" }] }
	})json"));
	CHECK(built.text("c") == "5");
	built.run("bump");
	CHECK_FALSE(built.last().full);
	CHECK(built.last().computedEvaluated == 3);
	CHECK(built.last().nodesEvaluated == 1);
	CHECK(built.text("c") == "7");
}

TEST_CASE("derived values: a watch handler runs once when its value changes, not when another place does") {
	Built built(json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "x": 0, "y": 0, "runs": 0, "seen": "" },
		"watch": { "state.x": "noticed" },
		"actions": {
			"noticed": [{ "increment": "state.runs" }, { "set": "state.seen", "to": "{{event.previous}}>{{event.value}}" }],
			"x": [{ "increment": "state.x" }],
			"y": [{ "increment": "state.y" }]
		},
		"root": { "type": "label", "text": "{{state.y}}" }
	})json"));
	built.run("y");
	built.window->render();
	CHECK(built.state()["runs"] == 0);
	built.run("x");
	built.window->render();
	built.window->render();
	CHECK(built.state()["runs"] == 1);
	CHECK(built.state()["seen"] == "0>1");
	built.run("y");
	built.window->render();
	CHECK(built.state()["runs"] == 1);
}

TEST_CASE("derived values: a changed record updates only what reads it, its collection or a query over it") {
	Built built(recordsScreen());
	CHECK(built.text("finished") == "Second");
	built.run("rename"); // a finished note: the open notes and the count stay
	CHECK_FALSE(built.last().full);
	CHECK(built.last().nodesEvaluated == 2); // the first note (its collection) and the finished one
	CHECK(built.text("finished") == "Renamed");
	CHECK(built.text("open") == "1 open");
	CHECK(built.text("total") == "2 notes");

	built.run("add"); // the queries over the notes run again; not the one over the tags
	CHECK_FALSE(built.last().full);
	CHECK(built.last().nodesEvaluated == 3); // the first note, the open notes and the count
	CHECK(built.text("open") == "2 open");
	CHECK(built.text("total") == "3 notes");
	CHECK(built.text("tags") == "1 tags");
}

TEST_CASE("derived values: a preview with changed models shows the preview's records") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(480, 320, clock);
	json document = recordsScreen();
	document.erase("queries");
	document["actions"] = json::object();
	document["root"] = json::parse(R"({ "type": "column", "children": [{ "type": "label", "id": "first",
		"text": "{{join(keys(data.notes[0]), ',')}}" }] })");
	affresco::Screen screen = affresco::Screen::fromString(document.dump());
	screen.build(*window);
	window->render();
	REQUIRE(screen.find("first"));
	const json renamed = {
	    {"author", "agent"},
	    {"set",
		 {{"/models/notes/version", 2},
		  {"/models/notes/fields/name", {{"type", "string"}, {"required", true}}},
		  {"/models/notes/migrations",
		   json::parse(R"([{ "from": 1, "to": 2, "steps": [{ "rename": "title", "to": "name" }] }])")}}},
	    {"remove", {"/models/notes/fields/title"}}};
	const affresco::RevisionResult proposed = screen.submitRevision(renamed.dump());
	REQUIRE(proposed.status == affresco::RevisionStatus::Proposed);
	REQUIRE(screen.previewProposal(proposed.proposal));
	window->render();
	const affresco::Label* label = dynamic_cast<affresco::Label*>(screen.find("first"));
	REQUIRE(label);
	CHECK(label->text().contains("name"));
	screen.endPreview();
	window->render();
	label = dynamic_cast<affresco::Label*>(screen.find("first"));
	REQUIRE(label);
	CHECK(label->text().contains("title"));
}

TEST_CASE("derived values: a revision drops every computed value, whatever it read") {
	Built built(json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"state": { "x": 2 },
		"computed": { "shown": "{{state.x * 10}}", "label": "{{computed.shown}} units" },
		"root": { "type": "column", "children": [{ "type": "label", "id": "shown", "text": "{{computed.label}}" }] }
	})json"));
	CHECK(built.text("shown") == "20 units");
	const affresco::RevisionResult applied = built.repository.submit(
	    {{"author", "app"}, {"message", "m"}, {"set", {{"/computed/shown", "{{state.x * 100}}"}}}});
	REQUIRE(applied.status == affresco::RevisionStatus::Applied);
	built.window->render();
	CHECK(built.text("shown") == "200 units");
	CHECK(built.last().computedEvaluated == 2);
}
