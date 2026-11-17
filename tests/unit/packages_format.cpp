/// @file
/// @brief Packages' format (schema/affrescoui-package.schema.json, Package.hpp): the schema accepts a minimal package
/// of each kind and refuses an unknown key, an operation other than add, remove and replace in the base's delta, and a
/// whole app without its files; a document's tree is the same whatever the order of its keys, and another when a value
/// changes.

#include <affresco/format/Package.hpp>

// The tool's own schema checker (tools/affrescoui/Schema.hpp), which `affrescoui check` uses: compiled in here, since
// it is no library's
#include "../../tools/affrescoui/Schema.cpp" // NOLINT(bugprone-suspicious-include)

#include <doctest/doctest.h>

#include <fstream>
#include <string>

using affresco::json;

namespace {

/// @brief The package schema, read from the repository.
/// @return It.
json packageSchema() {
	std::ifstream file(std::string(AFFRESCOUI_EXAMPLES_DIRECTORY) + "/../schema/affrescoui-package.schema.json");
	return json::parse(file);
}

/// @brief A minimal package of kind "changes": generated test data only.
/// @return It.
json minimalChanges() {
	return json::parse(R"json({
	"format": "affrescoui-package", "version": 1, "kind": "changes", "name": "Compact cards", "release": "1.0.0",
	"engine": "0.9.0",
	"base": { "app": "notes", "name": "Notes", "notation": "json",
	          "tree": "sha256:0000000000000000000000000000000000000000000000000000000000000000",
	          "delta": [ { "op": "replace", "path": "/state/title", "value": "Notes" },
	                     { "op": "remove", "path": "/state/compact" } ] },
	"document": { "state": { "title": "Compact notes" } }
	})json");
}

/// @brief A minimal package of kind "app": a minimal one of kind "changes" with the base app's files.
/// @return It.
json minimalApp() {
	json package = minimalChanges();
	package["kind"] = "app";
	package["base"]["release"] = "2.1.0";
	package["base"]["files"] =
	    json::parse(R"json({ "app.json": { "text": "{}" }, "assets/icon.png": { "data": "iVBORw0K" } })json");
	return package;
}

/// @brief Whether the package schema accepts a value.
/// @param value The value.
/// @return Whether it does.
bool accepted(const json& value) { return affresco::cli::Schema(packageSchema()).check(value).empty(); }

} // namespace

TEST_CASE("package format: the schema accepts a minimal package of each kind") {
	CHECK(accepted(minimalChanges()));
	CHECK(accepted(minimalApp()));
	json full = minimalChanges();
	full["description"] = "Smaller cards.";
	full["author"] = "A person";
	full["license"] = "Apache-2.0";
	full["state"] = {{"compact", true}};
	full["assets"] = {{"note.txt", {{"type", "text/plain"}, {"text", "Hello"}}}};
	full["proof"] = {{"scenarios", json::array()}};
	CHECK(accepted(full));
}

TEST_CASE("package format: the schema refuses an unknown key, a move in the delta and an app without its files") {
	json unknown = minimalChanges();
	unknown["runs"] = "anything";
	CHECK_FALSE(accepted(unknown));
	json unknownInBase = minimalChanges();
	unknownInBase["base"]["runs"] = "anything";
	CHECK_FALSE(accepted(unknownInBase));
	json move = minimalChanges();
	move["base"]["delta"].push_back({{"op", "move"}, {"from", "/state/a"}, {"path", "/state/b"}});
	CHECK_FALSE(accepted(move));
	json withoutFiles = minimalApp();
	withoutFiles["base"].erase("files");
	CHECK_FALSE(accepted(withoutFiles));
	json badRelease = minimalChanges();
	badRelease["release"] = "1.0";
	CHECK_FALSE(accepted(badRelease));
	json longName = minimalChanges();
	longName["name"] = std::string(81, 'a');
	CHECK_FALSE(accepted(longName));
	json outside = minimalApp();
	outside["base"]["files"]["../escape.json"] = {{"text", "{}"}};
	CHECK_FALSE(accepted(outside));
}

TEST_CASE("package format: a document's tree follows its content, not the order of its keys") {
	const json document =
	    json::parse(R"json({ "state": { "a": 1, "b": [true, "x"] }, "root": { "type": "panel" } })json");
	const json reordered =
	    json::parse(R"json({ "root": { "type": "panel" }, "state": { "b": [true, "x"], "a": 1 } })json");
	const std::string tree = affresco::format::documentTree(document);
	CHECK(tree.starts_with("sha256:"));
	CHECK(tree.size() == 7 + 64);
	CHECK(affresco::format::documentTree(reordered) == tree);
	json changed = document;
	changed["state"]["a"] = 2;
	CHECK(affresco::format::documentTree(changed) != tree);
	CHECK(accepted([&] {
		json package = minimalChanges();
		package["base"]["tree"] = tree;
		return package;
	}()));
}
