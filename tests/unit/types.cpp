/// @file
/// @brief Typed state ("stateTypes"): types read and described, values matched against them, the rules that refuse a
/// document or a revision that breaks them, and the checker's findings in writes, bindings and comparisons.

#include "Checker.hpp"
#include "StateTypes.hpp"

#include <affresco/format/Rules.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using affresco::json;
using namespace affresco::format;

namespace {

/// @brief The first mismatch of a value, as one line.
/// @param value The value, as JSON text.
/// @param type The type, as JSON text.
/// @return "<place>: <problem>", or "" if it matches.
std::string mismatchOf(const char* value, const char* type) {
	const auto found = typeMismatch(json::parse(value), json::parse(type), "state.x");
	return found ? found->text() : "";
}

/// @brief A screen with typed state.
/// @param state The state.
/// @param types Its "stateTypes".
/// @param extra More sections, as JSON text after a comma, or "".
/// @return The document.
json typedScreen(const char* state, const char* types, const std::string& extra = "") {
	return json::parse(std::string(R"({"version": 1, "state": )") + state + R"(, "stateTypes": )" + types + extra +
	                   R"(, "root": {"id": "root"}})");
}

/// @brief The checker's findings in a screen, against its own state.
/// @param document The document.
/// @return One line each.
std::vector<std::string> findingsOf(const json& document) {
	std::vector<std::string> lines;
	const auto found = check(document, document.value("state", json::object()), {});
	REQUIRE(found);
	if(!found) return lines;
	for(const Finding& finding : *found) lines.push_back(finding.text());
	return lines;
}

} // namespace

TEST_CASE("types: a type is checked when the screen is read, and an error names its place") {
	CHECK_NOTHROW(checkType(json::parse(R"({"type": ["string", "null"], "maxLength": 20, "note": "a name"})"), "t"));
	CHECK_NOTHROW(checkType(json(true), "t"));
	CHECK_THROWS_WITH_AS(checkType(json::parse(R"({"type": "text"})"), "stateTypes.name"),
	                     doctest::Contains("stateTypes.name.type: unknown type"), affresco::DocumentError);
	CHECK_THROWS_WITH_AS(checkType(json::parse(R"({"pattern": "^a"})"), "stateTypes.name"),
	                     doctest::Contains("stateTypes.name.pattern: unknown keyword"), affresco::DocumentError);
	CHECK_THROWS_WITH_AS(checkType(json::parse(R"({"properties": {"a": {"minItems": -1}}})"), "stateTypes.x"),
	                     doctest::Contains("stateTypes.x.properties.a.minItems"), affresco::DocumentError);
	CHECK_THROWS_AS(checkType(json::parse(R"({"enum": []})"), "t"), affresco::DocumentError);
	CHECK_THROWS_AS(checkType(json::parse(R"({"required": "a"})"), "t"), affresco::DocumentError);

	json deep = true;
	for(int i = 0; i <= maxTypeDepth + 1; ++i) deep = {{"items", deep}};
	CHECK_THROWS_WITH_AS(checkType(deep, "t"), doctest::Contains("nested too deeply"), affresco::DocumentError);
}

TEST_CASE("types: a value matches a type, or the first mismatch says where and why") {
	CHECK(mismatchOf("3", R"({"type": "number"})").empty());
	CHECK(mismatchOf("3.0", R"({"type": "integer"})").empty());
	CHECK(mismatchOf("3.5", R"({"type": "integer"})") == "state.x: expected a whole number, got 3.5");
	CHECK(mismatchOf(R"("a")", R"({"type": "number"})") == R"(state.x: expected a number, got "a")");
	CHECK(mismatchOf("null", R"({"type": ["string", "null"]})").empty());
	CHECK(mismatchOf(R"("bugs")", R"({"enum": ["all", "bug"]})") ==
	      R"(state.x: expected one of "all", "bug", got "bugs")");
	CHECK(mismatchOf("4", R"({"const": 3})") == "state.x: expected 3, got 4");
	CHECK(mismatchOf("120", R"({"type": "number", "maximum": 100})") == "state.x: expected at most 100, got 120");
	CHECK(mismatchOf("-1", R"({"minimum": 0})") == "state.x: expected at least 0, got -1");
	CHECK(mismatchOf(R"("héllo")", R"({"maxLength": 5})").empty()); // characters, not bytes
	CHECK(mismatchOf(R"("ab")", R"({"minLength": 3})") == "state.x: expected at least 3 characters, got 2");
	CHECK(mismatchOf("[1, 2, 3]", R"({"maxItems": 2})") == "state.x: expected at most 2 items, got 3");
	CHECK(mismatchOf(R"([1, "two"])", R"({"type": "array", "items": {"type": "number"}})") ==
	      R"(state.x[1]: expected a number, got "two")");
	CHECK(mismatchOf(R"({"a": 1})", R"({"required": ["a", "b"]})") == "state.x: missing 'b'");
	CHECK(mismatchOf(R"({"a": 1, "c": 2})", R"({"properties": {"a": {}}, "additionalProperties": false})") ==
	      "state.x.c: not one of its properties (a)");
	CHECK(mismatchOf(R"({"a": 1, "c": "x"})", R"({"additionalProperties": {"type": "number"}})") ==
	      R"(state.x.c: expected a number, got "x")");
	CHECK(mismatchOf("1", "false") == "state.x: nothing may be here");
}

TEST_CASE("types: a type is described in a few words, for messages and outlines") {
	CHECK(describeType(json::parse(R"({"type": "number"})")) == "number");
	CHECK(describeType(json::parse(R"({"type": ["string", "null"]})")) == "text | null");
	CHECK(describeType(json::parse(R"({"enum": ["all", "bug", 3]})")) == "'all' | 'bug' | 3");
	CHECK(describeType(json::parse(R"({"type": "array", "items": {"type": "object", "properties": {"title":
		{"type": "string"}, "done": {"type": "boolean"}}}})")) == "list of {title: text, done: boolean}");
	CHECK(describeType(json(true)) == "anything");
	CHECK(describeKinds(static_cast<Kinds>(bit(Kind::Text) | bit(Kind::Null))) == "text or null");
	CHECK(allowedKinds(json::parse(R"({"enum": ["a", 1.5]})")) == (bit(Kind::Text) | bit(Kind::Fraction)));
}

TEST_CASE("types: the rules refuse a screen whose state doesn't match its types, naming the place") {
	const Rules rules;
	CHECK_NOTHROW(rules.validate(typedScreen(R"({"count": 2})", R"({"count": {"type": "number"}})")));
	CHECK_THROWS_WITH_AS(rules.validate(typedScreen(R"({"count": "two"})", R"({"count": {"type": "number"}})")),
	                     doctest::Contains(R"(state.count: expected a number, got "two" (stateTypes.count))"),
	                     affresco::DocumentError);
	CHECK_THROWS_WITH_AS(rules.validate(typedScreen("{}", R"({"count": {"type": "nummber"}})")),
	                     doctest::Contains("stateTypes.count.type"), affresco::DocumentError);
}

TEST_CASE("types: a revision may not bring a value that doesn't match, nor a type the state doesn't match") {
	const Rules rules;
	const json before = typedScreen(R"({"filter": "all"})", R"({"filter": {"enum": ["all", "bug"]}})");

	json allowed = before;
	allowed["state"]["filter"] = "bug";
	CHECK_NOTHROW(rules.checkChange(before, allowed));

	json wrongValue = before;
	wrongValue["state"]["filter"] = "bugs";
	CHECK_THROWS_WITH_AS(rules.checkChange(before, wrongValue),
	                     doctest::Contains(R"(the state wouldn't match its types: state.filter: expected one of)"),
	                     affresco::DocumentError);

	json wrongType = before;
	wrongType["stateTypes"]["filter"] = {{"type", "number"}};
	CHECK_THROWS_AS(rules.checkChange(before, wrongType), affresco::DocumentError);

	// A mismatch that was there before (a state kept on disk) doesn't block another change
	json old = before;
	old["state"]["filter"] = "bugs";
	json other = old;
	other["state"]["count"] = 1;
	CHECK_NOTHROW(rules.checkChange(old, other));
	const std::vector<std::string> warned = rules.findings(old, old["state"]);
	REQUIRE_FALSE(warned.empty());
	CHECK(warned.front().starts_with("state.filter: expected one of"));
}

TEST_CASE("types: the checker finds writes, bindings and comparisons that don't fit a declared type") {
	const json document = typedScreen(
	    R"({"filter": "all", "count": 0, "name": "", "user": {"first": "Ada"}})",
	    R"({"filter": {"enum": ["all", "bug"]}, "count": {"type": "number"}, "name": {"type": "string"},
	        "user": {"type": "object", "properties": {"first": {"type": "string"}, "last": {"type": "string"}}},
	        "later": {"type": "array", "items": {"type": "number"}}})",
	    R"(, "actions": {
	        "pick": [{"set": "state.filter", "to": "bugs"}],
	        "flip": [{"toggle": "state.count"}],
	        "bump": [{"increment": "state.name"}],
	        "fill": [{"push": "state.count", "value": 1}, {"push": "state.later", "value": "x"}],
	        "copy": [{"set": "state.count", "to": "{{state.name}}"}],
	        "fine": [{"set": "state.count", "to": "{{state.count + 1}}"}, {"push": "state.later", "value": 2}]
	    })");
	json screen = document;
	screen["root"]["children"] = json::parse(R"([
		{"id": "volume", "type": "slider", "bind": "state.name"},
		{"id": "shown", "type": "label", "text": "{{state.filter == 'bugs' ? state.user.last : state.user.frst}}"},
		{"id": "ready", "type": "label", "text": "{{len(state.later)}}", "when": "{{state.filter != 'all'}}"},
		{"id": "go", "onClick": ["pick", "flip", "bump", "fill", "copy", "fine"]}
	])");
	screen["root"]["children"][3]["onClick"] = json::parse(
	    R"([{"run": "pick"}, {"run": "flip"}, {"run": "bump"}, {"run": "fill"}, {"run": "copy"}, {"run": "fine"}])");

	const std::vector<std::string> found = findingsOf(screen);
	const auto has = [&found](const std::string& text) {
		return std::ranges::any_of(found, [&](const std::string& line) { return line.find(text) != std::string::npos; });
	};
	CHECK(has(R"(actions.pick[0].to: state.filter: expected one of "all", "bug", got "bugs" (stateTypes.filter))"));
	CHECK(has("actions.flip[0].toggle: 'state.count' takes number, but a toggle writes true or false"));
	CHECK(has("actions.bump[0].increment: 'state.name' takes text, but it writes a number"));
	CHECK(has("actions.fill[0].push: 'state.count' takes number, but a push needs a list"));
	CHECK(has(R"(actions.fill[1].value: state.later[...]: expected a number, got "x")"));
	CHECK(has("actions.copy[0].to: 'state.count' takes number, but this writes text"));
	CHECK(has("root.children[0].bind: 'state.name' takes text, but a slider writes a number"));
	CHECK(has("'state.filter' is never \"bugs\": it takes 'all' | 'bug'"));
	CHECK(has("'state.user.frst' doesn't exist"));
	CHECK_FALSE(has("actions.fine"));
	CHECK_FALSE(has("state.later' doesn't exist")); // declared, though the state doesn't hold it yet
	CHECK_FALSE(has("state.user.last")); // declared, though the state doesn't hold it
}
