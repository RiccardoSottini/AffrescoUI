/// @file
/// @brief Unit tests for the Clojure forms a binding writes as an expression (modules/lisp/reference/bindings.md):
/// `let` and the macros made of it, the functions written under an expression's names, what stays a function, and
/// each row run both ways, as the program's function and as the expression, over values of every kind.

#include "LispRows.hpp"

#include <affresco/affresco.hpp>
#include <affresco/lisp/Program.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using affresco::json;
namespace lisp = affresco::lisp;

using lisprows::binding;
using lisprows::bindingError;
using lisprows::disagreements;
using lisprows::Row;

namespace {

/// @brief The rows of this package, and two whose truth is an expression's.
/// @return The rows.
std::vector<Row> packageRows() {
	return {{"(zero? x)", ""},
	        {"(pos? x)", ""},
	        {"(neg? x)", ""},
	        {"(even? x)", ""},
	        {"(odd? x)", ""},
	        {"(true? x)", ""},
	        {"(false? x)", ""},
	        {"(not-empty x)", ""},
	        {"(clojure.string/upper-case x)", ""},
	        {"(clojure.string/lower-case x)", ""},
	        {"(clojure.string/trim x)", ""},
	        {R"((clojure.string/split x ","))", ""},
	        {R"((clojure.string/includes? x "a"))", ""},
	        {"(let [n x] (some? n))", ""},
	        {"(let [a x b (nil? a)] (not b))", ""},
	        {"(if-some [m x] m 2)", ""},
	        {"(when-some [m x] m)", ""},
	        {"(some-> x count)", ""},
	        {"(some->> x count)", ""},
	        {"(as-> x n (count n) (inc n))", ""},
	        {"(cond-> x (number? x) inc)", ""},
	        {R"((case x 1 "one" "a" "letter" "other"))", ""},
	        {R"((condp = x 1 "one" "other"))", ""},
	        // An expression's truth (bindings.md): 0, "", [] and {} are false
	        {"(if-let [m x] m 2)", R"( 0 "" [] {})"},
	        {"(when-let [m x] m)", R"( 0 "" [] {})"}};
}

/// @brief The rows before this package. What they disagree on is a finding, kept here so a change is seen.
/// @return The rows.
std::vector<Row> olderRows() {
	return {{"(count x)", ""},
	        {"(range x)", ""},
	        {"(sort x)", R"( "" "a" " a " {})"},
	        {"(concat x [3])", R"( nil "" "a" " a " {})"},
	        {"(merge x {:a 1})", ""},
	        {"(keys x)", ""},
	        {"(vals x)", ""},
	        {"(reverse x)", R"( nil "" "a" " a " {})"},
	        {"(min x 1)", ""},
	        {"(max x 1)", ""},
	        {"(abs x)", ""},
	        {"(upper-case x)", ""},
	        {"(lower-case x)", ""},
	        {"(empty? x)", ""},
	        {"(inc x)", ""},
	        {"(dec x)", ""},
	        {"(nil? x)", ""},
	        {"(some? x)", ""},
	        {"(not x)", R"( 0 "" [] {})"},
	        {"(truthy? x)", ""},
	        {"(- x)", ""},
	        {"(= x 1)", ""},
	        {"(not= x 1)", ""},
	        {"(< x 1)", ""},
	        {"(> x 1)", ""},
	        {"(<= x 1)", ""},
	        {"(>= x 1)", ""},
	        {"(+ x 1)", ""},
	        {"(- x 1)", ""},
	        {"(* x 2)", ""},
	        {"(/ x 2)", ""},
	        {"(mod x 2)", " -1"},
	        {"(and x 1)", R"( 0 "" [] {})"},
	        {"(or x 1)", R"( 0 "" [] {})"},
	        {"(or-else x 1)", ""},
	        {"(get x 0)", ""},
	        {"(first x)", R"( "a" " a ")"},
	        {R"((join "," x))", " nil"},
	        {"(str x)", " [1 2]"},
	        {R"((str "n=" x))", " [1 2]"},
	        {"(filter (fn [i] i) x)", R"( "" "a" " a " {})"},
	        {"(remove (fn [i] i) x)", R"( "" "a" " a " {})"},
	        {"(map (fn [i] i) x)", R"( "" "a" " a " {})"},
	        {"(some (fn [i] i) x)", R"( nil "" "a" " a " [] [1 2] {})"},
	        {"(every? (fn [i] i) x)", R"( nil "" "a" " a " {})"},
	        {"(if x 1 2)", R"( 0 "" [] {})"},
	        {"(when x 1)", R"( 0 "" [] {})"},
	        {"(:a x)", ""}};
}

} // namespace

TEST_CASE(
    "lisp bindings: each row, run as the function and as the expression, agrees where the function gives a value") {
	std::vector<Row> rows = packageRows();
	const std::vector<Row> older = olderRows();
	rows.insert(rows.end(), older.begin(), older.end());
	std::vector<std::string> forms;
	forms.reserve(rows.size());
	for(const Row& row : rows) forms.push_back(row.form);
	const std::vector<std::string> found = disagreements(forms);
	for(std::size_t at = 0; at < rows.size(); ++at) {
		INFO(rows[at].form);
		CHECK(found[at] == rows[at].differs);
	}
}

TEST_CASE("lisp bindings: the functions left out stay calls of the function") {
	// On nil, a string or a map, each gives a value where the expression it would be gives an error or another value:
	// the rows before them (join, map, filter, some, every?) disagree there, above
	CHECK(binding("(clojure.string/join \",\" state.xs)") == "{{lisp('clojure.string/join', ',', state.xs)}}");
	CHECK(binding("(str/join state.xs)") == "{{lisp('clojure.string/join', state.xs)}}");
	CHECK(binding("(not-any? :done state.xs)") == "{{lisp('not-any?', 'done', state.xs)}}");
	CHECK(binding("(not-every? :done state.xs)") == "{{lisp('not-every?', 'done', state.xs)}}");
	CHECK(binding("(mapv :done state.xs)") == "{{lisp('mapv', 'done', state.xs)}}");
	CHECK(binding("(filterv :done state.xs)") == "{{lisp('filterv', 'done', state.xs)}}");
}

TEST_CASE("lisp bindings: a let's pattern is refused") {
	CHECK(bindingError("(let [[a b] state.xs] a)").contains("a let in a binding names plain values"));
}

TEST_CASE("lisp bindings: let writes its names' values where they stand") {
	CHECK(binding("(let [n (count state.xs) k 2] (+ n k n))") == "{{(length(state.xs) + 2 + length(state.xs))}}");
	CHECK(binding("(let [a state.n b (inc a)] (* a b))") == "{{(state.n * (state.n + 1))}}"); // each sees those before
	CHECK(binding("(let [a state.n] (let [a (inc a)] a))") == "{{(state.n + 1)}}");           // the inner one hides
	CHECK(binding("(let [x state.n x (inc x) x (* x 2)] x)") == "{{((state.n + 1) * 2)}}");
	CHECK(binding("(let [u state.user] u.name)") == "{{state.user.name}}");
	CHECK(binding("(let [first state.n when 2] (+ first when))") == "{{(state.n + 2)}}"); // a name hides a function's
	CHECK(binding("(let [d state.n] (filter (fn [c] (> c d)) state.xs))") == "{{filter(state.xs, (it > state.n))}}");
	CHECK(binding("(filter (fn [c] (let [d c.done] d)) state.xs)") == "{{filter(state.xs, it.done)}}");
	CHECK(binding("(let [n 2] (+ state.n n))") == "{{(state.n + 2)}}"); // a value of the build stays one
}

TEST_CASE("lisp bindings: the forms made of let are expressions") {
	CHECK(binding("(if-let [m state.mode] (str/upper-case m) \"none\")") ==
	      "{{(state.mode ? upper(state.mode) : 'none')}}");
	CHECK(binding("(if-let [m state.mode] m \"none\")") == "{{(state.mode ? state.mode : 'none')}}");
	CHECK(binding("(when-let [m state.mode] (str m \"!\"))") == "{{(state.mode ? (state.mode + '!') : null)}}");
	CHECK(binding("(if-some [m state.mode] m \"none\")") == "{{((state.mode != null) ? state.mode : 'none')}}");
	CHECK(binding("(when-some [m state.mode] m)") == "{{((state.mode != null) ? state.mode : null)}}");
	CHECK(binding("(as-> state.n x (inc x) (* x 2))") == "{{((state.n + 1) * 2)}}");
	CHECK(binding("(some-> state.user :name str/upper-case)") ==
	      "{{((state.user == null) ? null : ((state.user.name == null) ? null : upper(state.user.name)))}}");
	CHECK(binding("(some->> state.xs (filter :done) count)") ==
	      "{{((state.xs == null) ? null : ((filter(state.xs, it.done) == null) ? null : "
	      "length(filter(state.xs, it.done))))}}");
	CHECK(binding("(cond-> state.n state.on inc (> state.n 3) dec)") ==
	      "{{((state.n > 3) ? ((state.on ? (state.n + 1) : state.n) - 1) : (state.on ? (state.n + 1) : state.n))}}");
	CHECK(binding("(cond->> state.xs state.on (filter :done))") ==
	      "{{(state.on ? filter(state.xs, it.done) : state.xs)}}");
	CHECK(binding("(case (count state.xs) 0 \"none\" 1 \"one\" \"many\")") ==
	      "{{((length(state.xs) == 0) ? 'none' : ((length(state.xs) == 1) ? 'one' : 'many'))}}");
	CHECK(binding("(condp = state.mode \"edit\" 1 \"view\" 2 0)") ==
	      "{{(('edit' == state.mode) ? 1 : (('view' == state.mode) ? 2 : 0))}}");
	CHECK(binding("(for [x state.xs] (inc x))") == "{{map(state.xs, (it + 1))}}");
	CHECK(binding("(when-first [x state.xs] (inc x))") == "{{(!(empty(state.xs)) ? (state.xs[0] + 1) : null)}}");
	// A form's expansion calls words of the language, which a let's name or an item's would hide
	CHECK(bindingError("(let [first 1] (when-first [x state.xs] x))")
	          .contains("\"when-first\" uses the language's \"first\", which a local hides here: rename the local"));
	CHECK(bindingError("(filter (fn [nil?] (some-> nil? :a)) state.xs)")
	          .contains("\"some->\" uses the language's \"nil?\", which a local hides here"));
}

TEST_CASE("lisp bindings: the functions given an expression's names") {
	CHECK(binding("(str/upper-case state.mode)") == "{{upper(state.mode)}}");
	CHECK(binding("(clojure.string/lower-case state.mode)") == "{{lower(state.mode)}}");
	CHECK(binding("(str/trim state.s)") == "{{trim(state.s)}}");
	CHECK(binding("(str/split state.s \",\")") == "{{split(state.s, ',')}}");
	CHECK(binding("(str/includes? state.s \"a\")") == "{{contains(state.s, 'a')}}");
	CHECK(binding("(zero? (count state.xs))") == "{{(length(state.xs) == 0)}}");
	CHECK(binding("(pos? state.n)") == "{{(state.n > 0)}}");
	CHECK(binding("(neg? state.n)") == "{{(state.n < 0)}}");
	CHECK(binding("(even? state.n)") == "{{((state.n % 2) == 0)}}");
	CHECK(binding("(odd? state.n)") == "{{((state.n % 2) != 0)}}");
	CHECK(binding("(true? state.on)") == "{{(state.on == true)}}");
	CHECK(binding("(false? state.on)") == "{{(state.on == false)}}");
	CHECK(binding("(not-empty state.xs)") == "{{(empty(state.xs) ? null : state.xs)}}");
}

TEST_CASE("lisp bindings: what an expression can't be is refused, with what to do instead") {
	const std::string define = "a value read while the app runs is an expression or a call of a function: define a "
	                           "function for this";
	CHECK(bindingError("(let [a state.n] a a)").contains(define)); // a binding is one value
	CHECK(bindingError("(doseq [x state.xs] x)").contains(define));
	CHECK(bindingError("(dotimes [i state.n] i)").contains(define));
	CHECK(bindingError("(letfn [(f [x] x)] (f state.n))").contains(define));
	CHECK(bindingError("(doto state.n inc)").contains(define));
	CHECK(bindingError("(for [x state.xs :while x] x)").contains(define));
	CHECK(bindingError("((fn ([a] a) ([a b] b)) state.n)").contains(define));
	const std::string mapcat = "a binding can't give a function to \"mapcat\": define a function for this";
	CHECK(bindingError("(for [x state.xs y x] y)").contains(mapcat));
	CHECK(bindingError("(for [x state.xs :let [y x]] y)").contains(mapcat));
	CHECK(bindingError("(let [{:keys [a]} state.m] a)")
	          .contains("a let in a binding names plain values: define a "
	                    "function for a pattern"));
	CHECK(bindingError("(loop [i 0] (if (< i state.n) (recur (inc i)) i))").contains(define));
	CHECK(bindingError("(throw state.n)").contains("a binding can't throw"));
	CHECK(bindingError("(case (count state.xs) 0 \"none\" 1 \"one\")")
	          .contains("a binding can't throw: give a value for when nothing matches, or define a function for this"));
	CHECK(bindingError("(map (fn [c] (let [d c.n] (filter (fn [e] (> e d)) c.xs))) state.xs)")
	          .contains("a function in a binding can't use \"d\", whose value reads the item of a function around it: "
	                    "define a function for this"));
	// What builds at once still does: a let reading nothing of the app is a value
	CHECK(binding("(let [a 2] (* a 3))") == 6);
}

TEST_CASE("lisp bindings: a binding whose text grows past the cap is refused") {
	std::string steps;
	for(int step = 0; step < 40; ++step) steps += " inc";
	CHECK(bindingError("(some-> state.n" + steps + ")")
	          .contains("this binding is too large as an expression: define a function for it"));
	CHECK(binding("(some-> state.n inc inc)").get<std::string>().starts_with("{{((state.n == null) ? null :"));
	// A long text written once is no let's: it stays as it was
	std::string sum = "(+";
	for(int term = 0; term < 600; ++term) sum += " state.n";
	CHECK(binding(sum + ")").get<std::string>().size() > 4000);
}

TEST_CASE("lisp bindings: a let's binding updates with what it reads") {
	lispapp::App app(R"lisp(
(defscreen main :state {:mode nil}
  :actions {:edit [{:set "state.mode" :to "edit"}]}
  (column#root (label#mode :text ~(if-let [m state.mode] (str/upper-case m) "none"))))
)lisp");
	REQUIRE(app.errors.empty());
	CHECK(app.repository.document()["root"]["children"][0]["text"] == "{{(state.mode ? upper(state.mode) : 'none')}}");
	const auto* label = dynamic_cast<affresco::Label*>(app.window->root().findById("mode"));
	REQUIRE(label);
	CHECK(label->text() == "none");
	app.runtime->runAction("edit", app.window->root(), json::object());
	app.settle();
	CHECK(label->text() == "EDIT");
}

TEST_CASE("lisp bindings: a function's if-let tests the path its name holds, and it is noted") {
	lisp::Program program;
	program.addFile(R"lisp(
(defn f "Tests a path through a name." [] (if-let [m state.mode] m "none"))
(defn g "And through a let." [] (let [p computed.picked] (when p 1)))
(defscreen main (panel#root :text ~(if-let [m state.mode] m "none")))
)lisp",
	                "app.lisp");
	static_cast<void>(program.build());
	std::vector<std::string> paths;
	for(const lisp::UndefinedName& test : program.truthTests()) paths.push_back(test.name);
	CHECK(paths == std::vector<std::string>{"state.mode", "computed.picked"}); // a binding isn't one
}
