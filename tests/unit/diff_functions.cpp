/// @file
/// @brief The diff and JSON expression functions (format's ExpressionFunctionsDiff): diffLines() as unified diffs
/// write it, atPointer(), applyPatch() and formatJson().

#include "Expression.hpp"

#include <affresco/core/Json.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;

namespace {

/// @brief Evaluates an expression with a state.
/// @param expression The expression, without its braces.
/// @param state What it sees as `state`.
/// @return Its value.
json evaluate(const std::string& expression, const json& state = json::object()) {
	static const json window = {{"width", 800}, {"height", 600}}, revision = 1;
	static const json event = json::object(), globals = json::object();
	static const affresco::format::Scope locals{};
	const affresco::format::Context context{state, window, revision, event, locals, globals};
	return affresco::format::evaluate("{{" + expression + "}}", context, "test");
}

/// @brief A text of numbered lines, "1\n2\n...".
/// @param count How many.
/// @param changed A line to write as "x" instead, or 0.
/// @return The text.
std::string numbered(int count, int changed = 0) {
	std::string text;
	for(int i = 1; i <= count; ++i) text += (i == changed ? std::string("x") : std::to_string(i)) + "\n";
	return text;
}

} // namespace

TEST_CASE("diff functions: diffLines gives hunks with unified-diff headers and numbered lines") {
	const json state = {{"a", numbered(10, 0)}, {"b", numbered(10, 5)}};
	const json diff = evaluate("diffLines(state.a, state.b)", state);
	REQUIRE(diff["hunks"].size() == 1);
	const json& hunk = diff["hunks"][0];
	CHECK(hunk["header"] == "@@ -2,7 +2,7 @@");
	CHECK(hunk["hiddenBefore"] == 1);
	CHECK(diff["hiddenAfter"] == 2);
	REQUIRE(hunk["lines"].size() == 8);
	CHECK(hunk["lines"][0] == json{{"kind", "context"}, {"old", 2}, {"new", 2}, {"text", "2"}});
	CHECK(hunk["lines"][3] == json{{"kind", "delete"}, {"old", 5}, {"new", nullptr}, {"text", "5"}});
	CHECK(hunk["lines"][4] == json{{"kind", "insert"}, {"old", nullptr}, {"new", 5}, {"text", "x"}});
	CHECK(diff["added"] == 1);
	CHECK(diff["removed"] == 1);
	CHECK(diff["same"] == 9);
	CHECK(diff["truncated"] == false);
	CHECK(evaluate("diffLines('same', 'same').hunks") == json::array());
}

TEST_CASE("diff functions: diffLines writes empty ranges and single lines as unified diffs do") {
	CHECK(evaluate("diffLines('', 'a\\nb\\n').hunks[0].header") == "@@ -0,0 +1,2 @@");
	CHECK(evaluate("diffLines('a\\nb\\n', '').hunks[0].header") == "@@ -1,2 +0,0 @@");
	CHECK(evaluate("diffLines('a\\nb\\nc', 'a\\nb\\nc\\nd', { context: 0 }).hunks[0].header") == "@@ -3,0 +4 @@");
	CHECK(evaluate("diffLines('a', 'b').hunks[0].header") == "@@ -1 +1 @@");
}

TEST_CASE("diff functions: diffLines' context joins nearby changes, and -1 shows the whole text") {
	const json state = {{"a", numbered(20)}, {"b", numbered(20, 4)}};
	json b = state;
	std::string changed = state["b"].get<std::string>();
	b["b"] = changed.replace(changed.find("\n10\n") + 1, 2, "y");
	CHECK(evaluate("length(diffLines(state.a, state.b, { context: 3 }).hunks)", b) == 1); // 5 lines apart
	CHECK(evaluate("length(diffLines(state.a, state.b, { context: 2 }).hunks)", b) == 2);
	const json whole = evaluate("diffLines(state.a, state.b, { context: -1 })", b);
	REQUIRE(whole["hunks"].size() == 1);
	CHECK(whole["hunks"][0]["lines"].size() == 22);
	CHECK(whole["hunks"][0]["header"] == "@@ -1,20 +1,20 @@");
}

TEST_CASE("diff functions: diffLines can ignore whitespace and gives side-by-side rows") {
	const json state = {{"a", "if(x) {\n  run();\n}\n"}, {"b", "if(x) {\n    run();\n  stop();\n}\n"}};
	const json plain = evaluate("diffLines(state.a, state.b)", state);
	CHECK(plain["removed"] == 1);
	const json loose = evaluate("diffLines(state.a, state.b, { ignoreWhitespace: true, split: true })", state);
	CHECK(loose["removed"] == 0);
	CHECK(loose["added"] == 1);
	const json& rows = loose["hunks"][0]["rows"];
	REQUIRE(rows.size() == 4);
	CHECK(rows[1]["left"] == json{{"n", 2}, {"text", "  run();"}, {"kind", "context"}});
	CHECK(rows[1]["right"] == json{{"n", 2}, {"text", "    run();"}, {"kind", "context"}});
	CHECK(rows[2]["left"] == nullptr);
	CHECK(rows[2]["right"]["kind"] == "insert");
	const json paired = evaluate("diffLines('a\\nb\\nc', 'a\\nB\\nc', { split: true }).hunks[0].rows[1]");
	CHECK(paired["left"] == json{{"n", 2}, {"text", "b"}, {"kind", "delete"}});
	CHECK(paired["right"] == json{{"n", 2}, {"text", "B"}, {"kind", "insert"}});
}

TEST_CASE("diff functions: diffLines stops at its limit and refuses options that aren't an object") {
	const json state = {{"a", ""}, {"b", numbered(50)}};
	const json diff = evaluate("diffLines(state.a, state.b, { limit: 10 })", state);
	CHECK(diff["truncated"] == true);
	CHECK(diff["hunks"][0]["lines"].size() == 10);
	CHECK(diff["hunks"][0]["header"] == "@@ -0,0 +1,50 @@"); // the header still says what the hunk holds
	CHECK(diff["added"] == 50);
	CHECK_THROWS_AS(evaluate("diffLines('a', 'b', 3)"), affresco::DocumentError);
}

TEST_CASE("diff functions: atPointer reads JSON Pointers, escapes included") {
	const json state = {{"doc", {{"nodes", {{{"id", "a"}}, {{"id", "b"}}}}, {"a/b", 1}, {"m~n", 2}}}};
	CHECK(evaluate("atPointer(state.doc, '/nodes/1/id')", state) == "b");
	CHECK(evaluate("atPointer(state.doc, '/a~1b')", state) == 1);
	CHECK(evaluate("atPointer(state.doc, '/m~0n')", state) == 2);
	CHECK(evaluate("atPointer(state.doc, '')", state) == state["doc"]);
	CHECK(evaluate("atPointer(state.doc, '/nodes/01')", state) == nullptr);
	CHECK(evaluate("atPointer(state.doc, '/nodes/2')", state) == nullptr);
	CHECK(evaluate("atPointer(state.doc, '/nodes/-')", state) == nullptr);
	CHECK(evaluate("atPointer(state.doc, 'nodes')", state) == nullptr);
	CHECK(evaluate("atPointer(state.doc, '/missing/x')", state) == nullptr);
}

TEST_CASE("diff functions: applyPatch applies a JSON Patch, or gives null") {
	const json state = {{"doc", {{"a", 1}, {"list", {1, 2}}}},
	                    {"patch", json::parse(R"([{"op": "replace", "path": "/a", "value": 2},
	                                             {"op": "add", "path": "/list/-", "value": 3},
	                                             {"op": "remove", "path": "/list/0"}])")}};
	CHECK(evaluate("applyPatch(state.doc, state.patch)", state) == json{{"a", 2}, {"list", {2, 3}}});
	CHECK(evaluate("applyPatch(state.doc, [{ op: 'remove', path: '/nope' }])", state) == nullptr);
	CHECK(evaluate("applyPatch(state.doc, [{ op: 'test', path: '/a', value: 5 }])", state) == nullptr);
	CHECK(evaluate("applyPatch(state.doc, 'not a patch')", state) == nullptr);
	CHECK(evaluate("applyPatch(state.doc, [])", state) == state["doc"]);
	// Copies of copies double: the patch stops once it has copied too much
	json doubling = json::array({{{"op", "add"}, {"path", "/x"}, {"value", json::array({0, 0, 0, 0})}}});
	for(int i = 0; i < 40; ++i) doubling.push_back({{"op", "copy"}, {"from", "/x"}, {"path", "/x/0"}});
	CHECK(evaluate("applyPatch({}, state.patch)", {{"patch", doubling}}) == nullptr);
}

TEST_CASE("diff functions: formatJson writes one member per line, in the document's order") {
	CHECK(evaluate("formatJson({ b: 1, a: [true] })") == "{\n  \"b\": 1,\n  \"a\": [\n    true\n  ]\n}");
	CHECK(evaluate("formatJson({ b: 1 }, 4)") == "{\n    \"b\": 1\n}");
	CHECK(evaluate("formatJson({ b: 1 }, 0)") == "{\"b\":1}");
	CHECK(evaluate("formatJson(null)") == "null");
}
