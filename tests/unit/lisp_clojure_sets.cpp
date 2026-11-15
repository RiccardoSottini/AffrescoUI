/// @file
/// @brief Unit tests for the Lisp language's sets (modules/lisp/reference/sets.md): the literal and its refusals, the
/// core functions on sets, clojure.set's, a set or a vector called as a function, equality, the order a set keeps, what
/// a set leaves the language as, and what it costs.
#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>
#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>

using affresco::json;
namespace lisp = affresco::lisp;

namespace {

/// @brief Evaluates a text and prints the result.
/// @param text The text.
/// @return The printed value.
std::string run(std::string_view text) {
	lisp::Interpreter interpreter;
	return interpreter.print(interpreter.eval(text, "test", "tester"));
}

/// @brief The message of the error a text gives when it is evaluated.
/// @param text The text.
/// @return The message, or "it ran".
std::string errorOf(std::string_view text) {
	try {
		lisp::Interpreter interpreter;
		static_cast<void>(interpreter.eval(text, "test", "tester"));
	} catch(const lisp::LispError& error) {
		return error.message();
	}
	return "it ran";
}

/// @brief What a binding's form builds in a program: `(defscreen main (node :x ~form))`'s `x`.
/// @param form The form.
/// @return The value.
json binding(const std::string& form) {
	return lisp::buildText("(defscreen main (node :x ~" + form + "))", "test.lisp")["root"]["x"];
}

} // namespace

TEST_CASE("lisp sets: a literal reads as a set, each item once, in the order written") {
	CHECK(run("#{1 2 3}") == "#{1 2 3}");
	CHECK(run("#{3 1 2}") == "#{3 1 2}");
	CHECK(run("#{}") == "#{}");
	CHECK(run("#{\"a\" :b [1 2] {:c 1} #{4}}") == "#{\"a\" :b [1 2] {:c 1} #{4}}");
	CHECK(run("(let [x 1 y 2] #{x y 3})") == "#{1 2 3}");
	CHECK(run("(let [x 1] #{x 1})") == "#{1}"); // a form's value twice is dropped when it runs, as hash-set does
	CHECK(run("(pr-str #{1 \"a\"})") == "\"#{1 \\\"a\\\"}\"");
	CHECK(run("(quote #{1 (+ 1 1)})") == "(%set 1 (+ 1 1))");
	CHECK(run("#{#?(:clj 1 :affresco 2) #?@(:default [3 4])}") == "#{2 3 4}");
}

TEST_CASE("lisp sets: the same item twice in a literal is an error at the second") {
	CHECK(errorOf("#{1 2 1}") == "the same number twice in a set: a set holds each item once");
	CHECK(errorOf("#{:a \"a\"}") == "the item \"a\" twice in a set: a set holds each item once");
	CHECK(errorOf("#{x x}") == "the item \"x\" twice in a set: a set holds each item once");
	CHECK(errorOf("#{[1] [1]}") == "the same vector twice in a set: a set holds each item once");
	CHECK(errorOf("#{nil nil}") == "the same nil twice in a set: a set holds each item once");
	CHECK(errorOf("#{1 2").starts_with("expected '}' to close the set opened at"));
	try {
		lisp::Interpreter interpreter;
		static_cast<void>(interpreter.eval("#{1\n 2 1}", "test", "tester"));
		FAIL("it ran");
	} catch(const lisp::LispError& error) {
		CHECK(error.line() == 2);
		CHECK(error.column() == 4);
	}
}

TEST_CASE("lisp sets: membership is by =, where a keyword is the string of its text") {
	CHECK(run("(contains? #{:a :b} \"a\")") == "true");
	CHECK(run("(contains? #{\"a\"} :a)") == "true");
	CHECK(run("(contains? #{1 2} 3)") == "false");
	CHECK(run("(contains? #{1} 1.0)") == "true");
	CHECK(run("(contains? #{[1 {:a 2}]} [1 {\"a\" 2}])") == "true");
	CHECK(run("(contains? nil 1)") == "false");
	CHECK(run("(get #{1 2} 2)") == "2");
	CHECK(run("(get #{1 2} 3)") == "nil");
	CHECK(run("(get #{1 2} 3 :none)") == ":none");
	CHECK(run("(get #{:a} \"a\")") == ":a"); // the member, as it was added
}

TEST_CASE("lisp sets: a set or a vector called as a function") {
	CHECK(run("(#{1 2} 1)") == "1");
	CHECK(run("(#{1 2} 3)") == "nil");
	CHECK(run("(some #{2} [1 2 3])") == "2");
	CHECK(run("(filter #{:a :c} [:a :b :c])") == "[:a :c]");
	CHECK(run("([10 20] 1)") == "20");
	CHECK(run("([10 20] 0)") == "10");
	CHECK(run("(map [:a :b :c] [2 0])") == "[:c :a]");
	CHECK(errorOf("([10 20] 2)") == "a vector as a function: the index 2 is outside it, of 2 items");
	CHECK(errorOf("([10] -1)") == "a vector as a function: the index -1 is outside it, of 1 item");
	CHECK(errorOf("([10 20] :a)") == "a vector as a function takes an index, and was given a keyword");
	CHECK(errorOf("([10 20] 1 2)") == "a vector as a function takes one argument, and was given 2");
	CHECK(errorOf("(#{1} 1 2)") == "a set as a function takes one argument, and was given 2");
	CHECK(errorOf("(#{1})") == "a set as a function takes one argument, and was given 0");
	CHECK(run("(ifn? #{1})") == "true");
	CHECK(run("(ifn? [1])") == "true");
}

TEST_CASE("lisp sets: hash-set, set?, conj, disj and into") {
	CHECK(run("(hash-set 1 2 1)") == "#{1 2}");
	CHECK(run("(hash-set)") == "#{}");
	CHECK(run("(set? #{})") == "true");
	CHECK(run("(set? [1])") == "false");
	CHECK(run("(set? nil)") == "false");
	CHECK(errorOf("(set?)") == "\"set?\" was given 0 arguments");
	CHECK(run("(conj #{1} 1 2)") == "#{1 2}");
	CHECK(run("(conj #{:a} \"a\")") == "#{:a}");
	CHECK(run("(disj #{1 2 3} 2)") == "#{1 3}");
	CHECK(run("(disj #{1 2 3} 4)") == "#{1 2 3}");
	CHECK(run("(disj #{1 2 3} 1 3)") == "#{2}");
	CHECK(run("(disj #{1 2})") == "#{1 2}");
	CHECK(run("(disj nil 1)") == "nil");
	CHECK(run("(disj #{\"a\"} :a)") == "#{}");
	CHECK(errorOf("(disj [1 2] 1)") == "\"disj\" takes a set, and was given a vector");
	CHECK(run("(into #{} [1 1 2])") == "#{1 2}");
	CHECK(run("(count (into #{} [1 1 2]))") == "2");
	CHECK(run("(into #{3} '(1 3 2))") == "#{3 1 2}");
	CHECK(run("(into #{} {:a 1})") == "#{[\"a\" 1]}");
	CHECK(run("(into [] #{2 1})") == "[2 1]");
	CHECK(run("(into {} #{[:a 1]})") == "{:a 1}");
	CHECK(errorOf("(conj 1 2)") == "\"conj\" takes a list, a vector, a set or a map, and was given a number");
}

TEST_CASE("lisp sets: the functions on collections take a set, and give a vector where they give a sequence") {
	CHECK(run("(count #{1 2})") == "2");
	CHECK(run("(empty? #{})") == "true");
	CHECK(run("(empty? #{1})") == "false");
	CHECK(run("(not-empty #{})") == "nil");
	CHECK(run("(seq #{3 1})") == "[3 1]");
	CHECK(run("(seq #{})") == "nil");
	CHECK(run("(first #{5 6})") == "5");
	CHECK(run("(rest #{5 6})") == "[6]");
	CHECK(run("(map inc #{1 2})") == "[2 3]");
	CHECK(run("(reduce + #{1 2 3})") == "6");
	CHECK(run("(vec #{2 1})") == "[2 1]");
	CHECK(run("(sort #{3 1 2})") == "[1 2 3]");
	CHECK(run("(distinct #{1 2})") == "[1 2]");
	CHECK(run("(coll? #{})") == "true");
	CHECK(run("(truthy? #{})") == "false");
	CHECK(run("(str #{1})") == "\"#{1}\"");
}

TEST_CASE("lisp sets: = is the same members in any order, and a set is never a vector") {
	CHECK(run("(= #{1 2} #{2 1})") == "true");
	CHECK(run("(= #{1 2} #{1 2 3})") == "false");
	CHECK(run("(= #{1 2} #{1 3})") == "false");
	CHECK(run("(= #{:a} #{\"a\"})") == "true");
	CHECK(run("(= #{#{1 2}} #{#{2 1}})") == "true");
	CHECK(run("(= #{1} [1])") == "false");
	CHECK(run("(= [1] #{1})") == "false");
	CHECK(run("(= #{} [])") == "false");
	CHECK(run("(= {:a #{1 2}} {:a #{2 1}})") == "true");
	CHECK(run("(contains? #{#{1 2}} #{2 1})") == "true"); // a set's hash doesn't hang on its order
	CHECK(run("(count (distinct [#{1 2} #{2 1}]))") == "1");
}

TEST_CASE("lisp sets: a set keeps the order its members were added in, through conj, disj and union") {
	CHECK(run("(vec (conj #{3 1} 2 1 0))") == "[3 1 2 0]");
	CHECK(run("(vec (disj (conj #{3 1} 2) 1))") == "[3 2]");
	CHECK(run("(vec (conj (disj #{1 2 3} 1) 1))") == "[2 3 1]");
	CHECK(run("(vec (set/union #{3 1} #{2 1} #{0}))") == "[3 1 2 0]");
	CHECK(run("(let [a #{1 2} b (conj a 3) c (conj a 4)] [a b c])") == "[#{1 2} #{1 2 3} #{1 2 4}]");
	CHECK(run("(let [a #{1} b (disj (conj a 2) 2) c (conj a 3)] [a b c])") == "[#{1} #{1} #{1 3}]");
}

TEST_CASE("lisp sets: clojure.set's functions") {
	CHECK(run("(set/union #{1} #{2})") == "#{1 2}");
	CHECK(run("(set/union)") == "#{}");
	CHECK(run("(set/union #{1})") == "#{1}");
	CHECK(run("(set/union nil #{1})") == "#{1}");
	CHECK(run("(clojure.set/union #{1} [2 1])") == "#{1 2}"); // a set back from the app's state is a vector
	CHECK(run("(set/intersection #{1 2 3} #{3 2})") == "#{2 3}");
	CHECK(run("(set/intersection #{1 2} #{3})") == "#{}");
	CHECK(run("(set/intersection #{1 2} #{2} #{2 1})") == "#{2}");
	CHECK(run("(set/intersection #{:a})") == "#{:a}");
	CHECK(errorOf("(set/intersection)") == "\"intersection\" was given 0 arguments");
	CHECK(run("(set/difference #{1 2 3} #{2})") == "#{1 3}");
	CHECK(run("(set/difference #{1 2 3} #{2} #{3})") == "#{1}");
	CHECK(run("(set/difference #{\"a\" \"b\"} #{:a})") == "#{\"b\"}");
	CHECK(run("(set/subset? #{1} #{1 2})") == "true");
	CHECK(run("(set/subset? #{1 3} #{1 2})") == "false");
	CHECK(run("(set/subset? #{} #{})") == "true");
	CHECK(run("(set/superset? #{1 2} #{2})") == "true");
	CHECK(run("(set/superset? #{1} #{1 2})") == "false");
	CHECK(errorOf("(set/subset? #{1})") == "\"subset?\" was given 1 arguments");
	CHECK(run("(set/select odd? #{1 2 3})") == "#{1 3}");
	CHECK(run("(set/select (fn [x] (> x 5)) #{1 2})") == "#{}");
	CHECK(run("(set/rename-keys {:a 1 :b 2} {:a :c})") == "{:b 2 :c 1}");
	CHECK(run("(set/rename-keys {:a 1 :b 2} {:a :b})") == "{:b 1}");
	CHECK(run("(set/rename-keys {:a 1 :b 2} {:a :b :b :a})") == "{:b 1 :a 2}");
	CHECK(run("(set/rename-keys {:a 1} {:z :y})") == "{:a 1}");
	CHECK(run("(set/rename-keys nil {:a :b})") == "{}");
	CHECK(errorOf("(set/rename-keys {:a 1} [1])") ==
	      "\"rename-keys\" takes two maps, and was given a map and a vector");
	CHECK(errorOf("(set/rename-keys {:a 1} {:a 2})").starts_with("a map's key is a keyword or a string, not a number"));
	CHECK(run("(set/map-invert {:a \"x\" :b :y})") == "{:x \"a\" :y \"b\"}");
	CHECK(run("(set/map-invert {:a \"x\" :b \"x\"})") == "{:x \"b\"}");
	CHECK(run("(set/map-invert nil)") == "{}");
	CHECK(errorOf("(set/map-invert {:a 1})").starts_with("a map's key is a keyword or a string, not a number"));
	CHECK(errorOf("(set/map-invert [1])") == "\"map-invert\" takes a map, and was given a vector");
}

TEST_CASE("lisp sets: set is the host's word, and hash-set or into makes one") {
	CHECK(run("(into #{} [1 2])") == "#{1 2}");
	lisp::Interpreter interpreter;
	CHECK_FALSE(interpreter.global("set").has_value());
	CHECK(interpreter.global("hash-set").has_value());
}

TEST_CASE("lisp sets: a set leaves the language as a JSON list in its order, and comes back a vector") {
	CHECK(binding("#{3 1 2}") == json::parse("[3, 1, 2]"));
	CHECK(binding("#{\"a\" :b}") == json::parse(R"(["a", "b"])"));
	CHECK(binding("{:s #{1 #{2}}}") == json::parse(R"({"s": [1, [2]]})"));
	CHECK(binding("(conj #{2} 1)") == json::parse("[2, 1]"));
	const json state = lisp::buildText("(def kinds #{\"a\" \"b\"})\n(defstate shown ~kinds)\n(defscreen main (column))",
	                                   "test.lisp")["state"];
	CHECK(state == json::parse(R"({"shown": ["a", "b"]})"));
	// The same tree twice
	const std::string program = "(defscreen main (node :x ~(conj (disj #{5 4 3 2 1} 3) 0 3)))";
	CHECK(lisp::buildText(program, "a.lisp").dump() == lisp::buildText(program, "a.lisp").dump());
	// And back: JSON has no set, so what comes back is a vector
	lisp::Interpreter interpreter;
	const json tree = interpreter.toTree(interpreter.eval("#{1 2}", "test", "tester"));
	const lisp::Value back = interpreter.viewJson(std::make_shared<const json>(tree));
	CHECK(interpreter.print(back) == "[1 2]");
	interpreter.eval("(defn kind [x] [(set? x) (vector? x)])", "test", "tester");
	interpreter.workOutDefinitions();
	const lisp::Value kind = interpreter.global("kind").value_or(lisp::Value());
	const std::array<lisp::Value, 1> arguments = {back};
	CHECK(interpreter.print(interpreter.call(kind, arguments)) == "[false true]");
}

TEST_CASE("lisp sets: what an event makes of sets goes when it ends") {
	lisp::Interpreter interpreter;
	interpreter.eval(
	    R"lisp((def kept #{1 2}) (defn work [n] (count (set/union kept (into #{} (map str (range n)))))))lisp",
	    "a.lisp", "file");
	interpreter.workOutDefinitions();
	const lisp::Value work = interpreter.global("work").value_or(lisp::Value());
	const std::array<lisp::Value, 1> arguments = {lisp::Value::integer(100)};
	{ // the first call works out the library's entries, which stay
		const lisp::Interpreter::Transient transient(interpreter);
		static_cast<void>(interpreter.call(work, arguments));
	}
	const std::size_t before = interpreter.objects();
	for(int event = 0; event < 20; ++event) {
		const lisp::Interpreter::Transient transient(interpreter);
		interpreter.setBudget({});
		CHECK(interpreter.call(work, arguments).asDouble() == 102.0);
	}
	CHECK(interpreter.objects() == before);
	CHECK(interpreter.print(interpreter.global("kept").value_or(lisp::Value())) == "#{1 2}");
}

TEST_CASE("lisp sets: each item costs, adding is linear, and the walks stop at the depth limit") {
	lisp::Interpreter interpreter;
	interpreter.setBudget({.operations = 100'000'000, .bytes = 1U << 30U});
	const auto start = std::chrono::steady_clock::now();
	CHECK(interpreter.print(interpreter.eval("(count (reduce conj #{} (range 1000000)))", "test", "tester")) ==
	      "1000000");
	CHECK(interpreter.print(interpreter.eval("(contains? (into #{} (range 100000)) 99999)", "test", "tester")) ==
	      "true");
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(20));
	// The allocation budget runs out on a set too large for it
	CHECK(errorOf("(count (reduce conj #{} (range 5000000)))").contains("ran out"));
	// A set nested deeper than a value may be is refused where it is walked
	CHECK(errorOf("(str (reduce (fn [s _] #{s}) 1 (range 2000)))") == "a value nested too deeply to print");
	CHECK(errorOf("(defn deep [] (reduce (fn [s _] #{s}) 1 (range 2000))) (= (deep) (deep))") ==
	      "values nested too deeply to compare");
	CHECK(errorOf("(count (reduce (fn [s _] (hash-set s)) 1 (range 2000)))") == "it ran"); // making one isn't a walk
}

TEST_CASE("lisp sets: items chosen so their hashes meet take no longer than others") {
	// Sets of two numbers whose sum is the same, and numbers a step apart that would share buckets: each was quadratic
	std::string sets = "#{";
	for(int at = 0; at < 16384; ++at)
		sets += "#{" + std::to_string(1048576 + at) + " " + std::to_string(2097151 - at) + "} ";
	sets += "}";
	lisp::Interpreter interpreter;
	interpreter.setBudget({.operations = 100'000'000, .bytes = 1U << 30U});
	const auto start = std::chrono::steady_clock::now();
	CHECK(interpreter.print(interpreter.eval("(count " + sets + ")", "test", "tester")) == "16384");
	CHECK(interpreter.print(interpreter.eval(
	          "(count (distinct (range 4503599627370496 (+ 4503599627370496 (* 100003 100000)) 100003)))", "test",
	          "tester")) == "100000");
	CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
}

TEST_CASE("lisp sets: a set in a backquote, and a macro's set with code in it, are made when they run") {
	CHECK(run("(let [x 1] `#{~x 2})") == "#{1 2}");
	CHECK(run("(let [xs [1 2]] `#{~@xs 3 1})") == "#{1 2 3}");
	CHECK(run("`#{a b}") == "#{a b}");
	CHECK(run("(defmacro m [x] `#{~x 1}) (let [y 5] (m y))") == "#{5 1}");
	CHECK(run("(defmacro v [x] [`#{~x}]) (let [y 7] (v (+ y 1)))") == "[#{8}]");
	CHECK(binding("(let [x \"a\"] `#{~x \"b\"})") == json::parse(R"(["a", "b"])"));
}
