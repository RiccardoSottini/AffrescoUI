/// @file
/// @brief The data module's store: a bad record never gets in, the journal undoes and redoes, queries select, sort
/// and count, and live queries tell when their results change; CSV rows import with their failures reported.

#include <affresco/data/Csv.hpp>
#include <affresco/data/Store.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::data::DataError;
using affresco::data::LiveQuery;
using affresco::data::Store;

namespace {

/// @brief Generated test models: notes with tags and a folder they refer to.
const json models = json::parse(R"json({
	"folders": { "version": 1, "fields": { "name": { "type": "string", "required": true, "unique": true } },
		"seed": [{ "id": "inbox", "name": "Inbox" }, { "id": "work", "name": "Work" }] },
	"notes": { "version": 1, "fields": {
		"title": { "type": "string", "required": true, "maxLength": 40 },
		"stars": { "type": "integer", "minimum": 0, "maximum": 5, "default": 0 },
		"kind": { "type": "enum", "values": ["idea", "task"] },
		"done": { "type": "boolean" },
		"due": { "type": "date" },
		"tags": { "type": "list", "of": { "type": "string" }, "maxItems": 4 },
		"folder": { "type": "reference", "to": "folders" },
		"meta": { "type": "object", "fields": { "color": { "type": "string" } } }
	} }
})json");

/// @brief A store following the test models, in memory, on a clock that counts.
/// @param store The store.
void follow(Store& store) {
	static std::int64_t now = 1000;
	store.setClock([] { return now += 10; });
	(void)store.follow(models, "test");
}

/// @brief The error code of what a call throws.
/// @param call The call.
/// @return The code, or "" if it throws nothing.
template <typename Call> std::string codeOf(Call call) {
	try {
		call();
	} catch(const DataError& error) {
		return error.code();
	}
	return "";
}

} // namespace

TEST_CASE("data: a new collection starts with its seed, once") {
	Store store;
	follow(store);
	CHECK(store.size("folders") == 2);
	CHECK(store.get("folders", "inbox")->fields["name"] == "Inbox");
	(void)store.follow(models, "test"); // the same models again: nothing new
	CHECK(store.size("folders") == 2);
}

TEST_CASE("data: validation refuses bad records, and nothing gets in") {
	Store store;
	follow(store);
	CHECK(codeOf([&] { (void)store.insert("notes", {{"stars", 2}}, "app"); }) == "invalid");                 // required
	CHECK(codeOf([&] { (void)store.insert("notes", {{"title", 7}}, "app"); }) == "invalid");                 // type
	CHECK(codeOf([&] { (void)store.insert("notes", {{"title", "a"}, {"stars", 9}}, "app"); }) == "invalid"); // bound
	CHECK(codeOf([&] { (void)store.insert("notes", {{"title", "a"}, {"kind", "x"}}, "app"); }) == "invalid"); // enum
	CHECK(codeOf([&] { (void)store.insert("notes", {{"title", "a"}, {"due", "2026-02-30"}}, "app"); }) == "invalid");
	CHECK(codeOf([&] { (void)store.insert("notes", {{"title", "a"}, {"colour", 1}}, "app"); }) == "invalid");
	CHECK(codeOf([&] { (void)store.insert("notes", {{"title", "a"}, {"tags", {1}}}, "app"); }) == "invalid");
	CHECK(codeOf([&] { (void)store.insert("notes", {{"title", "a"}, {"meta", {{"size", 1}}}}, "app"); }) == "invalid");
	CHECK(codeOf([&] { (void)store.insert("notes", {{"title", "a"}, {"folder", "nowhere"}}, "app"); }) == "reference");
	CHECK(codeOf([&] { (void)store.insert("folders", {{"name", "Inbox"}}, "app"); }) == "unique");
	CHECK(codeOf([&] { (void)store.insert("songs", {{"title", "a"}}, "app"); }) == "model");
	CHECK(store.size("notes") == 0);

	const auto note = store.insert("notes", {{"title", "Plan"}, {"folder", "work"}, {"tags", {"a", "b"}}}, "app");
	CHECK(note.fields["stars"] == 0); // the default
	CHECK(codeOf([&] { (void)store.update("notes", note.id, {{"stars", 6}}, "app"); }) == "invalid");
	CHECK(store.get("notes", note.id)->fields["stars"] == 0);
	CHECK(codeOf([&] { store.remove("folders", "work", "app"); }) == "reference"); // a note refers to it
	CHECK(store.size("folders") == 2);
}

TEST_CASE("data: the journal undoes and redoes, one batch at a time") {
	Store store;
	follow(store);
	CHECK_FALSE(store.canUndo());
	const auto a = store.insert("notes", {{"title", "A"}}, "app");
	{
		const Store::Batch batch(store);
		(void)store.insert("notes", {{"title", "B"}}, "app");
		(void)store.update("notes", a.id, {{"title", "A2"}}, "agent/test");
	}
	CHECK(store.size("notes") == 2);
	const auto entries = store.journal(10);
	REQUIRE(entries.size() == 3);
	CHECK(entries[0].author == "agent/test");
	CHECK(entries[0].before["title"] == "A");
	CHECK(entries[0].after["title"] == "A2");

	CHECK(store.undo()); // the batch, both writes
	CHECK(store.size("notes") == 1);
	CHECK(store.get("notes", a.id)->fields["title"] == "A");
	CHECK(store.canRedo());
	CHECK(store.redo());
	CHECK(store.size("notes") == 2);
	CHECK(store.get("notes", a.id)->fields["title"] == "A2");

	store.remove("notes", a.id, "app");
	CHECK_FALSE(store.get("notes", a.id));
	CHECK(store.undo());
	CHECK(store.get("notes", a.id)->fields["title"] == "A2");
	(void)store.insert("notes", {{"title", "C"}}, "app"); // a new write: nothing to redo
	CHECK_FALSE(store.canRedo());
}

TEST_CASE("data: queries filter, sort, page and count") {
	Store store;
	follow(store);
	for(int i = 0; i < 20; ++i)
		(void)store.insert("notes",
		                   {{"title", "n" + std::to_string(i)},
		                    {"stars", i % 6},
		                    {"kind", i % 2 ? "task" : "idea"},
		                    {"tags", i % 3 ? json::array({"x"}) : json::array({"y"})}},
		                   "app");
	const json tasks = store.run({{"from", "notes"},
	                              {"where", {{"kind", "task"}, {"stars", {{">=", 3}}}}},
	                              {"sort", {{{"field", "stars"}, {"descending", true}}, "title"}}},
	                             "query");
	REQUIRE(tasks.is_array());
	CHECK(tasks.size() == 6);
	CHECK(tasks[0]["stars"] == 5);
	CHECK(store.run({{"from", "notes"}, {"where", {{"tags", {{"contains", "y"}}}}}, {"count", true}}, "q") == 7);
	CHECK(store.run({{"from", "notes"}, {"where", {{"any", {{{"stars", 0}}, {{"stars", 5}}}}}}, {"count", true}},
	                "q") == 7);
	CHECK(store.run({{"from", "notes"}, {"where", {{"title", {{"startsWith", "n1"}}}}}, {"count", true}}, "q") == 11);
	CHECK(store.run({{"from", "notes"}, {"where", {{"stars", {{"in", {1, 2}}}}}}, {"count", true}}, "q") == 7);
	CHECK(store.run({{"from", "notes"}, {"where", {{"due", nullptr}}}, {"count", true}}, "q") == 20);
	const json page = store.run({{"from", "notes"}, {"sort", "title"}, {"limit", 5}, {"offset", 5}}, "q");
	CHECK(page.size() == 5);
	CHECK(page[0]["title"] == "n13");
	CHECK_THROWS_AS((void)store.run({{"from", "notes"}, {"where", {{"colour", 1}}}}, "q"), DataError);
	CHECK_THROWS_AS((void)store.run({{"from", "notes"}, {"where", {{"stars", {{"~", 1}}}}}}, "q"), DataError);
	// A value that looks like SQL is only ever a value
	CHECK(store.run({{"from", "notes"}, {"where", {{"title", "x' OR 1=1 --"}}}, {"count", true}}, "q") == 0);
}

TEST_CASE("data: a live query tells when its results change, and only then") {
	Store store;
	follow(store);
	LiveQuery tasks(store, {{"from", "notes"}, {"where", {{"kind", "task"}}}, {"count", true}});
	int told = 0;
	tasks.onResults.connect([&](const json&) { ++told; });
	(void)store.insert("notes", {{"title", "idea"}, {"kind", "idea"}}, "app");
	CHECK(told == 0); // the count of tasks didn't change
	const auto task = store.insert("notes", {{"title", "task"}, {"kind", "task"}}, "app");
	CHECK(told == 1);
	CHECK(tasks.results() == 1);
	(void)store.insert("folders", {{"name", "Other"}}, "app"); // another collection
	CHECK(told == 1);
	store.remove("notes", task.id, "app");
	CHECK(told == 2);
	CHECK(tasks.results() == 0);
}

TEST_CASE("data: the store keeps its records on disk, in WAL mode") {
	const auto file = std::filesystem::temp_directory_path() / "affrescoui-data-test.data.sqlite";
	std::filesystem::remove(file);
	{
		Store store(file);
		follow(store);
		(void)store.insert("notes", {{"title", "kept"}}, "app");
	}
	{
		Store store(file);
		CHECK(store.size("notes") == 1);
		CHECK(store.models().contains("notes"));
		(void)store.follow(models, "test");
		CHECK(store.size("folders") == 2); // not seeded twice
	}
	for(const char* suffix : {"", "-wal", "-shm"}) std::filesystem::remove(file.string() + suffix);
}

TEST_CASE("data: CSV rows import into a collection, the failing ones reported") {
	Store store;
	follow(store);
	const std::string csv = "Title,Stars,Kind,Tags\n"
	                        "\"Buy milk, eggs\",2,task,shop;home\n"
	                        "Too many stars,9,idea,\n"
	                        "\"Say \"\"hi\"\"\",,idea,\n"
	                        "Unknown kind,1,chore,\r\n";
	affresco::data::CsvImport options{"notes", nullptr, ',', false, "app"};
	const json report = affresco::data::importCsv(store, csv, options, nullptr);
	CHECK(report["imported"] == 2);
	CHECK(report["failed"] == 2);
	CHECK(report["failures"][0]["row"] == 3);
	CHECK(store.run({{"from", "notes"}, {"where", {{"title", "Say \"hi\""}}}, {"count", true}}, "q") == 1);
	const json milk = store.run({{"from", "notes"}, {"where", {{"title", "Buy milk, eggs"}}}}, "q");
	REQUIRE(milk.size() == 1);
	CHECK(milk[0]["tags"] == json::array({"shop", "home"}));
	CHECK(milk[0]["stars"] == 2);
	CHECK(store.undo()); // one step: the whole import
	CHECK(store.size("notes") == 0);

	options.atomic = true; // all or nothing
	CHECK(affresco::data::importCsv(store, csv, options, nullptr)["imported"] == 0);
	CHECK(store.size("notes") == 0);
	options.columns = {{"title", "Title"}, {"stars", {{"column", "Stars"}, {"with", "{{double}}"}}}};
	CHECK_THROWS_AS((void)affresco::data::importCsv(store, "Name\nx\n", options, nullptr), DataError); // no column
	CHECK_THROWS_AS((void)affresco::data::parseCsv("a,\"b\n"), DataError);                             // unclosed
}
