/// @file
/// @brief Unit tests for the language's everyday forms (modules/lisp/reference/forms.md, src/PreludeForms.cpp): each
/// macro's usual case, its edges, its error, its expansion, the guard on the words it writes, the bindings they lower
/// to, and the limits a long form meets.

#include "../../modules/lisp/src/Machine.hpp"
#include "../../modules/lisp/src/Reader.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lisp = affresco::lisp;
using affresco::json;

// NOLINTBEGIN(bugprone-unchecked-optional-access): REQUIRE checks each optional first, and a missed one
// throws, failing the test

namespace {

/// @brief Evaluates a text in a new interpreter and prints the result.
/// @param text The text.
/// @return The printed value.
std::string run(std::string_view text) {
	lisp::Interpreter interpreter;
	return interpreter.print(interpreter.eval(text, "test", "tester"));
}

/// @brief The error a text gives when it is evaluated in a new interpreter.
/// @param text The text.
/// @return The error, or nothing when it ran.
std::optional<lisp::LispError> failureOf(std::string_view text) {
	lisp::Interpreter interpreter;
	try {
		static_cast<void>(interpreter.eval(text, "test", "tester"));
	} catch(const lisp::LispError& error) {
		return error;
	}
	return std::nullopt;
}

/// @brief The message of the error a text gives when it is evaluated in a new interpreter.
/// @param text The text.
/// @return Its message, or "it ran".
std::string errorOf(std::string_view text) {
	const std::optional<lisp::LispError> error = failureOf(text);
	return error ? error->message() : "it ran";
}

/// @brief What a macro's call expands to, once, its gensyms numbered from 1.
/// @param text The call, `(name argument...)`.
/// @return The expansion, printed.
std::string expansionOf(std::string_view text) {
	lisp::Interpreter interpreter;
	lisp::Machine& machine = interpreter.machine();
	lisp::Reader reader(machine.heap, text, "test");
	const std::optional<lisp::ReadForm> read = reader.next();
	REQUIRE(read.has_value());
	const lisp::Value form = read->form;
	std::vector<lisp::Value> arguments;
	for(std::size_t at = 1; at < form.count(); ++at) arguments.push_back(form.at(at));
	machine.gensymCounter() = 0;
	return interpreter.print(machine.expand(machine.global(form.at(0).text()), arguments, form.place()));
}

/// @brief What a binding's form builds: `(defscreen main (node :x ~form))`'s `x`.
/// @param form The form.
/// @return The value.
json binding(const std::string& form) {
	lisp::Program program;
	program.addFile("(defscreen main (node :x ~" + form + "))", "app.lisp");
	return program.build()["root"]["x"];
}

} // namespace

TEST_CASE("lisp forms: -> and ->> thread a value through calls") {
	CHECK(run("(-> 1 inc inc)") == "3");
	CHECK(run("(-> 10 (- 3) (* 2))") == "14");
	CHECK(run("(->> 10 (- 3) (* 2))") == "-14");
	CHECK(run("(->> [1 2 3] (map inc))") == "[2 3 4]");
	CHECK(run("(-> {:a {:b 2}} :a :b)") == "2");
	CHECK(run("(-> 5)") == "5");
	CHECK(run("(->> nil (map inc))") == "[]");
	CHECK(expansionOf("(-> x f (g a))") == "(g (f x) a)");
	CHECK(expansionOf("(->> x f (g a))") == "(g a (f x))");
}

TEST_CASE("lisp forms: as-> names the value at each step") {
	CHECK(run("(as-> 1 n (+ n 1) (* n 10))") == "20");
	CHECK(run("(as-> [1 2] v)") == "[1 2]");
	CHECK(errorOf("(as-> 1 [n] n)") == "as-> takes a value, a name and forms");
	CHECK(expansionOf("(as-> x n (f n) (g n))") == "(let [n x n (f n) n (g n)] n)");
}

TEST_CASE("lisp forms: some-> and some->> stop at nil") {
	CHECK(run("(some-> {:a 1} :a inc)") == "2");
	CHECK(run("(some-> {:a 1} :b inc)") == "nil");
	CHECK(run("(some->> [1 2] (map inc) first)") == "2");
	CHECK(run("(some-> nil inc)") == "nil");
	CHECK(run("(some-> 4)") == "4");
	CHECK(expansionOf("(some-> x f (g a))") ==
	      "(let [g__1 x] (if (nil? g__1) nil (let [g__2 (f g__1)] (if (nil? g__2) nil (g g__2 a)))))");
	CHECK(errorOf("(let [nil? identity] (some-> 1 inc))") ==
	      "\"some->\" uses the language's \"nil?\", which a local hides here: rename the local");
}

TEST_CASE("lisp forms: cond-> and cond->> take each step whose test holds") {
	CHECK(run("(cond-> 1 true inc false (* 10) true (* 2))") == "4");
	CHECK(run("(cond->> [1 2] true (map inc) false (map dec))") == "[2 3]");
	CHECK(run("(cond-> 7)") == "7");
	CHECK(errorOf("(cond-> 1 true)") == "cond-> takes a value and pairs of a test and a form");
	CHECK(errorOf("(cond->> 1 true)") == "cond->> takes a value and pairs of a test and a form");
	CHECK(expansionOf("(cond-> x a (f b))") == "(let [g__1 x g__1 (if a (f g__1 b) g__1)] g__1)");
}

TEST_CASE("lisp forms: if-not and when-not") {
	CHECK(run("(if-not false 1 2)") == "1");
	CHECK(run("(if-not true 1 2)") == "2");
	CHECK(run("(if-not true 1)") == "nil");
	CHECK(run("(when-not nil 1 2)") == "2");
	CHECK(run("(when-not 0 1)") == "nil");
	CHECK(errorOf("(if-not true 1 2 3)") == "if-not takes a test, a form, and maybe another");
	CHECK(expansionOf("(if-not t a b)") == "(if (not t) a b)");
	CHECK(expansionOf("(when-not t a b)") == "(when (not t) a b)");
	CHECK(errorOf("(let [not 1] (when-not x y))") ==
	      "\"when-not\" uses the language's \"not\", which a local hides here: rename the local");
	CHECK(errorOf("(defn not [x] x) (if-not false 1 2)") ==
	      "\"if-not\" uses the language's \"not\", which this program defines: name yours otherwise");
}

TEST_CASE("lisp forms: if-let, when-let, if-some, when-some") {
	CHECK(run("(if-let [a 1] (+ a 1) 0)") == "2");
	CHECK(run("(if-let [a nil] a 0)") == "0");
	CHECK(run("(if-let [a false] a)") == "nil");
	CHECK(run("(if-let [[x y] [1 2]] (+ x y) 0)") == "3");
	CHECK(run("(when-let [{:keys [a]} {:a 5}] a)") == "5");
	CHECK(run("(when-let [a nil] a)") == "nil");
	CHECK(run("(if-some [a false] (str a) :none)") == "\"false\"");
	CHECK(run("(if-some [a nil] a :none)") == ":none");
	CHECK(run("(when-some [a 0] (inc a))") == "1");
	CHECK(errorOf("(if-let [a 1 b 2] a)") == "if-let takes a vector of one name and one value");
	CHECK(errorOf("(if-let a a)") == "if-let takes a vector of one name and one value");
	CHECK(errorOf("(if-let [a 1] a b c)") ==
	      "if-let takes a vector of one name and one value, a form, and maybe another");
	CHECK(errorOf("(when-let [a] a)") == "when-let takes a vector of one name and one value");
	CHECK(errorOf("(when-some (a 1) a)") == "when-some takes a vector of one name and one value");
	CHECK(expansionOf("(if-let [p e] a b)") == "(let [g__1 e] (if g__1 (let [p g__1] a) b))");
	CHECK(expansionOf("(when-some [p e] a b)") == "(let [g__1 e] (when (some? g__1) (let [p g__1] a b)))");
	CHECK(errorOf("(let [some? 1] (if-some [a 1] a))") ==
	      "\"if-some\" uses the language's \"some?\", which a local hides here: rename the local");
}

TEST_CASE("lisp forms: when-first") {
	CHECK(run("(when-first [x [3 4]] (* x 2))") == "6");
	CHECK(run("(when-first [x []] :never)") == "nil");
	CHECK(run("(when-first [x nil] :never)") == "nil");
	CHECK(errorOf("(when-first [x] x)") == "when-first takes a vector of one name and one value");
	CHECK(expansionOf("(when-first [x xs] a)").starts_with("(let [g__1 xs] (when (not ("));
	CHECK(errorOf("(let [not 1] (when-first [x [1]] x))") ==
	      "\"when-first\" uses the language's \"not\", which a local hides here: rename the local");
}

TEST_CASE("lisp forms: case compares a value with constants") {
	CHECK(run("(case 2 1 \"a\" (2 3) \"b\" \"c\")") == "\"b\"");
	CHECK(run("(case 9 1 \"a\" (2 3) \"b\" \"c\")") == "\"c\"");
	CHECK(run("(case :edit :view 1 :edit 2)") == "2");
	CHECK(run("(case 'x x :word \"x\" :text)") == ":word");
	CHECK(run("(case [1 2] [1 2] :pair :other)") == ":pair");
	CHECK(run("(case nil nil :none true :yes)") == ":none");
	CHECK(run("(case (+ 1 1) 2 :two)") == ":two");
	CHECK(errorOf("(case 5 1 :one)") == "no clause of case matches");
	CHECK(errorOf("(case 5)") == "no clause of case matches");
	CHECK(errorOf("(case 1 {:a 1} :map)") == "case takes pairs of a constant and a form, and maybe a default");
	CHECK(expansionOf("(case e 1 a (b \"c\") d z)") == "(cond (= e 1) a (or (= e (quote b)) (= e \"c\")) d :else z)");
	CHECK(expansionOf("(case (f) 1 a)") ==
	      "(let [g__1 (f)] (cond (= g__1 1) a :else (throw \"no clause of case matches\")))");
	CHECK(errorOf("(let [= 1] (case 1 1 :a))") ==
	      "\"case\" uses the language's \"=\", which a local hides here: rename the local");
}

TEST_CASE("lisp forms: condp tests each constant with a function") {
	CHECK(run("(condp = 2 1 :one 2 :two :many)") == ":two");
	CHECK(run("(condp < 5 9 :small 3 :big :none)") == ":big");
	CHECK(run("(condp (fn [a b] (= a (inc b))) 1 3 :x 2 :y)") == ":y");
	CHECK(errorOf("(condp = 7 1 :one)") == "no clause of condp matches");
	// The reader refuses `:>>` as written; a macro can still write the keyword
	CHECK(errorOf("(defmacro m [] (list 'condp '= 1 1 (keyword \">>\") 'inc)) (m)") ==
	      "condp's :>> isn't supported: bind the value with let");
	CHECK(expansionOf("(condp f e 1 a b)") == "(cond (f 1 e) a :else b)");
}

TEST_CASE("lisp forms: for builds a vector") {
	CHECK(run("(for [x [1 2 3] :when (> x 1)] (* x x))") == "[4 9]");
	CHECK(run("(for [x [1 2] y [10 20]] (+ x y))") == "[11 21 12 22]");
	CHECK(run("(for [x [1 2 3] :let [y (* x 10)] :when (> y 10)] y)") == "[20 30]");
	CHECK(run("(for [x [1 2 3 4 1] :while (< x 3)] x)") == "[1 2]");
	CHECK(run("(for [x [] y [1]] [x y])") == "[]");
	CHECK(run("(for [x nil] x)") == "[]");
	CHECK(run("(for [[a b] [[1 2] [3 4]]] (+ a b))") == "[3 7]");
	CHECK(run("(for [x [1 2] y [x 5] :when (not= x y)] [x y])") == "[[1 5] [2 5]]");
	CHECK(errorOf("(for [x] x)") == "for takes a vector of names and collections");
	CHECK(errorOf("(for (x [1]) x)") == "for takes a vector of names and collections");
	CHECK(errorOf("(for [:when true x [1]] x)") == "for takes a vector of names and collections");
	CHECK(errorOf("(for [x [1] :until true] x)") == "for takes a vector of names and collections");
	CHECK(expansionOf("(for [x xs] b)") == "(map (fn [x] b) xs)");
	CHECK(expansionOf("(for [x xs :when p] b)") == "(map (fn [x] b) (filter (fn [x] p) xs))");
	CHECK(expansionOf("(for [x xs y ys] b)") == "(mapcat (fn [x] (map (fn [y] b) ys)) xs)");
	CHECK(expansionOf("(for [x xs :let [y 1] :when p] b)") == "(mapcat (fn [x] (let [y 1] (if p [b] []))) xs)");
	CHECK(errorOf("(let [map 1] (for [x [1]] x))") ==
	      "\"for\" uses the language's \"map\", which a local hides here: rename the local");
}

TEST_CASE("lisp forms: doseq runs its body for each item and gives nil") {
	CHECK(run("(doseq [x [1 2 3]] (* x 2))") == "nil");
	CHECK(run("(doseq [x [] y [1]] (throw \"never\"))") == "nil");
	CHECK(errorOf("(doseq [x [1 2]] (when (= x 2) (throw \"two\")))") == "two");
	CHECK(errorOf("(doseq [x [1 2] y [3] :when (= y 3)] (when (= x 2) (throw \"seen\")))") == "seen");
	CHECK(run("(doseq [x [1 2] :when (> x 5)] (throw \"never\"))") == "nil");
	CHECK(errorOf("(doseq [x 1 2] x)") == "doseq takes a vector of names and collections");
	CHECK(expansionOf("(doseq [x xs] a b)").starts_with("(do ("));
	CHECK(expansionOf("(doseq [x xs] a b)").ends_with(" (fn [g__1 x] (do a b) nil) nil xs) nil)"));
}

TEST_CASE("lisp forms: dotimes") {
	CHECK(run("(dotimes [i 3] i)") == "nil");
	CHECK(run("(dotimes [i 0] (throw \"never\"))") == "nil");
	CHECK(errorOf("(dotimes [i 3] (when (= i 2) (throw \"two\")))") == "two");
	CHECK(errorOf("(dotimes [i] i)") == "dotimes takes [name count]");
	CHECK(errorOf("(dotimes [[i] 3] i)") == "dotimes takes [name count]");
}

TEST_CASE("lisp forms: letfn defines functions that call each other") {
	CHECK(run("(letfn [(twice [n] (* 2 n))] (twice 4))") == "8");
	CHECK(run("(letfn [(fact [n] (if (< n 2) 1 (* n (fact (dec n)))))] (fact 5))") == "120");
	CHECK(run("(letfn [(ev? [n] (if (= n 0) true (od? (dec n)))) (od? [n] (if (= n 0) false (ev? (dec n))))] "
	          "(ev? 10))") == "true");
	CHECK(run("(letfn [(f [x & more] (if (empty? more) x (g more))) (g [xs] (count xs))] [(f 1) (f 1 2 3)])") ==
	      "[1 2]");
	// A parameter named like a function of the group is the parameter
	CHECK(run("(letfn [(f [g] (g 1)) (g [x] :never)] (f inc))") == "2");
	CHECK(run("(letfn [] 3)") == "3");
	CHECK(errorOf("(letfn [f [x] x] 1)") == "letfn takes a vector of (name [params] body...)");
	CHECK(expansionOf("(letfn [(f [a] a)] (f 1))") == "(let [f (fn f [a] a)] (f 1))");
}

TEST_CASE("lisp forms: doto, comment, assert, defn-, defonce") {
	CHECK(run("(doto {:a 1} (get :a) keys)") == "{:a 1}");
	CHECK(run("(doto [] (conj 1))") == "[]");
	CHECK(expansionOf("(doto x (f a) g)") == "(let [g__1 x] (f g__1 a) (g g__1) g__1)");
	CHECK(run("(comment (anything at all) 1 2)") == "nil");
	CHECK(run("(assert (= 1 1))") == "nil");
	CHECK(errorOf("(assert (= 1 2))") == "Assert failed");
	CHECK(errorOf("(assert false \"the count\")") == "Assert failed: the count");
	CHECK(errorOf("(let [not 1] (assert true))") ==
	      "\"assert\" uses the language's \"not\", which a local hides here: rename the local");
	CHECK(run("(defn- twice [x] (* 2 x)) (twice 3)") == "6");
	CHECK(run("(defonce size 4) (+ size 1)") == "5");
}

TEST_CASE("lisp forms: an error in a form names the form's place") {
	const std::optional<lisp::LispError> error = failureOf("(def a 1)\n  (if-let [x] x)");
	REQUIRE(error.has_value());
	CHECK(error->message() == "if-let takes a vector of one name and one value");
	CHECK(error->line() == 2);
	CHECK(error->column() == 3);
}

TEST_CASE("lisp forms: a long form ends at a limit, naming its place") {
	std::string bindings;
	for(int at = 0; at < 200; ++at) bindings += "x" + std::to_string(at) + " [1 2] ";
	const std::optional<lisp::LispError> wide = failureOf("\n(for [" + bindings + "] 1)");
	REQUIRE(wide.has_value());
	CHECK(wide->line() == 2);
	std::string steps;
	for(int at = 0; at < 10000; ++at) steps += " inc";
	const std::optional<lisp::LispError> deep = failureOf("\n(-> 0" + steps + ")");
	REQUIRE(deep.has_value());
	CHECK(deep->line() == 2);
}

TEST_CASE("lisp forms: the bindings they lower to") {
	CHECK(binding("(-> state.items count inc)") == "{{(length(state.items) + 1)}}");
	CHECK(binding("(if-not state.mode \"a\" \"b\")") == "{{(!(state.mode) ? 'a' : 'b')}}");
	CHECK(binding("(case state.mode \"edit\" \"E\" \"view\" \"V\" \"?\")") ==
	      "{{((state.mode == 'edit') ? 'E' : ((state.mode == 'view') ? 'V' : '?'))}}");
	CHECK(binding("(for [c state.cards :when (= c.status \"done\")] c.title)") ==
	      "{{map(filter(state.cards, (it.status == 'done')), it.title)}}");
	CHECK(binding("(->> state.cards (filter (fn [c] c.done)) count)") == "{{length(filter(state.cards, it.done))}}");
}

TEST_CASE("lisp forms: a program's own definition of a form's name is the program's") {
	CHECK(run("(defmacro doseq [b & body] :mine) (doseq [x [1]] x)") == ":mine");
	CHECK(run("(defn comment [c n] [c n]) (comment 1 2)") == "[1 2]");
	CHECK(run("(defmacro each [b & body] `(map (fn [~(first b)] ~@body) ~(second b))) (each [x [1 2]] (inc x))") ==
	      "[2 3]");
}

// NOLINTEND(bugprone-unchecked-optional-access)
