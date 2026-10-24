/// @file
/// @brief The data layer in a screen: `data.<collection>` and declared queries in bindings, the write steps (a refused
/// write runs onError), undo of the records; the checker finds a binding to a field its model doesn't have; in
/// version control a view-only revision has data impact none and never touches the store, a transforming model change
/// is refused without its migration, a destructive one is always reviewed, and reverting a migrating commit brings
/// the records back; the old "persist" records import once, from a copy in a temporary folder.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/render/Label.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A field of a stored record, or null if there is no such record.
/// @param store The store.
/// @param collection The collection.
/// @param id The record.
/// @param field The field.
/// @return Its value.
json fieldOf(const affresco::data::Store& store, const std::string& collection, const std::string& id,
             const std::string& field) {
	const std::optional<affresco::data::Record> record = store.get(collection, id);
	return record ? record->fields.value(field, json()) : json();
}

/// @brief A notes screen: generated test data only.
const json& notesScreen() {
	static const json value = json::parse(R"json({
	"format": "affrescoui-screen", "version": 2,
	"models": { "notes": { "version": 1, "fields": {
		"title": { "type": "string", "required": true },
		"done": { "type": "boolean", "default": false } },
		"seed": [{ "id": "n1", "title": "First" }, { "id": "n2", "title": "Second", "done": true }] } },
	"queries": { "open": { "from": "notes", "where": { "done": false } }, "total": { "from": "notes", "count": true } },
	"state": { "draft": "Third", "error": "" },
	"actions": {
		"add": [{ "insert": "notes", "value": { "title": "{{state.draft}}" } }],
		"bad": [{ "insert": "notes", "value": { "title": 42 }, "onError": [{ "set": "state.error", "to": "{{error.code}}" }] }],
		"finish": [{ "update": "notes", "where": { "done": false }, "fields": { "done": true } }],
		"back": [{ "undoData": true }]
	},
	"root": { "type": "column", "children": [
		{ "type": "label", "id": "count", "text": "{{data.total}} notes, {{length(data.open)}} open" },
		{ "type": "column", "id": "list", "children": [
			{ "type": "label", "repeat": { "for": "note", "in": "data.notes" }, "text": "{{note.title}}" }] }
	] }
})json");
	return value;
}

/// @brief A label's text.
/// @param screen The screen.
/// @param id The label's id.
/// @return Its text.
std::string textOf(affresco::Screen& screen, std::string_view id) {
	auto* label = dynamic_cast<affresco::Label*>(screen.find(id));
	REQUIRE(label);
	return label->text();
}

/// @brief A screen with a notes model and no binding to its fields, for revisions of the models.
/// @return The document.
json bareNotes() {
	json document = notesScreen();
	document.erase("queries");
	document["root"] = {{"type", "label"}, {"id", "count"}, {"text", "{{length(data.notes)}}"}};
	document["actions"] = json::object();
	return document;
}

/// @brief A revision as JSON text.
/// @param revision The revision.
/// @return Its text.
std::string text(const json& revision) { return revision.dump(); }

} // namespace

TEST_CASE("data screens: bindings read collections and queries, and write steps change them") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(480, 320, clock);
	affresco::Screen screen = affresco::Screen::fromString(notesScreen().dump());
	screen.build(*window);
	window->render();
	CHECK(textOf(screen, "count") == "2 notes, 1 open");
	affresco::Panel& root = window->root();
	screen.runAction("add", root);
	window->render();
	CHECK(textOf(screen, "count") == "3 notes, 2 open");
	CHECK(screen.find("list")->children().size() == 3);

	screen.runAction("bad", root);
	window->render();
	CHECK(json::parse(screen.stateJson())["error"] == "invalid");
	CHECK(textOf(screen, "count") == "3 notes, 2 open"); // nothing got in

	screen.runAction("finish", root);
	window->render();
	CHECK(textOf(screen, "count") == "3 notes, 0 open");
	screen.runAction("back", root); // one step: the whole update
	window->render();
	CHECK(textOf(screen, "count") == "3 notes, 2 open");
	CHECK(screen.data().journal(10).front().author == "app");
}

TEST_CASE("data screens: the checker finds a binding to a field the model doesn't have") {
	const affresco::format::Rules rules;
	json document = notesScreen();
	document["root"]["children"][1]["children"][0]["text"] = "{{note.colour}}";
	const auto findings = rules.findings(document, document["state"]);
	REQUIRE_FALSE(findings.empty());
	CHECK(findings.front().contains("colour"));
	CHECK(rules.findings(notesScreen(), notesScreen()["state"]).empty());
	json query = notesScreen(); // a query of a field that isn't there
	query["queries"]["open"]["where"] = {{"finished", false}};
	CHECK_THROWS_WITH_AS(rules.validate(query), doctest::Contains("finished"), affresco::DocumentError);
}

TEST_CASE("data screens: a view-only revision has no data impact and never touches the store") {
	affresco::Screen screen = affresco::Screen::fromString(bareNotes().dump());
	const json before = screen.data().run({{"from", "notes"}}, "q");
	const std::size_t journal = screen.data().journal(100).size();
	const json revision = {{"author", "agent"}, {"set", {{"#count/textColor", "#ff0000"}}}};
	std::string error;
	REQUIRE(screen.applyRevision(text(revision), &error));
	CHECK(screen.data().run({{"from", "notes"}}, "q") == before);
	CHECK(screen.data().journal(100).size() == journal);

	const affresco::format::Rules rules;
	json after = bareNotes();
	after["root"]["textColor"] = "#ff0000";
	const json impact = rules.impact(bareNotes(), after);
	CHECK(impact["view"] == 1);
	CHECK(impact["logic"] == 0);
	CHECK(impact["data"] == "none");
	CHECK(impact["model"].empty());
}

TEST_CASE("data screens: a transforming model change needs its migration, and its revert brings the records back") {
	affresco::Screen screen = affresco::Screen::fromString(bareNotes().dump());
	const json before = screen.data().run({{"from", "notes"}}, "q");
	std::string error;
	// A rename without a migration: refused
	const json rename = {{"author", "app"},
	                     {"set", {{"/models/notes/fields/name", {{"type", "string"}, {"required", true}}}}},
	                     {"remove", {"/models/notes/fields/title"}}};
	CHECK_FALSE(screen.applyRevision(text(rename), &error));
	INFO(error);
	CHECK(error.contains("migration"));
	// With the version raised and its migration
	json migrated = rename;
	migrated["set"]["/models/notes/version"] = 2;
	migrated["set"]["/models/notes/migrations"] =
	    json::parse(R"([{ "from": 1, "to": 2, "steps": [{ "rename": "title", "to": "name" }] }])");
	INFO(error);
	REQUIRE(screen.applyRevision(text(migrated), &error));
	const json now = screen.data().run({{"from", "notes"}}, "q");
	CHECK(now[0]["name"] == "First");
	CHECK_FALSE(now[0].contains("title"));
	// A rewritten migration is refused: it is history
	json rewritten = {{"author", "app"}, {"set", {{"/models/notes/migrations/0/steps/0/to", "label"}}}};
	CHECK_FALSE(screen.applyRevision(text(rewritten), &error));
	// The revert: the records as they were
	REQUIRE(screen.revert(screen.revision(), &error));
	CHECK(nlohmann::json(screen.data().run({{"from", "notes"}}, "q")) == nlohmann::json(before)); // key order aside
}

TEST_CASE("data screens: a destructive model change is always reviewed; a model change's records must fit") {
	affresco::Screen screen = affresco::Screen::fromString(bareNotes().dump());
	const json drop = {{"author", "agent"}, {"remove", {"/models/notes/fields/done"}}};
	const affresco::RevisionResult result = screen.submitRevision(text(drop));
	INFO(result.error);
	CHECK(result.status == affresco::RevisionStatus::Proposed);
	REQUIRE(screen.proposals().size() == 1);
	const json impact = screen.proposals().front().impact;
	CHECK(impact["model"][0]["class"] == "destructive");
	CHECK(impact["data"].get<std::string>().starts_with("destructive"));

	// A narrowed field the stored records break: the dry run refuses it, naming them
	std::string error;
	const json narrow = {{"author", "app"}, {"set", {{"/models/notes/fields/title/maxLength", 5}}}};
	CHECK_FALSE(screen.applyRevision(text(narrow), &error));
	INFO(error);
	CHECK(error.contains("id n2: notes[n2].title: expected 0 to 5 characters")); // the dry run names each record
}

TEST_CASE("data screens: a model's importFrom brings the old persist records in once, from a backed-up copy") {
	const auto folder = std::filesystem::temp_directory_path() / "affrescoui-data-import-test";
	std::filesystem::remove_all(folder);
	std::filesystem::create_directories(folder);
	json document = bareNotes();
	document["models"]["notes"]["importFrom"] = "state.notes";
	document["persist"] = json::array({"state.draft"});
	{
		std::ofstream(folder / "notes.screen.json") << document.dump();
		std::ofstream(folder / "notes.state.json")
		    << R"({"/draft": "kept", "/notes": [{"title": "Old one"}, {"title": 7}, {"title": "Old two", "done": true}]})";
	}
	{
		affresco::Screen screen = affresco::Screen::fromFile(folder / "notes.screen.json", folder);
		CHECK(screen.data().size("notes") == 2); // the one that doesn't fit stays in the backup
		CHECK(std::filesystem::exists(folder / "notes.state.json.before-data.json"));
		CHECK(json::parse(screen.stateJson())["draft"] == "kept");
		(void)screen.data().insert("notes", {{"title", "New"}}, "app");
	}
	{
		affresco::Screen again = affresco::Screen::fromFile(folder / "notes.screen.json", folder);
		CHECK(again.data().size("notes") == 3); // imported once, kept in the database
	}
	std::filesystem::remove_all(folder);
}

TEST_CASE("data screens: importCsv imports rows; an agent writes only what its data rules let it") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(480, 320, clock);
	json document = notesScreen();
	document["actions"]["csv"] =
	    json::parse(R"([{ "importCsv": "notes", "text": "{{event.text}}", "into": "state.report" }])");
	document["permissions"] = {{"data", {{{"author", "agent"}, {"never", {"/data/notes"}}}}}};
	affresco::Screen screen = affresco::Screen::fromString(document.dump());
	screen.build(*window);
	affresco::Panel& root = window->root();
	screen.runAction("csv", root, json{{"text", "Title,Done\nFrom a file,yes\n,no\n"}}.dump());
	window->render();
	const json report = json::parse(screen.stateJson())["report"];
	CHECK(report["imported"] == 1);
	CHECK(report["failed"] == 1); // no title
	CHECK(textOf(screen, "count") == "3 notes, 1 open");

	screen.setDataAuthor("agent/test"); // the host runs an agent's reviewed step
	screen.runAction("bad", root);      // onError gets the refusal
	CHECK(json::parse(screen.stateJson())["error"] == "denied");
	screen.setDataAuthor("");
	screen.runAction("add", root);
	window->render();
	CHECK(textOf(screen, "count") == "4 notes, 2 open");
}

TEST_CASE("data screens: a previewed proposal that migrates shows its records on a copy; the store stays") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(480, 320, clock);
	affresco::Screen screen = affresco::Screen::fromString(bareNotes().dump());
	screen.build(*window);
	const json renamed = {
	    {"author", "agent"},
	    {"set",
		 {{"/models/notes/version", 2},
		  {"/models/notes/fields/name", {{"type", "string"}, {"required", true}}},
		  {"/models/notes/migrations",
		   json::parse(R"([{ "from": 1, "to": 2, "steps": [{ "rename": "title", "to": "name" }] }])")}}},
	    {"remove", {"/models/notes/fields/title"}}};
	const affresco::RevisionResult proposed = screen.submitRevision(renamed.dump());
	REQUIRE(proposed.status == affresco::RevisionStatus::Proposed); // a migration is always reviewed
	CHECK(screen.proposals().front().impact["data"] == "migrates 2 records");
	REQUIRE(screen.previewProposal(proposed.proposal));
	CHECK(json::parse(screen.dataJson())["notes"][0]["name"] == "First"); // the preview reads a migrated copy
	CHECK(fieldOf(screen.data(), "notes", "n1", "title") == "First");     // the store is as it was
	screen.endPreview();
	CHECK(json::parse(screen.dataJson())["notes"][0]["title"] == "First");
}

TEST_CASE("data screens: persisted values that no longer fit the stateTypes open as defaults, kept in a side file") {
	const auto folder = std::filesystem::temp_directory_path() / "affrescoui-data-rejected-test";
	std::filesystem::remove_all(folder);
	std::filesystem::create_directories(folder);
	const json document = {{"format", "affrescoui-screen"},
	                       {"version", 2},
	                       {"state", {{"volume", 50}, {"name", "a"}}},
	                       {"stateTypes", {{"volume", {{"type", "number"}, {"maximum", 100}}}}},
	                       {"persist", {"state.volume", "state.name"}},
	                       {"root", {{"type", "label"}, {"id", "v"}, {"text", "{{state.volume}}"}}}};
	std::ofstream(folder / "player.screen.json") << document.dump();
	std::ofstream(folder / "player.state.json") << R"({"/volume": "loud", "/name": "kept"})";
	{
		const affresco::Screen screen = affresco::Screen::fromFile(folder / "player.screen.json", folder);
		const json state = json::parse(screen.stateJson());
		CHECK(state["volume"] == 50);   // the default: "loud" isn't a number
		CHECK(state["name"] == "kept"); // what fits is loaded
	}
	std::ifstream side(folder / "player.state.rejected.json");
	REQUIRE(side);
	const json rejected = json::parse(side);
	CHECK(rejected["/volume"]["value"] == "loud");
	CHECK(rejected["/volume"]["problem"].get<std::string>().contains("state.volume"));
	std::filesystem::remove_all(folder);
}
