/// @file
/// @brief Unit tests for the forms of everyday Clojure in a binding (modules/lisp/reference/bindings.md): each is
/// written as an expression, no call to the program in it, and gives what the program's function gives, over states of
/// several shapes. A set's test of membership is here too, with its rows over values of every kind.

#include "LispRows.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <array>
#include <format>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
using lisprows::binding;
using lisprows::disagreements;
using lisprows::Row;
using lisprows::with;

namespace {

/// The states the rows run on, as the language writes them: `x` is the state's map, its paths `x.xs`, `x.filter`...
constexpr std::array<std::string_view, 4> states = {
    R"({:xs [{:tag "a" :ref "r1" :done true :order 2 :title "A"} {:tag "b" :ref "r2" :done false :order 1 :title "B"}]
        :filter "a" :selected {:title "S" :i 1} :query "q" :view "board" :kind "text" :count 0 :tags []})",
    R"({:xs [] :filter "b" :selected nil :query nil :view "list" :kind "line" :count 3 :tags ["a"]})",
    R"({:xs [{:tag "b" :ref "r3" :done true :order 5 :title "C"}] :filter "z" :selected {:title nil :i 0} :query ""
        :view "grid" :kind nil :count -1 :tags nil})",
    R"({:xs [{:tag "a" :done nil :order 1.5 :title ""}] :filter nil :selected {:i 9} :query "x" :view nil :kind "shape"
        :count 1.0 :tags [1]})"};

/// @brief Tells whether a row's binding is written as an expression with no call to the program in it.
/// @param form The row's form, `x` the state.
/// @return Whether it is.
bool writtenAsExpression(const std::string& form) {
	const json text = binding(with(form, "state.v"));
	return text.is_string() && !text.get<std::string>().contains("lisp(");
}

/// @brief Checks rows: each is an expression, and gives what its function gives over every state.
/// @param rows The rows.
void checkRows(const std::vector<std::string>& rows) {
	for(const std::string& row : rows) {
		CAPTURE(row);
		CHECK(writtenAsExpression(row));
	}
	const std::vector<std::string> found = disagreements(rows, states);
	for(std::size_t at = 0; at < rows.size(); ++at) {
		CAPTURE(rows[at]);
		CHECK(found[at].empty());
	}
}

/// @brief The rows of a set's membership test, and the values each disagrees on by its function's truth.
/// @return The rows.
std::vector<Row> setRows() {
	return {{"(contains? #{:a :b} x)", ""},
	        {"(contains? #{\"a\" 1 true} x)", ""},
	        {"(contains? #{nil false} x)", ""},
	        {"(if (#{:a \"\" 2.0} x) 1 0)", ""},
	        {"(if (#{false nil 0} x) 1 0)", ""},
	        {"(#{nil :b} x)", ""},
	        {"(when (#{1 -1} x) \"y\")", ""},
	        {"(#{:a \"b\"} x)", ""},
	        {"(#{\" a \" true} x)", ""},
	        {"(and (#{:a} x) 1)", ""},
	        {"(or (#{:a} x) \"none\")", ""},
	        {"(not (#{:a} x))", ""},
	        {"(cond (#{0 1} x) :small :else :other)", ""},
	        {"(if (some #{1 2} x) 1 0)", R"( "" "a" " a " {})"},
	        {"(filter #{1 2} x)", R"( "" "a" " a " {})"},
	        {"(remove #{1} x)", R"( "" "a" " a " {})"},
	        {"(every? #{1 2} x)", R"( nil "" "a" " a " {})"},
	        {"(if (contains? #{:a} (str x)) 1 0)", ""}};
}

} // namespace

TEST_CASE("Lisp: style bindings: an item's function, written as #( ), is an expression") {
	checkRows({"(filter #(= (:tag %) x.filter) x.xs)", "(map #(= (:tag %) x.filter) x.xs)",
	           "(if (some #(= (:tag %) x.filter) x.xs) 1 0)", "(every? #(= (:tag %) x.filter) x.xs)",
	           "(sort-by #(= (:tag %) x.filter) x.xs)", "(sort-by #(:order %) x.xs)",
	           "(find #(= (:tag %) x.filter) x.xs)"});
	// Its value, where no item matches, is false where the function gives nil (bindings.md: where the rows differ)
	CHECK(writtenAsExpression("(some #(= (:tag %) x.filter) x.xs)"));
	CHECK_FALSE(disagreements({"(some #(= (:tag %) x.filter) x.xs)"}, states)[0].empty());
}

TEST_CASE("Lisp: style bindings: a keyword as the function is an expression") {
	checkRows({"(map :ref x.xs)", "(filter :done x.xs)", "(sort-by :order x.xs)"});
}

TEST_CASE("Lisp: style bindings: the threading macros are expressions") {
	checkRows({"(->> x.xs (filter :done) (map :title) count)", "(-> x.selected :title)", "(-> x :selected :i inc)"});
}

TEST_CASE("Lisp: style bindings: some-> and the forms that bind a value that may be nil are expressions") {
	checkRows({"(some-> x.selected :title)", "(when-some [at x.selected] (get x.xs at.i))",
	           "(if-some [v x.query] v \"none\")"});
}

TEST_CASE("Lisp: style bindings: case and condp are expressions") {
	checkRows({"(case x.view :board 1 :list 2 0)", "(condp = x.view :board 1 :list 2 0)"});
}

TEST_CASE("Lisp: style bindings: the everyday small functions are expressions") {
	checkRows({"(not= x.view :board)", "(if-not (empty? x.xs) 1 0)", "(when-not (zero? x.count) \"some\")",
	           "(dec x.count)", "(inc x.count)", "(empty? x.tags)", "(zero? x.count)"});
}

TEST_CASE("Lisp: style bindings: a keyword is the text of its name") {
	checkRows({"(= x.view :board)", "(if (= :list x.view) 1 0)", "{:view :board :kind x.kind}"});
}

TEST_CASE("Lisp: style bindings: a set's membership test is an expression where its truth or value agrees") {
	const std::vector<Row> rows = setRows();
	std::vector<std::string> forms;
	forms.reserve(rows.size());
	for(const Row& row : rows) forms.push_back(row.form);
	for(const std::string& form : forms) {
		CAPTURE(form);
		CHECK(writtenAsExpression(form));
	}
	const std::vector<std::string> found = disagreements(forms);
	for(std::size_t at = 0; at < rows.size(); ++at) {
		CAPTURE(rows[at].form);
		CHECK(found[at] == rows[at].differs);
	}
	CHECK(disagreements({"(if (#{:a \"b\"} x) 1 0)"}, std::array<std::string_view, 2>{"\"a\"", "\"b\""})[0].empty());
}

TEST_CASE("Lisp: style bindings: a set that isn't all constants, or whose value an expression can't give, is a call") {
	CHECK(binding("(some #{:a} state.xs)").get<std::string>().contains("lisp(")); // gives the member
	CHECK(binding("(contains? #{:a [1]} state.v)").get<std::string>().contains("lisp("));
	CHECK(binding("(contains? #{:a state.w} state.v)").get<std::string>().contains("lisp("));
	CHECK(binding("(map #{:a} state.xs)").get<std::string>().contains("lisp("));
	// The call is given the set as a vector, whose contains? tests an index: pinned, so that a change shows
	CHECK(disagreements({"(contains? #{:a [1]} x)"})[0] == " 0 1 \"a\"");
	// Called for its value, a set of numbers gives its member, which 1.0 is not: refused, as every such call was
	CHECK(lisprows::bindingError("(#{1 2} state.n)").contains("define a function for this"));
	CHECK(lisprows::bindingError("(#{[1]} state.n)").contains("define a function for this"));
}

TEST_CASE("Lisp: style bindings: a set called for its value, nested, is refused once its text is too large") {
	// (#{:a} x) writes x twice, so each level of nesting doubled the text: 40 levels were 2^40 copies of state.v
	std::string form = "state.v";
	for(int level = 0; level < 40; ++level) form.insert(0, "(#{:a} ").push_back(')');
	CHECK(lisprows::bindingError(form).contains("too large as an expression"));
	// A few levels stay an expression
	const std::string inner = "(contains(['b'], state.v) ? state.v : null)";
	CHECK(binding("(#{:a} (#{:b} state.v))") == std::format("{{{{(contains(['a'], {0}) ? {0} : null)}}}}", inner));
}

TEST_CASE("Lisp: style bindings: a node's :when on a set shows and hides it") {
	const std::string program = R"lisp(
(defscreen main :state {:kind "text"}
  (column#root (label#shown :text "x" :when ~(#{:text :shape} state.kind))))
)lisp";
	const json tree = lisprows::built(program);
	const json& when = tree["root"]["children"][0]["when"];
	REQUIRE(when.is_string());
	CHECK_FALSE(when.get<std::string>().contains("lisp("));
	for(const auto& [kind, shown] : std::array<std::pair<std::string_view, bool>, 4>{
	        {{"\"text\"", true}, {"\"shape\"", true}, {"\"line\"", false}, {"nil", false}}}) {
		CAPTURE(kind);
		std::string text = program;
		text.replace(text.find("\"text\"}"), 6, kind);
		lispapp::App app(text);
		REQUIRE(app.errors.empty());
		CHECK((app.window->root().findById("shown") != nullptr && app.window->root().findById("shown")->isVisible()) ==
		      shown);
	}
}
