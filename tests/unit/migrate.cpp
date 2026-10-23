/// @file
/// @brief The migration from format version 1 to 2 (Migrate.hpp; SCREEN_FORMAT.md, "Migrating from version 1"): each
/// fixture in tests/migrate/v1 becomes its counterpart in tests/migrate/v2, twice changes nothing, the loader reads
/// version 1, and a revision with a removed key is refused with a hint.

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Migrate.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using affresco::json;
using namespace affresco::format;

namespace {

/// @brief The folder of the migration fixtures, next to the rendering check's (tests/render/fonts).
/// @return tests/migrate.
std::filesystem::path fixtures() {
	return std::filesystem::path(AFFRESCOUI_TEST_FONTS_DIR).parent_path().parent_path() / "migrate";
}

/// @brief Reads a fixture.
/// @param path Its path.
/// @return Its JSON.
json readFixture(const std::filesystem::path& path) {
	std::ifstream in(path, std::ios::binary);
	REQUIRE(in);
	std::stringstream text;
	text << in.rdbuf();
	return affresco::parseUntrusted(text.str());
}

/// The fixtures, by name.
constexpr std::string_view fixtureNames[] = {"board.screen.json", "board.scenarios.json", "board.changes.json",
                                             "feature.json", "catalog.json"};

} // namespace

TEST_CASE("migrate: each version 1 fixture becomes its version 2 counterpart, and twice changes nothing") {
	for(const std::string_view name : fixtureNames) {
		INFO("fixture ", std::string(name));
		json document = readFixture(fixtures() / "v1" / name);
		const json expected = readFixture(fixtures() / "v2" / name);
		CHECK(documentVersion(document) == 1);
		CHECK_FALSE(migrate(document).empty());
		CHECK(document == expected);
		CHECK(migrate(document).empty());
		json current = expected;
		CHECK(migrate(current).empty());
		CHECK(current == expected);
	}
}

TEST_CASE("migrate: renames keep ids, key order and data; quoted text and text outside placeholders stay") {
	json screen = readFixture(fixtures() / "v1" / "board.screen.json");
	const std::vector<Rewrite> rewrites = migrate(screen);
	const json& root = screen["root"];
	CHECK(root["id"] == "main");
	CHECK(root.begin().key() == "type");
	CHECK(std::next(root.begin(), 2).key() == "justify"); // where "from" was
	CHECK(root["children"][1]["justify"] == "center");    // a stack's "align"
	CHECK(screen["styles"]["toolbar"]["justify"] == "end");
	CHECK(screen["state"]["from"] == "kept: data is never rewritten");
	CHECK(root["children"][2]["text"] == "{{length(state.cards)}} cards, 'len(' stays in quotes: {{'len(x)'}}");
	CHECK(root["children"][3]["text"] == "len(state.cards) outside a placeholder is text");
	CHECK(rewrites.back().pointer == "/version");
	CHECK(rewrites.front().message.find("in version 2") != std::string::npos);
}

TEST_CASE("migrate: a revision's many compact edits keep their order, in time that grows with them" *
          doctest::timeout(affresco::test::timeLimit(5))) {
	std::string text = "{"; // parsed, as building it with operator[] would itself take quadratic time
	for(int i = 0; i < 200000; ++i) {
		text += "\"/k" + std::to_string(i) + "\": " + std::to_string(i) + ",";
		if(i == 1000) text += R"("#toolbar/from": "end",)";
		if(i == 2000) text += R"("#panel/from": "start", "#panel/justify": "end",)"; // "#panel/from" stays
	}
	text.back() = '}';
	const json set = affresco::parseUntrusted(text);
	json scenario = {
	    {"format", "affrescoui-scenarios"}, {"version", 1}, {"steps", json::array({{{"revision", {{"set", set}}}}})}};
	const std::vector<Rewrite> rewrites = migrate(scenario);
	const json& migrated = scenario["steps"][0]["revision"]["set"];
	REQUIRE(migrated.size() == set.size());
	auto expected = set.begin();
	for(auto it = migrated.begin(); it != migrated.end(); ++it, ++expected) {
		REQUIRE(it.key() == (expected.key() == "#toolbar/from" ? "#toolbar/justify" : expected.key()));
		REQUIRE(it.value() == expected.value());
	}
	CHECK(rewrites.size() == 2); // the rename and the version
}

TEST_CASE("migrate: a stack with both keys keeps \"justify\", and other documents are left") {
	json both = {{"format", "affrescoui-screen"},
	             {"version", 1},
	             {"root", {{"type", "stack"}, {"align", "end"}, {"justify", "start"}}}};
	const std::vector<Rewrite> rewrites = migrate(both);
	REQUIRE(rewrites.size() == 2);
	CHECK(rewrites[0].kind == Rewrite::Kind::Remove);
	CHECK(both["root"] == json{{"type", "stack"}, {"justify", "start"}});

	json other = {{"format", "something-else"}, {"version", 1}, {"from", "x"}};
	CHECK(migrate(other).empty());
	CHECK(documentKind(other) == DocumentKind::Other);

	json patch = json::parse(R"([{"op": "replace", "path": "#bar/from", "value": "end"},
	                             {"op": "replace", "path": "/state/from", "value": "kept"}])");
	CHECK(migratePatch(patch).size() == 1);
	CHECK(patch[0]["path"] == "#bar/justify");
	CHECK(patch[1]["path"] == "/state/from");
}

TEST_CASE("migrate: the hints name the version 2 form") {
	CHECK(removedKeyHint("row", "from") == "'from' was removed in version 2: use 'justify'");
	CHECK(removedKeyHint("stack", "align").find("'justify'") != std::string::npos);
	CHECK(removedKeyHint("row", "gap").empty());
	CHECK(removedFunctionHint("len") == "'len' was removed in version 2: use 'length'");
	CHECK(removedFunctionHint("length").empty());
}

TEST_CASE("migrate: the loader reads version 1, and a revision with a removed key is refused with a hint") {
	const affresco::detail::ManualClock clock;
	const auto headless = affresco::detail::createHeadlessWindow(400, 300, clock); // a screen checks keys once built
	affresco::Screen screen = affresco::Screen::fromString(R"({"format": "affrescoui-screen", "version": 1,
		"root": {"type": "column", "children": [{"type": "row", "id": "bar", "from": "end", "children": []}]}})");
	const json document = affresco::parseUntrusted(screen.documentJson());
	CHECK(document["version"] == 2);
	CHECK(document["root"]["children"][0]["justify"] == "end");

	screen.build(*headless);
	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#bar/from": "start"}})", &error));
	CHECK(error.find("'from' was removed in version 2: use 'justify'") != std::string::npos);
	CHECK(screen.applyRevision(R"({"set": {"#bar/justify": "start"}})", &error));
}
