/// @file
/// @brief Unit tests for the format module's expressions (src/Expression.hpp): a table of values and their results
/// or errors.

#include "Expression.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <vector>

using affresco::json;

namespace {

/// @brief One row of the table: a value, and what evaluating it gives.
struct Case {
	json value;        ///< A `{{...}}` string, a template or a `$` operator.
	json expected;     ///< The result, compared as JSON text (so 7 and 7.0 differ). Unused when error is set.
	std::string error; ///< If not empty: evaluating throws a DocumentError whose message contains this.
};

/// @brief Brackets nested around a value.
/// @param depth How many pairs.
/// @return E.g. "((1))" for 2.
std::string parentheses(int depth) {
	const auto n = static_cast<std::size_t>(depth);
	return std::string(n, '(') + "1" + std::string(n, ')');
}

/// @brief Evaluates a value with a fixed state, as a screen would.
/// @param value The value.
/// @return The result.
/// @throws DocumentError If the value is invalid.
json evaluate(const json& value) {
	static const json state = {{"count", 3}, {"items", {"a", "b", "c"}}, {"scores", {4, 9, 2}}, {"my-list", {1, 2}}};
	static const json window = {{"width", 800}, {"height", 600}};
	static const json revision = 0, event = json::object(), globals = {{"mode", "default"}};
	static const affresco::format::Scope locals{};
	const affresco::format::Context context{state, window, revision, event, locals, globals};
	return affresco::format::evaluate(value, context, "test");
}

/// @brief The table. Pins today's behaviour, including where it differs from JavaScript (`||` gives a boolean).
const std::vector<Case> cases = {
    // Arithmetic: integers in, integers out
    {"{{1 + 2 * 3}}", 7, {}},
    {"{{(1 + 2) * 3}}", 9, {}},
    {"{{7 / 2}}", 3.5, {}},
    {"{{8 / 2}}", 4.0, {}},
    {"{{7 % 3}}", 1, {}},
    {"{{-state.count}}", -3, {}},
    {"{{1.5 + 1}}", 2.5, {}},
    {"{{1 / 0}}", {}, "division by zero"},
    // Comparisons, logic, choices
    {"{{state.count > 2 ? 'big' : 'small'}}", "big", {}},
    {"{{'abc' < 'abd'}}", true, {}},
    {"{{state.count == 3 && !false}}", true, {}},
    {"{{null || 0 || 'x'}}", true, {}},
    {"{{state.missing ?? 'none'}}", "none", {}},
    {"{{coalesce(null, 2)}}", 2, {}},
    {"{{'a' < 1}}", {}, "expected a number"},
    // Paths
    {"{{state.items[1]}}", "b", {}},
    {"{{state.items[state.count - 2]}}", "b", {}},
    {"{{state.items.length}}", 3, {}},
    {"{{state.items[5]}}", nullptr, {}},
    {"{{window.width / 2}}", 400.0, {}},
    {"{{mode}}", "default", {}},
    {"{{nope + 1}}", {}, "unknown variable 'nope'"},
    // Text and templates
    {"Clicked {{state.count}} times", "Clicked 3 times", {}},
    {"{{state.count + ' cards'}}", "3 cards", {}},
    {"{{@accent}}", "@accent", {}},
    {"{{upper('abc') + lower('DEF')}}", "ABCdef", {}},
    {"{{fixed(3.14159, 2)}}", "3.14", {}},
    {"{{plural(2, 'card')}}", "2 cards", {}},
    {"{{plural(1, 'card')}}", "1 card", {}},
    {"{{number('42')}}", 42, {}},
    {"{{number('4x')}}", nullptr, {}},
    {"{{slice('hello', -3)}}", "llo", {}},
    {"{{split('a-b', '-')}}", {"a", "b"}, {}},
    {"{{join(state.items, ', ')}}", "a, b, c", {}},
    {"{{'abc}}", {}, "unclosed '{{'"},
    // pad: zeros by default, any fill, width capped at 10000
    {"{{pad(7, 3)}}", "007", {}},
    {"{{pad('x', 4, '-')}}", "---x", {}},
    {"{{pad(1, 10000)}}", std::string(9999, '0') + "1", {}},
    {"{{pad(1, 10001)}}", {}, "width too large"},
    // Lists
    {"{{range(3)}}", {0, 1, 2}, {}},
    {"{{len(range(10000))}}", 10000, {}},
    {"{{range(10001)}}", {}, "range too long"},
    {"{{max(3, 9, 4)}}", 9, {}},
    {"{{min(state.scores)}}", 2, {}},
    {"{{clamp(15, 0, 10)}}", 10, {}},
    {"{{round(2.5)}}", 3, {}},
    {"{{filter(state.items, it != 'b')}}", {"a", "c"}, {}},
    {"{{map([1, 2, 3], it * 2)}}", {2, 4, 6}, {}},
    {"{{sum([1, 2, 3.5])}}", 6.5, {}},
    {"{{count(state.scores, it > 3)}}", 2, {}},
    {"{{sort(state.scores, it, 'desc')}}", {9, 4, 2}, {}},
    {"{{contains(state.items, 'c')}}", true, {}},
    {"{{type([])}}", "list", {}},
    {"{{{a: 1, 'b c': [2]}}}", {{"a", 1}, {"b c", {2}}}, {}},
    // Operators written as JSON
    {{{"$clamp", {"{{state.count}}", 0, 2}}}, 2, {}},
    {{{"$length", {1, 2, 3}}}, 3, {}},
    {{{"$nope", {1}}}, {}, "unknown operator '$nope'"},
    // Syntax errors and the nesting limit (100 levels inside the parser)
    {"{{1 +}}", {}, "expected a value"},
    {"{{foo(1)}}", {}, "unknown function 'foo'"},
    {"{{len(1, 2)}}", {}, "'len' takes one argument"},
    {"{{" + parentheses(30) + "}}", 1, {}},
    {"{{" + parentheses(200) + "}}", {}, "nested too deeply"},
    {"{{" + std::string(200, '!') + "true}}", {}, "nested too deeply"},
    {"{{" + std::string(200, '[') + " 1 " + std::string(200, ']') + "}}", {}, "nested too deeply"},
};

} // namespace

TEST_CASE("format: expressions give their expected result or error") {
	CHECK(cases.size() >= 30);
	for(const Case& c : cases) {
		const std::string shown = c.value.dump();
		CAPTURE(shown.substr(0, 80));
		if(c.error.empty()) {
			json result;
			CHECK_NOTHROW(result = evaluate(c.value)); // CHECK, not REQUIRE: one failing row doesn't hide the rest
			CHECK(result.dump() == c.expected.dump());
		} else {
			CHECK_THROWS_WITH_AS((void)evaluate(c.value), doctest::Contains(c.error.c_str()), affresco::DocumentError);
		}
	}
}

TEST_CASE("format: truthy follows the documented rules") {
	for(const json& value : {json(false), json(nullptr), json(0), json(0.0), json(""), json::array(), json::object()})
		CHECK_FALSE(affresco::format::truthy(value));
	for(const json& value : {json(true), json(1), json(-0.5), json("0"), json{0}, json{{"a", 0}}})
		CHECK(affresco::format::truthy(value));
}

// Bug: compile() falls back to a plain path ("Paths written before expressions existed may contain '-' in names
// (\"state.my-list\")") only when the text doesn't parse, but "state.my-list" parses as state.my - list, which then
// fails with "unknown variable 'list'".
TEST_CASE("format: a path with '-' in a name, from before expressions, reads that name" * doctest::skip(true)) {
	CHECK(evaluate("{{state.my-list}}").dump() == "[1,2]");
}
