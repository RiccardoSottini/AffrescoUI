/// @file
/// @brief Screen::fileChanges(): a change as a reviewer reads it, in the files that hold what it changes.

#include "HistoryFiles.hpp" // TemporaryFolder

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

using affresco::json;
using affresco::Screen;
using affresco::test::TemporaryFolder;
namespace fs = std::filesystem;

namespace {

/// @brief Writes a file, making its folders.
/// @param file The file.
/// @param text What it holds.
void writeText(const fs::path& file, const std::string& text) {
	fs::create_directories(file.parent_path());
	std::ofstream(file, std::ios::binary) << text;
}

/// @brief A screen in three files: the entry, a part it includes, and a language file that part includes.
/// @param folder The folder.
/// @return The entry.
fs::path writeApp(const fs::path& folder) {
	writeText(folder / "index.screen.json", R"json({
		"format": "affrescoui-screen", "version": 2, "include": ["part.json"],
		"permissions": { "reviewers": ["studio"], "ask": ["agent"] },
		"state": { "count": 1 },
		"root": { "id": "root", "children": [{ "id": "title", "type": "label", "text": "One" }] } })json");
	writeText(folder / "part.json", R"json({ "include": ["locales/en.json"],
		"components": { "card": { "node": { "id": "card", "color": "#111111" } } } })json");
	writeText(folder / "locales" / "en.json", R"json({ "strings": { "en": { "hello": "Hello" } } })json");
	return folder / "index.screen.json";
}

/// @brief Proposes a patch as the agent.
/// @param screen The screen.
/// @param patch The patch.
/// @return The proposal's id.
int propose(Screen& screen, const json& patch) {
	const affresco::RevisionResult result = screen.versions().submit({{"author", "agent"}, {"patch", patch}});
	CAPTURE(result.error);
	REQUIRE(result.proposal > 0);
	return result.proposal;
}

/// @brief A file's row in a change.
/// @param changes The change.
/// @param file The file's name.
/// @return The row, or null.
json rowOf(const json& changes, const std::string& file) {
	for(const json& row : changes["files"])
		if(row["file"] == file) return row;
	return nullptr;
}

/// @brief The files a change lists.
/// @param changes The change.
/// @return How many.
std::size_t fileCount(const json& changes) { return changes["files"].size(); }

} // namespace

TEST_CASE("file changes: a change is listed in the file that holds what it changes, however deep the include") {
	const TemporaryFolder folder;
	Screen screen = Screen::fromFile(writeApp(folder.path()), folder.path() / "data", Screen::History::InMemory);
	const int inPart = propose(screen, json::parse(R"json([
		{ "op": "replace", "path": "/components/card/node/color", "value": "#222222" },
		{ "op": "replace", "path": "/strings/en/hello", "value": "Hi" },
		{ "op": "replace", "path": "/root/children/0/text", "value": "Two" }])json"));
	const json changes = screen.fileChanges({{"proposal", inPart}});
	CHECK(changes["kind"] == "proposal");
	CHECK(changes["author"] == "agent");
	REQUIRE(fileCount(changes) == 3);
	CHECK(rowOf(changes, "part.json")["added"] == 1);
	CHECK(rowOf(changes, "part.json")["removed"] == 1);
	CHECK(rowOf(changes, "locales/en.json")["added"] == 1);
	CHECK(rowOf(changes, "index.screen.json")["removed"] == 1);
	CHECK_FALSE(rowOf(changes, "part.json").contains("before")); // only the file asked for has its text
}

TEST_CASE("file changes: a file's text is what it holds before and after, and nothing of the other files") {
	const TemporaryFolder folder;
	Screen screen = Screen::fromFile(writeApp(folder.path()), folder.path() / "data", Screen::History::InMemory);
	const int id = propose(screen, json::parse(R"json([
		{ "op": "replace", "path": "/components/card/node/color", "value": "#222222" }])json"));
	const json row = rowOf(screen.fileChanges({{"proposal", id}, {"file", "part.json"}}), "part.json");
	const std::string before = row["before"].get<std::string>();
	const std::string after = row["after"].get<std::string>();
	CHECK(before.contains("#111111"));
	CHECK(after.contains("#222222"));
	CHECK_FALSE(after.contains("#111111"));
	CHECK_FALSE(after.contains("hello")); // the language file's
	CHECK_FALSE(after.contains("title")); // the entry's
}

TEST_CASE("file changes: what no file holds yet is the screen's own, and the live state only shows if changed") {
	const TemporaryFolder folder;
	Screen screen = Screen::fromFile(writeApp(folder.path()), folder.path() / "data", Screen::History::InMemory);
	const int added = propose(screen, json::parse(R"json([
		{ "op": "add", "path": "/components/fresh", "value": { "node": { "id": "fresh" } } }])json"));
	const json row =
	    rowOf(screen.fileChanges({{"proposal", added}, {"file", "index.screen.json"}}), "index.screen.json");
	REQUIRE(row.is_object());
	CHECK(row["after"].get<std::string>().contains("fresh"));
	CHECK_FALSE(row["after"].get<std::string>().contains("count"));
}

TEST_CASE("file changes: the same change reads in either notation, with the same counts") {
	const TemporaryFolder folder;
	Screen screen = Screen::fromFile(writeApp(folder.path()), folder.path() / "data", Screen::History::InMemory);
	const int id = propose(screen, json::parse(R"json([
		{ "op": "replace", "path": "/strings/en/hello", "value": "Hi" }])json"));
	const json asJson = screen.fileChanges({{"proposal", id}, {"file", "locales/en.json"}, {"notation", "json"}});
	const json asLisp = screen.fileChanges({{"proposal", id}, {"file", "locales/en.json"}, {"notation", "lisp"}});
	CHECK(asJson["notation"] == "json");
	CHECK(asLisp["notation"] == "lisp");
	CHECK(rowOf(asJson, "locales/en.json")["after"].get<std::string>().contains("\"hello\""));
	CHECK(rowOf(asLisp, "locales/en.json")["after"].get<std::string>().contains(":hello"));
	CHECK(rowOf(asLisp, "locales/en.json")["added"] == rowOf(asJson, "locales/en.json")["added"]);
}

TEST_CASE("file changes: previewing a proposal doesn't change what the reviewer is told about any change") {
	const TemporaryFolder folder;
	Screen screen = Screen::fromFile(writeApp(folder.path()), folder.path() / "data", Screen::History::InMemory);
	const int restyle = propose(screen, json::parse(R"json([
		{ "op": "replace", "path": "/root/children/0/text", "value": "Nothing to see" }])json"));
	const json request = {{"proposal", restyle}, {"file", "index.screen.json"}};
	const json before = screen.fileChanges(request);
	REQUIRE(screen.previewProposal(restyle));
	CHECK(screen.fileChanges(request) == before);
	screen.endPreview();
	CHECK(screen.fileChanges(request) == before);
}

TEST_CASE("file changes: an applied commit reads against its parent, and an unknown one is an error") {
	const TemporaryFolder folder;
	Screen screen = Screen::fromFile(writeApp(folder.path()), folder.path() / "data", Screen::History::InMemory);
	const int id = propose(screen, json::parse(R"json([
		{ "op": "replace", "path": "/components/card/node/color", "value": "#222222" }])json"));
	const std::string head = screen.proposals().front().head;
	REQUIRE(screen.acceptProposal(id, head));
	const json merged = screen.fileChanges({{"to", "HEAD"}, {"file", "part.json"}});
	CHECK(merged["kind"] == "commit");
	const json row = rowOf(merged, "part.json");
	REQUIRE(row.is_object());
	CHECK(row["before"].get<std::string>().contains("#111111"));
	CHECK(row["after"].get<std::string>().contains("#222222"));
	CHECK_THROWS_AS(static_cast<void>(screen.fileChanges({{"to", "no-such-commit"}})), affresco::DocumentError);
	CHECK_THROWS_AS(static_cast<void>(screen.fileChanges({{"proposal", 999}})), affresco::DocumentError);
}

TEST_CASE("file changes: what a change does besides how it looks is named for the reviewer") {
	const TemporaryFolder folder;
	Screen screen = Screen::fromFile(writeApp(folder.path()), folder.path() / "data", Screen::History::InMemory);
	const int id = propose(screen, json::parse(R"json([
		{ "op": "add", "path": "/actions", "value": { "reset": [{ "set": "state.count", "to": 0 }] } }])json"));
	CHECK_FALSE(screen.fileChanges({{"proposal", id}})["behaviour"].empty());
}

TEST_CASE("file changes: an example's changes are in the files that hold them, in both of its notations") {
	const fs::path examples = AFFRESCOUI_EXAMPLES_DIRECTORY;
	for(const char* notation : {"json", "lisp"}) {
		CAPTURE(notation);
		Screen screen = Screen::fromApp(examples / "flow" / notation, {}, Screen::History::InMemory);
		std::ifstream file(examples / "flow" / "json" / "strings.json");
		const json strings = json::parse(file)["strings"]["en"];
		REQUIRE_FALSE(strings.empty());
		const std::string key = strings.items().begin().key();
		const json patch = {{{"op", "replace"}, {"path", "/strings/en/" + key}, {"value", "Changed"}}};
		const affresco::RevisionResult result = screen.versions().submit({{"author", "studio"}, {"patch", patch}});
		CAPTURE(result.error);
		const json changes = screen.fileChanges({{"to", "HEAD"}});
		REQUIRE(fileCount(changes) == 1);
		CHECK(changes["files"][0]["file"] == std::string("strings.") + notation);
	}
}
