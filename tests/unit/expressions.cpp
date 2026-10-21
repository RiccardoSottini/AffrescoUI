/// @file
/// @brief Unit tests for the format module's expressions (src/Expression.hpp): a table of values and their results
/// or errors, the function registry, the value type (src/Value.hpp) and the compile cache.

#include "Expression.hpp"
#include "ExpressionFunctions.hpp"
#include "Value.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
using affresco::format::Value;

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

/// @brief Parses JSON written in a row.
/// @param text The JSON.
/// @return The value.
json J(std::string_view text) { return json::parse(text); }

/// @brief A computed value or a node, read by name: what a screen's runtime gives for `computed.<name>` and
/// `nodes.<id>`.
/// @param name The name.
/// @return The value, or null.
json computedValue(std::string_view name) {
	if(name == "total") return 12;
	if(name == "list") return J(R"([1, 2, {"a": "b"}])");
	return nullptr;
}

/// @brief A built panel's geometry, by id.
/// @param id The id.
/// @return The geometry, or null.
json nodeGeometry(std::string_view id) {
	return id == "board" ? J(R"({"x": 0, "y": 0, "width": 800, "height": 600, "visible": true})") : json();
}

/// @brief Evaluates a value with a fixed state, as a screen would.
/// @param value The value.
/// @return The result.
/// @throws DocumentError If the value is invalid.
json evaluate(const json& value) {
	static const json state = J(R"({"count": 3, "items": ["a", "b", "c"], "scores": [4, 9, 2], "my-list": [1, 2],
	    "name": "Ünïcødé ✓", "big": 18446744073709551615, "negative": -7, "half": 0.5, "flag": true, "none": null,
	    "empty": "", "cards": [{"title": "B", "points": 2, "done": true}, {"title": "a", "points": 5, "done": false}],
	    "object": {"z": 1, "a": 2, "m": {"k": "v"}}, "nested": {"list": [1, [2, 3], {"x": 4}]}})");
	static const json window = {{"width", 800}, {"height", 600}};
	static const json revision = 3, event = {{"x", 10}, {"key", "Enter"}};
	static const json globals = {{"mode", "default"}, {"theme", {{"name", "light"}}}, {"proposals", json::array()}};
	static const affresco::format::Scope locals{
	    nullptr, nullptr, {{"card", {{"title", "x"}, {"points", 3}}}, {"it", "outer"}}, {}, {}};
	static const affresco::format::NodeLookup nodes = nodeGeometry, computed = computedValue;
	const affresco::format::Context context{state, window, revision, event, locals, globals, &nodes, &computed};
	return affresco::format::evaluate(value, context, "test");
}

/// @brief The table. Pins the behaviour of every function and error; where it differs from JavaScript, it says so.
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
    // Numbers too large to hold: found by fuzzing (tests/fuzz, expression), they threw std::out_of_range
    {"{{9223372036854775807}}", INT64_MAX, {}},
    {"{{0 - 9223372036854775807 - 1}}", INT64_MIN, {}}, // the smallest 64-bit integer stays an integer
    {"{{9223372036854775808 + 1}}", {}, "number '9223372036854775808' is too large"},
    {"{{1 + " + std::string(400, '9') + ".5}}", {}, "is too large (at column 407)"},
    {"{{state.items[99999999999999999999]}}", {}, "index '99999999999999999999' is too large"},
    // Whole results beyond 64 bits: found by fuzzing, they were cast to an integer (undefined behaviour)
    {"{{3000000000000 * 3000000000000}}", 9e24, {}},
    {"{{number('99999999999999999999999')}}", 1e23, {}},
    {"{{round(10000000000 * 10000000000 * 0.5)}}", 5e19, {}},
    {"{{floor(-1.5)}}", -2, {}},
    {"{{ceil(1.2)}}", 2, {}},
    {"{{range(10000000000 * 10000000000)}}", {}, "range too long"},
    {"{{slice(state.items, 0 - 10000000000 * 10000000000)}}", json::array({"a", "b", "c"}), {}},
    {"{{fixed(1, 10000000000 * 10000000000)}}", "1.000000000000", {}},
    {"{{pad('a', 10000000000 * 10000000000)}}", {}, "width too large"},
    // Comparisons, logic, choices
    {"{{state.count > 2 ? 'big' : 'small'}}", "big", {}},
    {"{{'abc' < 'abd'}}", true, {}},
    {"{{state.count == 3 && !false}}", true, {}},
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
    // Syntax errors and the nesting limit (100 levels inside the parser: a bracket takes two, a '!' one)
    {"{{1 +}}", {}, "expected a value"},
    {"{{foo(1)}}", {}, "unknown function 'foo'"},
    {"{{len(1, 2)}}", {}, "'len' takes one argument"},
    // Numbers too large for their type are errors, not exceptions that escape (they used to: std::out_of_range)
    {"{{99999999999999999999}}", {}, "number '99999999999999999999' is too large"},
    {"{{1" + std::string(400, '0') + ".5}}", {}, "is too large (at column 404)"},
    {{{"$path", "items[99999999999999999999]"}}, {}, "index '99999999999999999999' is too large"},
    {"{{" + parentheses(30) + "}}", 1, {}},
    {"{{" + parentheses(49) + "}}", 1, {}},
    {"{{" + parentheses(50) + "}}", {}, "nested too deeply"},
    {"{{" + parentheses(200) + "}}", {}, "nested too deeply"},
    {"{{" + std::string(98, '!') + "true}}", true, {}},
    {"{{" + std::string(99, '!') + "true}}", {}, "nested too deeply"},
    {"{{" + std::string(200, '!') + "true}}", {}, "nested too deeply"},
    {"{{" + std::string(200, '[') + " 1 " + std::string(200, ']') + "}}", {}, "nested too deeply"},
    // A "$path" that isn't text was a json::type_error, which the runtime doesn't catch: now it is a document error
    {J(R"({"$path": 5})"), {}, "test.$path: expected a path written as text"},
    // `||` and `&&` give an operand, like JavaScript (D1): the first that decides, else the last
    {"{{null || 0 || 'x'}}", "x", {}},
    {"{{card && card.title}}", "x", {}},
    {"{{state.missing && state.missing.name}}", nullptr, {}},
    {"{{0 && 'x'}}", 0, {}},
    {"{{'' || null}}", nullptr, {}},
    {"{{false || 0}}", 0, {}},
    {"{{'a' && 'b' && 'c'}}", "c", {}},
    {"{{state.count > 2 && 'big'}}", "big", {}},
    {J(R"({"$and": []})"), true, {}},
    {J(R"({"$or": []})"), false, {}},
    // Choices, logic and comparisons: every function, and the errors of each
    {"{{if(true, 1, 2)}}", 1, {}},
    {"{{if(true, 1)}}", {}, "test.$if: expected [condition, then, else]"},
    {J(R"({"$if": 5})"), {}, "test.$if: expected [condition, then, else]"},
    {"{{false ? 1 / 0 : 2}}", 2, {}},
    {"{{true || 1 / 0}}", true, {}},
    {"{{not(0)}}", true, {}},
    {"{{!state.items}}", false, {}},
    {"{{coalesce(null, state.none, 'z')}}", "z", {}},
    {"{{coalesce()}}", nullptr, {}},
    {J(R"({"$coalesce": 1})"), {}, "test.$coalesce: expected an array of values"},
    {J(R"({"$and": 1})"), {}, "test.$and: expected an array of conditions"},
    {"{{1 == 1.0}}", true, {}},
    {"{{[1, 2] == [1, 2]}}", true, {}},
    {"{{{a: 1, b: 2} == {b: 2, a: 1}}}", false, {}},
    {"{{'a' != 'b'}}", true, {}},
    {"{{equals(1)}}", {}, "test.$equals: expected 2 arguments"},
    {"{{2 >= 2}}", true, {}},
    {"{{'b' <= 'a'}}", false, {}},
    {"{{null < 1}}", {}, "test.$less: expected a number, got null"},
    {"{{greater(1, 2, 3)}}", {}, "test.$greater: expected 2 arguments"},
    {"{{empty(null)}}", true, {}},
    {"{{empty(0)}}", false, {}},
    {"{{empty(false)}}", false, {}},
    {"{{empty({})}}", true, {}},
    {"{{empty('x')}}", false, {}},
    {"{{type(null)}}", "null", {}},
    {"{{type(true)}}", "boolean", {}},
    {"{{type(1.5)}}", "number", {}},
    {"{{type('a')}}", "string", {}},
    {"{{type({})}}", "object", {}},
    // Numbers: arity, types, integers in and integers out, values past 64 bits
    {"{{add()}}", {}, "test.$add: expected at least one argument"},
    {"{{add(1)}}", 1, {}},
    {"{{1 + true}}", {}, "test.$add: expected a number, got true"},
    {"{{'n' + null}}", "n", {}},
    {"{{1 + 2.0}}", J(R"(3.0)"), {}},
    {"{{subtract(5)}}", {}, "test.$subtract: expected 2 arguments"},
    {"{{2 * 3.5}}", J(R"(7.0)"), {}},
    {"{{multiply()}}", {}, "test.$multiply: expected at least one argument"},
    {"{{7 / 0.5}}", J(R"(14.0)"), {}},
    {"{{-7 % 3}}", -1, {}},
    {"{{7 % 0}}", {}, "test.$mod: division by zero"},
    {"{{'a' / 0}}", {}, "test.$divide: division by zero"},
    {"{{max([])}}", {}, "test.$max: expected at least one argument"},
    {"{{max([2, 7])}}", 7, {}},
    {"{{min(5)}}", 5, {}},
    {"{{min('a', 1)}}", {}, "test.$min: expected a number, got \"a\""},
    {"{{clamp(5, 10, 0)}}", 10, {}},
    {"{{clamp(1.5, 0, 1)}}", J(R"(1.0)"), {}},
    {"{{clamp(1, 2)}}", {}, "test.$clamp: expected 3 arguments"},
    {"{{round(-2.5)}}", -3, {}},
    {"{{floor('x')}}", {}, "test.$floor: expected a number, got \"x\""},
    {"{{ceil(-0.5)}}", 0, {}},
    {"{{abs(-3)}}", 3, {}},
    {"{{abs(-2.5)}}", J(R"(2.5)"), {}},
    {"{{abs(0 - 9223372036854775807 - 1)}}", J(R"(9.223372036854776e+18)"), {}},
    {"{{9223372036854775807 + 1}}", J(R"(9.223372036854776e+18)"), {}},
    {"{{state.big}}", J(R"(18446744073709551615)"), {}},
    {"{{state.big + 1}}", J(R"(1.8446744073709552e+19)"), {}},
    // Texts and conversions; texts are UTF-8, and len, indexOf and slice count bytes
    {"{{upper('héllo')}}", "HéLLO", {}},
    {"{{lower('ÀB')}}", "Àb", {}},
    {"{{trim('  a b  ')}}", "a b", {}},
    {"{{text(null)}}", "", {}},
    {"{{text(2.0)}}", "2", {}},
    {"{{text(1.5)}}", "1.5", {}},
    {"{{text([1, 'a'])}}", "[1,\"a\"]", {}},
    {"{{number(' 42')}}", 42, {}},
    {"{{number('1e3')}}", J(R"(1000.0)"), {}},
    {"{{number('1e400')}}", nullptr, {}},
    {"{{number(true)}}", nullptr, {}},
    {"{{number(null)}}", nullptr, {}},
    {"{{fixed(2, 0)}}", "2", {}},
    {"{{fixed('x', 1)}}", {}, "test.$fixed: expected a number, got \"x\""},
    {"{{pad('é', 3)}}", "0é", {}},
    {"{{pad(5, 4, 'ab')}}", "abab5", {}},
    {"{{pad(5, 2, '')}}", "5", {}},
    {"{{pad(1)}}", {}, "test.$pad: expected 2 to 3 arguments"},
    {"{{plural(0, 'card')}}", "0 cards", {}},
    {"{{plural(3, 'box', 'boxes')}}", "3 boxes", {}},
    {"{{plural('x', 'card')}}", {}, "test.$plural: expected a number, got \"x\""},
    {"{{split('a,b,,c', ',')}}", J(R"(["a","b","","c"])"), {}},
    {"{{split('aé✓', '')}}", J(R"(["a","é","✓"])"), {}},
    {"{{split('', ',')}}", J(R"([""])"), {}},
    {"{{split('a')}}", {}, "test.$split: expected 2 arguments"},
    {"{{json({b: 1, a: [true, null]})}}", R"({"b":1,"a":[true,null]})", {}},
    {R"({{parseJson('{"z": 1, "a": 2}')}})", J(R"({"z":1,"a":2})"), {}},
    {"{{parseJson('nope')}}", nullptr, {}},
    {"{{parseJson(state.name)}}", nullptr, {}},
    // Lists and objects; objects keep the order of their keys, as the documents' JSON does
    {"{{len('é')}}", 2, {}},
    {"{{len(state.name)}}", 15, {}},
    {"{{len({a: 1})}}", 1, {}},
    {"{{len(null)}}", 0, {}},
    {"{{len(5)}}", {}, "test.$length: expected a string, a list or an object"},
    {"{{length([1])}}", 1, {}},
    {"{{concat([1], [2, 3])}}", J(R"([1,2,3])"), {}},
    {"{{concat('a', 1, null)}}", "a1", {}},
    {"{{concat()}}", J(R"([])"), {}},
    {"{{concat([1], 'a')}}", "[1]a", {}},
    {"{{join([], ', ')}}", "", {}},
    {"{{join(['', 'a', 'b'], '-')}}", "a-b", {}},
    {"{{join('ab', '-')}}", {}, "test.$join: expected a list first"},
    {"{{join([1])}}", {}, "test.$join: expected 2 arguments"},
    {"{{slice([1, 2, 3], 1)}}", J(R"([2,3])"), {}},
    {"{{slice([1, 2, 3], -2, -1)}}", J(R"([2])"), {}},
    {"{{slice('héllo', 0, 3)}}", "hé", {}},
    {"{{slice({a: 1}, 0)}}", {}, "test.$slice: expected a list or a text first"},
    {"{{slice([1], 'x')}}", {}, "test.$slice: expected a number, got \"x\""},
    {"{{contains('héllo', 'll')}}", true, {}},
    {"{{contains({a: 1}, 'a')}}", true, {}},
    {"{{contains(5, 5)}}", false, {}},
    {"{{contains(state.cards, {title: 'B', points: 2, done: true})}}", true, {}},
    {"{{indexOf('héllo', 'l')}}", 3, {}},
    {"{{indexOf([1, 2], 3)}}", -1, {}},
    {"{{keys({b: 1, a: 2})}}", J(R"(["b","a"])"), {}},
    {"{{keys([1])}}", J(R"([])"), {}},
    {"{{values({b: 1, a: 2})}}", J(R"([1,2])"), {}},
    {"{{entries({b: 1})}}", J(R"([{"key":"b","value":1}])"), {}},
    {"{{entries(null)}}", J(R"([])"), {}},
    {"{{merge({b: 1}, null, {a: 2, b: 3})}}", J(R"({"b":3,"a":2})"), {}},
    {"{{merge({a: 1}, 2)}}", {}, "test.$merge: expected objects"},
    {"{{merge()}}", J(R"({})"), {}},
    {"{{reverse([1, 2, 3])}}", J(R"([3,2,1])"), {}},
    {"{{reverse('aé✓')}}", "✓éa", {}},
    {"{{reverse(5)}}", {}, "test.$reverse: expected a list"},
    {"{{range(0)}}", J(R"([])"), {}},
    {"{{range(-3)}}", J(R"([])"), {}},
    {"{{range(2.9)}}", J(R"([0,1])"), {}},
    {"{{range('x')}}", {}, "test.$range: expected a number, got \"x\""},
    {"{{{b: 1, a: 2, b: 3}}}", J(R"({"b":3,"a":2})"), {}},
    // List functions: `it` and `index`, nesting, null lists, and the shape of their arguments
    {"{{filter(null, it)}}", J(R"([])"), {}},
    {"{{filter(5, it)}}", {}, "test.$filter: expected a list, got 5"},
    {"{{filter(state.cards, it.done)}}", J(R"([{"title":"B","points":2,"done":true}])"), {}},
    {"{{filter([0, 1, ''])}}", J(R"([1])"), {}},
    {"{{map(state.cards, it.title)}}", J(R"(["B","a"])"), {}},
    {"{{map(['a', 'b'], index)}}", J(R"([0,1])"), {}},
    {"{{map([[1, 2], [3]], map(it, it * 10))}}", J(R"([[10,20],[30]])"), {}},
    {"{{map([1], card.title + it)}}", J(R"(["x1"])"), {}},
    {"{{count(null, it)}}", 0, {}},
    {"{{count([1, 2, 3])}}", 3, {}},
    {"{{count(state.scores, it > index)}}", 2, {}},
    {"{{sum(null)}}", 0, {}},
    {"{{sum(state.cards, it.points)}}", 7, {}},
    {"{{sum(['a'])}}", {}, "test.$sum: expected a number, got \"a\""},
    {"{{find(state.cards, !it.done)}}", J(R"({"title":"a","points":5,"done":false})"), {}},
    {"{{find([1, 2], it > 5)}}", nullptr, {}},
    {"{{find(null, it)}}", nullptr, {}},
    {"{{some([0, 1], it)}}", true, {}},
    {"{{some(null, it)}}", J(R"([])"), {}},
    {"{{every([1, 2], it)}}", true, {}},
    {"{{every([], it)}}", true, {}},
    {"{{sort(state.cards, it.title)}}",
	 J(R"([{"title":"B","points":2,"done":true},{"title":"a","points":5,"done":false}])"),
	 {}},
    {"{{sort([3, 1, 2])}}", J(R"([1,2,3])"), {}},
    {"{{sort(['b', 'a', 'C'], it, 'desc')}}", J(R"(["b","a","C"])"), {}},
    {"{{sort([{a: 2}, {a: 1}], it)}}", J(R"([{"a":1},{"a":2}])"), {}},
    {"{{filter()}}", {}, "test.$filter: expected [list, expression]"},
    {J(R"({"$filter": 3})"), {}, "test.$filter: expected [list, expression]"},
    {"{{it}}", "outer", {}},
    {"{{map([5], it + index)}}", J(R"([5])"), {}},
    // The clock: 200 days after 1970 began is in 1970 in every time zone
    {"{{time(17280000000, 'YYYY')}}", "1970", {}},
    {"{{time('x', 'YYYY')}}", {}, "test.$time: expected a number, got \"x\""},
    {"{{len(text(now())) >= 13}}", true, {}},
    // Paths and variables: missing values are null; computed values and nodes are read by name
    {"{{state.missing.x}}", nullptr, {}},
    {"{{state.none.x}}", nullptr, {}},
    {"{{state.items[9]}}", nullptr, {}},
    {"{{state.items[-1]}}", nullptr, {}},
    {"{{state.items[1.0]}}", nullptr, {}},
    {"{{state.items['1']}}", nullptr, {}},
    {"{{state.cards[1].title}}", "a", {}},
    {"{{state.object.m.k}}", "v", {}},
    {"{{state.object.length}}", nullptr, {}},
    {"{{state.name.length}}", 15, {}},
    {"{{card.title}}", "x", {}},
    {"{{computed.total}}", 12, {}},
    {"{{computed.list[2].a}}", "b", {}},
    {"{{computed.nope}}", nullptr, {}},
    {"{{computed[0]}}", {}, "test: expected computed.<name>"},
    {"{{nodes.board.width}}", 800, {}},
    {"{{nodes.nope}}", nullptr, {}},
    {"{{nodes}}", {}, "test: expected nodes.<name>"},
    {"{{(state.object ?? 1).m.k}}", "v", {}},
    {"{{[1, 2][1]}}", 2, {}},
    {"{{{a: [3]}.a[0]}}", 3, {}},
    {"{{'abc'.length}}", 3, {}},
    {"{{theme.name}}", "light", {}},
    {"{{event.key}}", "Enter", {}},
    {"{{revision}}", 3, {}},
    {J(R"({"$get": []})"), {}, "test.$get: expected [variable, key, ...]"},
    {J(R"({"$get": [{"$value": {"a": [5]}}, "a", 0]})"), 5, {}},
    {J(R"({"$get": ["state", "items", {"$add": [0, 1]}]})"), "b", {}},
    {J(R"({"$path": "state.items[state.count - 2]"})"), "b", {}},
    {J(R"({"$path": "state..x"})"), {}, "test: invalid path 'state..x'"},
    {J(R"({"$path": "state.items["})"), {}, "test: unclosed '[' in path 'state.items['"},
    {J(R"({"$path": "state.items[true]"})"), {}, "test: index 'true' is neither a number nor a key"},
    {J(R"({"$path": "nope"})"), {}, "test: unknown variable 'nope'"},
    {J(R"({"$value": "{{not evaluated}}"})"), "{{not evaluated}}", {}},
    {J(R"({"$object": {"b": "{{state.count}}", "a": 1}})"), J(R"({"b":3,"a":1})"), {}},
    {J(R"({"$length": "{{state.items}}"})"), 3, {}},
    {J(R"({"$max": 3})"), {}, "test.$max: expected an array of arguments"},
    {J(R"({"a": "{{state.count}}", "b": ["{{1 + 1}}", "x"]})"), J(R"({"a":3,"b":[2,"x"]})"), {}},
    {"{{state.count}} of {{len(state.items)}}", "3 of 3", {}},
    {"  {{state.count}}  ", 3, {}},
    {"{{state.count}}{{", {}, "test: unclosed '{{' in \"{{state.count}}{{\""},
    {"{{ {a: '}}'}.a }}", "}}", {}},
    // Syntax errors name the column
    {"{{1 2}}", {}, "test: in {{1 2}}: unexpected '2' (at column 3)"},
    {"{{(1}}", {}, "test: in {{(1}}: expected ')' (at column 3)"},
    {"{{a.}}", {}, "test: invalid path 'a.'"},
    {"{{f(}}", {}, "test: in {{f(}}: expected a value (at column 3)"},
    {"{{len()}}", {}, "test: in {{len()}}: 'len' takes one argument (at column 6)"},
    {"{{9.}}", {}, "test: in {{9.}}: expected a name (at column 3)"},
};

/// @brief The rows of the table that evaluate without an error.
/// @return How many.
std::size_t succeeding() {
	return static_cast<std::size_t>(std::ranges::count_if(cases, [](const Case& c) { return c.error.empty(); }));
}

} // namespace

TEST_CASE("format: expressions give their expected result or error") {
	CHECK(cases.size() >= 280);
	CHECK(succeeding() >= 180);
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

TEST_CASE("format: every function has a row of the table") {
	std::string table;
	for(const Case& c : cases) table += c.value.dump();
	for(const auto& function : affresco::format::detail::functions()) {
		CAPTURE(function.name);
		const std::string name(function.alias.empty() ? function.name : function.alias);
		const bool written = table.contains(name + "(") || table.contains("\"$" + std::string(function.name) + "\"");
		const bool symbol = function.group == affresco::format::detail::Group::Syntax; // `+`, `&&`, `c ? a : b`
		CHECK((written || symbol));
	}
}

TEST_CASE("format: truthy follows the documented rules") {
	for(const json& value : {json(false), json(nullptr), json(0), json(0.0), json(""), json::array(), json::object()}) {
		CHECK_FALSE(affresco::format::truthy(value));
		CHECK_FALSE(affresco::format::truthy(Value::fromJson(value)));
	}
	for(const json& value : {json(true), json(1), json(-0.5), json("0"), json{0}, json{{"a", 0}}}) {
		CHECK(affresco::format::truthy(value));
		CHECK(affresco::format::truthy(Value::copyJson(value)));
	}
}

// SCREEN_FORMAT.md, "Expressions": in an expression '-' subtracts, so a name with '-' is read in brackets
TEST_CASE("format: a name with '-' is read with brackets; after a dot, '-' subtracts") {
	CHECK(evaluate("{{state['my-list']}}").dump() == "[1,2]");
	CHECK(evaluate("{{state[\"my-list\"][1]}}").dump() == "2");
	CHECK(evaluate("{{len(state['my-list'])}}").dump() == "2");
	CHECK_THROWS_WITH_AS((void)evaluate("{{state.my-list}}"), doctest::Contains("unknown variable 'list'"),
	                     affresco::DocumentError);
}

TEST_CASE("format: the registry lists every function once, with what the catalog and the guide say") {
	std::set<std::string_view> names;
	for(const auto& function : affresco::format::detail::functions()) {
		CAPTURE(function.name);
		CHECK(names.insert(function.name).second);
		CHECK_FALSE(function.about.empty());
		CHECK(((function.apply != nullptr) != (function.decide != nullptr))); // one implementation
		CHECK((function.decide == nullptr || !function.shape.empty()));
		CHECK(affresco::format::detail::findOperator("$" + std::string(function.name)) == &function);
	}
	CHECK(affresco::format::detail::findFunction("len") == affresco::format::detail::findFunction("length"));
	CHECK(affresco::format::detail::findOperator("$len") == nullptr); // an alias is written in expressions only
	CHECK(affresco::format::operatorNames().size() == names.size());
	CHECK(affresco::format::functionCatalog().size() == names.size());
	CHECK(affresco::format::functionCatalog()["clamp"] == "value, low, high: The value, kept between low and high");
}

TEST_CASE("format: SCREEN_FORMAT.md lists the functions of the registry") {
	std::ifstream file(std::string(AFFRESCOUI_SCREENS_DIRECTORY) + "/../../../SCREEN_FORMAT.md");
	REQUIRE(file);
	const std::string guide{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	const std::string expected = affresco::format::functionGuide();
	CAPTURE(expected); // on a failure: the lines to paste into "Values, expressions and state"
	CHECK(guide.contains(expected));
}

TEST_CASE("format: values are small, cheap to copy, and read JSON in place") {
	CHECK(sizeof(Value) <= 48);
	const Value shortText = Value::fromText("sixteen bytes ok"), longText = Value::fromText(std::string(40, 'x'));
	CHECK(shortText.text() == "sixteen bytes ok");
	const Value copy = longText; // NOLINT(performance-unnecessary-copy-initialization): the copy is what is tested
	CHECK(copy.text().data() == longText.text().data()); // it shares the text

	const json state = J(R"({"items": ["a", {"b": 2}], "big": 18446744073709551615})");
	const Value items = Value::fromJson(state).member("items").value_or(Value());
	CHECK(items.item(0).text().data() == state["items"][0].get_ref<const std::string&>().data()); // not copied
	CHECK(items.item(1).member("b").value_or(Value()).integer() == 2);
	CHECK_FALSE(items.item(1).member("c"));
	const Value big = Value::fromJson(state).member("big").value_or(Value());
	CHECK(big.toJson().dump() == "18446744073709551615"); // exact past 64 bits
	CHECK(big.isInteger());

	CHECK(Value::fromObject({{"b", Value::fromInteger(1)}, {"a", Value::fromInteger(2)}, {"b", Value::fromInteger(3)}})
	          .dump() == R"({"b":3,"a":2})");
	CHECK(Value::fromInteger(1) == Value::fromNumber(1.0));
	CHECK_FALSE(Value::fromInteger(1) == Value::fromBoolean(true));
	CHECK(Value::copyJson(J(R"({"a": [1, "x"]})")) == Value::fromJson(J(R"({"a": [1.0, "x"]})")));
	CHECK_FALSE(Value::copyJson(J(R"({"a": 1, "b": 2})")) == Value::copyJson(J(R"({"b": 2, "a": 1})")));
	CHECK(Value::fromNumber(4.0).toText() == "4");
	CHECK(Value::fromNumber(1e15).toText() == "1e+15");
	CHECK(Value().toText().empty());
	CHECK(Value::fromText(std::string_view("\xC3")).toText() == "\xC3"); // a text is given back as it is
	CHECK(Value::fromList({Value::fromText(std::string_view("\xC3"))}).toText() ==
	      "[\"\xEF\xBF\xBD\"]"); // JSON: replaced
}

TEST_CASE("format: an expression still running survives the compile cache being cleared under it") {
	// Reading computed.<name> may compile and evaluate other expressions, which can fill the cache and clear it
	static const json state = {{"count", 3}}, window = json::object(), revision = 0, event = json::object(),
	                  globals = json::object();
	static const affresco::format::Scope locals{};
	const affresco::format::NodeLookup computed = [](std::string_view) {
		affresco::format::clearCompileCache();
		return json(4);
	};
	const affresco::format::Context context{state, window, revision, event, locals, globals, nullptr, &computed};
	CHECK(affresco::format::evaluate("{{state.count + computed.x + state.count}}", context, "test") == 10);
	CHECK(affresco::format::evaluate("a {{computed.x}} b {{state.count * 2}}", context, "test") == "a 4 b 6");
}

TEST_CASE("format: an invalid template names the place it is used at, each time") {
	CHECK_THROWS_WITH_AS((void)evaluate(J(R"({"first": "x {{1 +}}"})")), doctest::Contains("test.first: in {{1 +}}"),
	                     affresco::DocumentError);
	CHECK_THROWS_WITH_AS((void)evaluate(J(R"({"second": "x {{1 +}}"})")), doctest::Contains("test.second: in {{1 +}}"),
	                     affresco::DocumentError);
	// The placeholders before an invalid one are evaluated first, so their errors come first
	CHECK_THROWS_WITH_AS((void)evaluate("{{nope}} {{1 +}}"), doctest::Contains("unknown variable 'nope'"),
	                     affresco::DocumentError);
}
