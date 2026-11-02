/// @file
/// @brief Unit tests for the Lisp language (modules/lisp): evaluation, closures, macros, tail calls at great depth,
/// limits, purity, programs, what a host sees, and that every example's Lisp file builds to the notation reader's exact
/// tree.

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>
#include <affresco/notation/Notation.hpp>
#include <affresco/notation/Words.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <vector>

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

/// @brief The error a text gives when it is evaluated.
/// @param text The text.
/// @param limits The limits.
/// @return The error, or an empty one if it ran.
lisp::LispError errorOf(std::string_view text, const lisp::Limits& limits = {}) {
	try {
		lisp::Interpreter interpreter(limits);
		static_cast<void>(interpreter.eval(text, "test", "tester"));
	} catch(const lisp::LispError& error) {
		return error;
	}
	return {"", 0, 0, "", "it ran"};
}

/// @brief The error a program's build gives.
/// @param text The text.
/// @param limits The limits.
/// @return The error, or an empty one if it built.
lisp::LispError buildError(std::string_view text, const lisp::Limits& limits = {}) {
	try {
		static_cast<void>(lisp::buildText(text, "test.lisp", limits));
	} catch(const lisp::LispError& error) {
		return error;
	}
	return {"", 0, 0, "", "it built"};
}

/// @brief Reads a file's text.
/// @param file The file.
/// @return Its bytes.
std::string textOf(const std::filesystem::path& file) {
	const std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

} // namespace

TEST_CASE("lisp: values read as JSON's, and print back") {
	CHECK(run("nil") == "nil");
	CHECK(run("[1 -2.5 true false nil \"a\\n\"]") == "[1 -2.5 true false nil \"a\\n\"]");
	CHECK(run("{:a 1 \"b c\" 2}") == "{:a 1 \"b c\" 2}");
	CHECK(run("18446744073709551615") == "18446744073709551615");
	CHECK(run("'(a :b \"c\")") == "(a :b \"c\")");
	CHECK(run("[1, 2 #_ 3 4]") == "[1 2 4]");
}

TEST_CASE("lisp: arithmetic and comparisons") {
	CHECK(run("(+ 1 2 3)") == "6");
	CHECK(run("(+)") == "0");
	CHECK(run("(- 5)") == "-5");
	CHECK(run("(- 5 2 1)") == "2");
	CHECK(run("(* 2 3.5)") == "7.0");
	CHECK(run("(/ 6 3)") == "2");
	CHECK(run("(/ 1 2)") == "0.5");
	CHECK(run("(mod -1 3)") == "2");
	CHECK(run("(rem -1 3)") == "-1");
	CHECK(run("(mod 5.5 2)") == "1.5");
	CHECK(run("(< 1 2 3)") == "true");
	CHECK(run("(< 1 3 2)") == "false");
	CHECK(run("(>= 3 3 2)") == "true");
	CHECK(run("(<= 1 1 0)") == "false");
	CHECK(run("(= 1 1.0)") == "true");
	CHECK(run("(= [1 {:a 2}] '(1 {\"a\" 2}))") == "true");
	CHECK(run("(= {:a 1 :b 2} {:b 2 :a 1})") == "true");
	CHECK(run("(not= 1 2)") == "true");
	CHECK(run("(min 3 1 2)") == "1");
	CHECK(run("(max 3 1 2)") == "3");
	CHECK(run("(abs -3)") == "3");
	CHECK(run("(* 3037000500 3037000499)") == "9223372033963249500"); // still an integer
	CHECK(run("(* -9223372036854775807 -1)") == "9223372036854775807");
	CHECK(run("(+ 9223372036854775807 1)") == "9.223372036854776e+18"); // past an integer: a double
	CHECK(run("(- -9223372036854775807 2)") == "-9.223372036854776e+18");
	CHECK(run("(* 9223372036854775807 2)") == "1.8446744073709552e+19");
	CHECK(run("(rem -9223372036854775807 -1)") == "0");
	CHECK(run("(/ -9223372036854775808 -1)") == "9.223372036854776e+18");
	CHECK(errorOf("(/ 1 0)").message() == "dividing by zero");
	CHECK(errorOf("(mod 1 0)").message() == "dividing by zero");
	CHECK(errorOf("(+ 1 \"secret\")").message() == "\"+\" takes numbers, and was given a string"); // never the value
}

TEST_CASE("lisp: truth, and and or give an operand") {
	CHECK(run("(if nil 1 2)") == "2");
	CHECK(run("(if false 1 2)") == "2");
	CHECK(run("(if 0 1 2)") == "1");
	CHECK(run("(if \"\" 1 2)") == "1");
	CHECK(run("(if false 1)") == "nil");
	CHECK(run("(and 1 2)") == "2");
	CHECK(run("(and nil 2)") == "nil");
	CHECK(run("(and)") == "true");
	CHECK(run("(or nil false 3)") == "3");
	CHECK(run("(or nil false)") == "false");
	CHECK(run("(or)") == "nil");
	CHECK(run("(not nil)") == "true");
	CHECK(run("(when true 1 2)") == "2");
	CHECK(run("(when false 1 2)") == "nil");
	CHECK(run("(cond false 1 nil 2 :else 3)") == "3");
	CHECK(run("(cond false 1)") == "nil");
	CHECK(run("(do 1 2)") == "2");
	CHECK(run("(do)") == "nil");
}

TEST_CASE("lisp: let, functions and lexical closures") {
	CHECK(run("(let [a 1 b (+ a 1)] (* a b))") == "2");
	CHECK(run("(let [a 1] (let [a 2] a))") == "2");
	CHECK(run("((fn [a b] (+ a b)) 1 2)") == "3");
	CHECK(run("((fn [a & more] more) 1 2 3)") == "[2 3]");
	CHECK(run("((fn [& more] more))") == "[]");
	CHECK(run("(let [add (fn [a] (fn [b] (+ a b)))] ((add 2) 3))") == "5");
	CHECK(run("(let [a 1 f (fn [] a) a 2] [a (f)])") == "[2 1]"); // lexical, not dynamic
	CHECK(run("((fn fact [n] (if (< n 2) 1 (* n (fact (- n 1))))) 5)") == "120");
	CHECK(run("(defn f [] 1) (defn g [] (f)) (defn f [] 2) (g)") == "2"); // a global is looked up at each call
	CHECK(run("(def x 2) (def y (* x 2)) y") == "4");
	CHECK(run("(defn outer [a] (fn [] (fn [] a))) (((outer 7)))") == "7"); // through two levels
	CHECK(run("(let [counters (map (fn [i] (fn [] i)) [1 2])] [((first counters)) ((second counters))])") == "[1 2]");
	CHECK(errorOf("((fn [a] a))").message() == "this function takes 1 arguments, and was given 0");
	CHECK(errorOf("(defn f [a] a) (f 1 2)").message() == "\"f\" takes 1 arguments, and was given 2");
	CHECK(errorOf("(1 2)").message() == "this isn't a function: it is a number");
	CHECK(errorOf("nope").message() == "\"nope\" is not defined");
	CHECK(errorOf("(let [a])").message() == "expected a vector of names and values, in pairs");
	CHECK(errorOf("(fn x)").message() == "expected the function's parameters, a vector");
	CHECK(errorOf("(defn if [] 1)").message() == "\"if\" is a form of the language, and can't be redefined");
	CHECK(errorOf("(do (quote))").message() == "quote takes one form");
	CHECK(errorOf("(if)").message() == "if takes a test, a form and maybe another form");
}

TEST_CASE("lisp: destructuring of vectors and maps") {
	CHECK(run("(let [[a b] [1 2]] (+ a b))") == "3");
	CHECK(run("(let [[a b] [1]] b)") == "nil");
	CHECK(run("(let [[a & more :as all] [1 2 3]] [a more all])") == "[1 [2 3] [1 2 3]]");
	CHECK(run("(let [[[a] {:keys [b]}] [[1] {:b 2}]] [a b])") == "[1 2]");
	CHECK(run("(let [{:keys [a b] :or {b 5} :as m} {:a 1}] [a b m])") == "[1 5 {:a 1}]");
	CHECK(run("(let [{x :a [y] :b} {:a 1 :b [2]}] [x y])") == "[1 2]");
	CHECK(run("((fn [[a b] {:keys [c]}] [a b c]) [1 2] {:c 3})") == "[1 2 3]");
	CHECK(run("(loop [[a & more] [1 2 3] total 0] (if a (recur more (+ total a)) total))") == "6");
	CHECK(errorOf("(let [1 2] 3)").message().starts_with("expected a name, a vector or a map to bind"));
}

TEST_CASE("lisp: collections") {
	CHECK(run("(first [1 2])") == "1");
	CHECK(run("(first nil)") == "nil");
	CHECK(run("(rest [1 2 3])") == "[2 3]");
	CHECK(run("(rest '(1 2))") == "(2)");
	CHECK(run("(rest nil)") == "[]");
	CHECK(run("(second [1 2])") == "2");
	CHECK(run("(last [1 2])") == "2");
	CHECK(run("(cons 1 [2])") == "(1 2)");
	CHECK(run("(conj [1] 2 3)") == "[1 2 3]");
	CHECK(run("(conj '(1) 2)") == "(2 1)");
	CHECK(run("(conj nil 1)") == "[1]");
	CHECK(run("(let [a [1] b (conj a 2) c (conj a 3)] [a b c])") == "[[1] [1 2] [1 3]]"); // sharing never shows
	CHECK(run("(let [a (conj [] 1) b (conj a 2) c (rest b) d (conj c 9)] [a b c d])") == "[[1] [1 2] [2] [2 9]]");
	CHECK(run("(assoc {:a 1} :b 2 :a 3)") == "{:a 3 :b 2}");
	CHECK(run("(assoc nil :a 1)") == "{:a 1}");
	CHECK(run("(assoc [1 2] 0 9)") == "[9 2]");
	CHECK(run("(dissoc {:a 1 :b 2} :a)") == "{:b 2}");
	CHECK(run("(get {:a 1} :a)") == "1");
	CHECK(run("(get {:a 1} \"a\")") == "1");
	CHECK(run("(get {:a 1} :b 2)") == "2");
	CHECK(run("(get [5] 0)") == "5");
	CHECK(run("(get nil :a)") == "nil");
	CHECK(run("(:a {:a 1})") == "1");
	CHECK(run("(:b {:a 1} 2)") == "2");
	CHECK(run("({:a 1} :a)") == "1");
	CHECK(run("(get-in {:a {:b [1 2]}} [:a :b 1])") == "2");
	CHECK(run("(get-in {:a 1} [:b :c] 5)") == "5");
	CHECK(run("(assoc-in {:a {:b 1}} [:a :c] 2)") == "{:a {:b 1 :c 2}}");
	CHECK(run("(update {:a 1} :a + 2)") == "{:a 3}");
	CHECK(run("(update-in {:a {:b 1}} [:a :b] inc)") == "{:a {:b 2}}");
	CHECK(run("(merge {:a 1} nil {:a 2 :b 3})") == "{:a 2 :b 3}");
	CHECK(run("(keys {:a 1})") == "[\"a\"]");
	CHECK(run("(vals {:a 1})") == "[1]");
	CHECK(run("(count [1 2])") == "2");
	CHECK(run("(count {:a 1})") == "1");
	CHECK(run("(count \"\\u00e9a\")") == "2");
	CHECK(run("(count nil)") == "0");
	CHECK(run("(empty? [])") == "true");
	CHECK(run("(contains? {:a nil} :a)") == "true");
	CHECK(run("(range 3)") == "[0 1 2]");
	CHECK(run("(range 1 3)") == "[1 2]");
	CHECK(run("(range 5 0 -2)") == "[5 3 1]");
	CHECK(run("(nth [1 2] 1)") == "2");
	CHECK(run("(nth [1] 5 :x)") == ":x");
	CHECK(run("(concat [1] '(2) nil)") == "[1 2]");
	CHECK(run("(reverse [1 2])") == "[2 1]");
	CHECK(run("(sort [3 1 2])") == "[1 2 3]");
	CHECK(run("(sort [\"b\" \"a\"])") == "[\"a\" \"b\"]");
	CHECK(run("(sort-by :a [{:a 2} {:a 1}])") == "[{:a 1} {:a 2}]");
	CHECK(run("(vec '(1 2))") == "[1 2]");
	CHECK(run("(take 2 [1 2 3])") == "[1 2]");
	CHECK(run("(drop 2 [1 2 3])") == "[3]");
	CHECK(run("(zipmap [:a :b] [1 2])") == "{:a 1 :b 2}");
	CHECK(run("(list 1 2)") == "(1 2)");
	CHECK(errorOf("(nth [1] 5)").message() == "\"nth\": an index outside the collection");
	CHECK(errorOf("(get 5 :a)").message() == "\"get\" takes a map or a vector, and was given a number");
	CHECK(errorOf("(:a 5)").message() == "a keyword as a function takes a map, and was given a number");
	CHECK(errorOf("(sort [1 \"a\"])").message().starts_with("\"sort\" compares"));
	CHECK(errorOf("(range 1 2 0)").message().contains("never ends"));
}

TEST_CASE("lisp: functions that take functions") {
	CHECK(run("(map inc [1 2])") == "[2 3]");
	CHECK(run("(map first {:a 1})") == "[\"a\"]");
	CHECK(run("(map upper-case \"ab\")") == "[\"A\" \"B\"]");
	CHECK(run("(filter (fn [x] (> x 1)) [1 2 3])") == "[2 3]");
	CHECK(run("(remove (fn [x] (> x 1)) [1 2 3])") == "[1]");
	CHECK(run("(reduce + [1 2 3])") == "6");
	CHECK(run("(reduce + 10 [1 2 3])") == "16");
	CHECK(run("(reduce + [])") == "nil");
	CHECK(run("(find (fn [x] (> x 1)) [1 2 3])") == "2");
	CHECK(run("(find (fn [x] (> x 9)) [1])") == "nil");
	CHECK(run("(some (fn [x] (and (> x 1) :yes)) [1 2])") == ":yes");
	CHECK(run("(every? number? [1 2])") == "true");
	CHECK(run("(mapcat (fn [x] [x x]) [1 2])") == "[1 1 2 2]");
	CHECK(run("(map-indexed (fn [i x] [i x]) [:a])") == "[[0 :a]]");
	CHECK(run("(apply + 1 2 [3 4])") == "10");
	CHECK(run("(apply + [])") == "0");
	CHECK(run("(apply (fn [& a] a) 1 nil)") == "[1]");
	CHECK(run("(map :a [{:a 1}])") == "[1]");
	// A long list is linear: 100000 conj would be 5e9 copies otherwise
	CHECK(run("(count (reduce conj [] (range 100000)))") == "100000");
	// A program's redefinition doesn't change the language's own functions
	CHECK(run("(defn count [x] 0) (defn nth [a b] 0) (reduce + [1 2 3])") == "6");
	CHECK(run("(defn get [a b] 0) (let [{:keys [a]} {:a 1}] a)") == "1");
}

TEST_CASE("lisp: text") {
	CHECK(run("(str 1 \"a\" :b 'c nil [\"d\" nil])") == "\"1a:bc[\\\"d\\\" nil]\"");
	CHECK(run("(str)") == "\"\"");
	CHECK(run("(join \", \" [1 \"a\"])") == "\"1, a\"");
	CHECK(run("(subs \"\\u00e9ab\" 1)") == "\"ab\"");
	CHECK(run("(subs \"\\u00e9ab\" 0 1)") == "\"\u00e9\"");
	CHECK(run("(upper-case \"a\\u00e9\")") == "\"A\u00e9\"");
	CHECK(run("(lower-case \"A\")") == "\"a\"");
	CHECK(run("(name :a)") == "\"a\"");
	CHECK(run("(keyword \"a\")") == ":a");
	CHECK(run("(symbol \"a\")") == "a");
	CHECK(errorOf("(subs \"a\" 5)").message() == "\"subs\": indexes outside the string");
}

TEST_CASE("lisp: macros and templates") {
	CHECK(run("(defmacro unless [test & body] `(if ~test nil (do ~@body))) (unless false 1 2)") == "2");
	CHECK(run("(defmacro twice [form] `(do ~form ~form)) (twice 1)") == "1");
	CHECK(run("(let [a 1 b [2 3]] `(x ~a ~@b y))") == "(x 1 2 3 y)");
	CHECK(run("(let [a 1 b [2]] `[~a ~@b {:c ~a}])") == "[1 2 {:c 1}]");
	CHECK(run("(let [k \"a\"] `{~k 1})") == "{:a 1}");
	CHECK(run("`(a `(b ~(c ~(+ 1 2))))") == "(a (backquote (b (unquote (c 3)))))");
	CHECK(run("(defmacro m [x] (list 'quote x)) (m (a b))") == "(a b)");
	CHECK(run("(defmacro with-g [& body] (let [g (gensym)] `(let [~g 1] ~@body ~g))) (with-g 2)") == "1");
	CHECK(errorOf("(defmacro m [] `(m)) (m)").exhausted());
	CHECK(errorOf("(defmacro m [] 1) m").message() == "\"m\" is a macro, not a value");
	CHECK(errorOf("~a").message().starts_with("~ goes inside a template"));
	CHECK(errorOf("`~@[1]").message() == "~@ goes inside a list or a vector");
	CHECK(errorOf("`(~@1)").message() == "~@ spreads a list or a vector, not a number");
	// A macro's own budget: an expansion that runs forever ends, and names its place
	lisp::Limits limits;
	limits.macroOperations = 1000;
	const lisp::LispError spent = errorOf("(defmacro m [] (loop [] (recur)))\n(m)", limits);
	CHECK(spent.exhausted());
	CHECK(spent.message() == "the budget for macro expansion ran out");
}

TEST_CASE("lisp: conditions") {
	CHECK(run("(try (throw 1) (catch e [e]))") == "[1]");
	CHECK(run("(try 2 (catch e 3))") == "2");
	CHECK(run("(try (first 5) (catch e (:message e)))") ==
	      "\"\\\"first\\\" takes something with items, and was given a number\"");
	CHECK(run("(try (try (throw 1) (catch e (throw (+ e 1)))) (catch e e))") == "2");
	CHECK(run("(defn f [] (throw :x)) (try (f) (catch e e))") == ":x");
	CHECK(run("(defn f [] (try (throw 1) (catch e (g)))) (defn g [] 7) (f)") == "7");
	CHECK(run("(try (map (fn [x] (throw x)) [4]) (catch e e))") == "4"); // through the language's own functions
	CHECK(run("(let [a 1] (try (let [a 2] (throw a)) (catch e [a e])))") == "[1 2]");
	CHECK(errorOf("(throw \"said here\")").message() == "said here");
	CHECK(errorOf("(throw (str \"secret\"))").message() == "a condition was thrown and nothing caught it");
	CHECK(errorOf("(try 1)").message() == "try ends with (catch name form ...)");
	CHECK(errorOf("(catch e 1)").message() == "catch goes at the end of a try");
	CHECK(errorOf("(loop [] (try (recur) (catch e 1)))").message().starts_with("recur goes in tail position"));
	CHECK(errorOf("(+ (recur) 1)").message().starts_with("recur goes in tail position"));
}

TEST_CASE("lisp: tail calls at great depth, and recursion that ends with an error") {
	lisp::Limits limits;
	limits.budget.operations = 1000000000;
	limits.budget.bytes = std::size_t{1} << 30U;
	lisp::Interpreter interpreter(limits);
	CHECK(interpreter.print(interpreter.eval("(defn f [n] (if (= n 0) :done (f (- n 1)))) (f 2000000)")) == ":done");
	CHECK(interpreter.print(interpreter.eval("(defn even [n] (if (= n 0) true (odd (- n 1)))) (defn odd [n] (if (= n "
	                                         "0) false (even (- n 1)))) (even 1000001)")) == "false");
	CHECK(interpreter.print(interpreter.eval("(loop [n 2000000] (if (= n 0) :done (recur (- n 1))))")) == ":done");
	CHECK(interpreter.print(interpreter.eval("(defn r [n] (if (= n 0) :done (recur (- n 1)))) (r 2000000)")) ==
	      ":done");
	CHECK(interpreter.print(interpreter.eval("(defn r [n] (if (= n 0) :done (apply r [(- n 1)]))) (r 1000000)")) ==
	      ":done");
	CHECK(interpreter.print(interpreter.eval("(defn d [n] (if (= n 0) 0 (+ 1 (d (- n 1))))) (d 15000)")) == "15000");

	const lisp::LispError deep = errorOf("(defn d [n]\n  (+ 1 (d (+ n 1))))\n(d 0)");
	CHECK(deep.exhausted());
	CHECK(deep.message() == "calls nested more than 20000 deep");
	CHECK(deep.line() == 2);
	CHECK(deep.definition() == "d");
	// Through the functions that take functions, the same
	CHECK(errorOf("(defn walk [n] (map walk [n])) (walk 0)").exhausted());
	CHECK(errorOf("(defn walk [n] (reduce (fn [a b] (walk b)) 0 [n n])) (walk 0)").exhausted());
	// No try catches a limit
	CHECK(errorOf("(defn d [n] (+ 1 (d n))) (try (d 0) (catch e 1))").exhausted());
}

TEST_CASE("lisp: limits end what would never end, naming the place") {
	lisp::Limits low;
	low.budget.operations = 100000;
	low.budget.bytes = 1U << 20U;
	const lisp::LispError endless = errorOf("\n(defn f []\n  (loop [] (recur)))\n(f)", low);
	CHECK(endless.exhausted());
	CHECK(endless.message() == "the budget for work ran out");
	CHECK(endless.line() == 3);
	CHECK(endless.definition() == "f");
	CHECK(errorOf("(try (loop [] (recur)) (catch e 1))", low).exhausted());
	CHECK(errorOf("(loop [a []] (recur (conj a 1)))", low).exhausted());
	CHECK(errorOf("(loop [s \"xxxxxxxx\"] (recur (str s s)))", low).exhausted());
	CHECK(errorOf("(range 1000000000000)", low).exhausted());
	CHECK(errorOf("(apply + (range 100000000))", low).exhausted());
	// A value that shares itself is cheap to make, and refused when walked
	const std::string doubling = "(loop [a [1] n 60] (if (= n 0) a (recur [a a] (- n 1))))";
	lisp::Interpreter interpreter(low);
	const lisp::Value shared = interpreter.eval(doubling);
	CHECK_THROWS_AS(static_cast<void>(interpreter.print(shared)), lisp::LispError);
	lisp::Interpreter other(low);
	const lisp::Value alsoShared = other.eval(doubling);
	CHECK_THROWS_AS(static_cast<void>(other.toTree(alsoShared)), lisp::LispError);
	CHECK(errorOf("(let [a " + doubling + "] (= a a))", low).exhausted());
	// A value too deep to walk: an error, and freeing it is flat
	lisp::Limits roomy;
	roomy.budget.operations = 100000000;
	roomy.budget.bytes = 1U << 30U;
	lisp::Interpreter deep(roomy);
	const lisp::Value nested = deep.eval("(loop [a [] n 1000000] (if (= n 0) a (recur [a] (- n 1))))");
	CHECK_THROWS_AS(static_cast<void>(deep.print(nested)), lisp::LispError);
	CHECK_THROWS_AS(static_cast<void>(deep.toTree(nested)), lisp::LispError);
	CHECK_THROWS_AS(static_cast<void>(deep.eval("(defn q [] nil)(= (loop [a [] n 1000000] (if (= n 0) a (recur [a] (- "
	                                            "n 1)))) (loop [a [] n 1000000] (if (= n 0) a (recur [a] (- n 1)))))")),
	                lisp::LispError);
	lisp::Interpreter chain(roomy); // closures in a chain: freed without recursion
	static_cast<void>(chain.eval("(loop [f nil n 1000000] (if (= n 0) f (recur (fn [] f) (- n 1))))"));
	// A pattern from a macro, too deep to compile
	CHECK(
	    errorOf(
	        "(defmacro m [] (let [p (loop [a 'x n 2000] (if (= n 0) a (recur [a] (- n 1))))] (list 'let [p 1] 2))) (m)")
	        .message()
	        .starts_with("a pattern nested more than"));
	// The reader's limits, and the compiler's
	CHECK(errorOf(std::string(100000, '(')).message() == "nested more than 256 levels deep");
	CHECK(errorOf(std::string(100000, '\'')).message() == "nested more than 256 levels deep");
	CHECK(errorOf(std::string(100000, '~')).message() == "nested more than 256 levels deep");
	CHECK(errorOf("(defmacro m [] (loop [a 1 n 2000] (if (= n 0) a (recur (list 'do a) (- n 1))))) (m)").message() ==
	      "macros nested too deeply");
	CHECK(errorOf("(defmacro m [] (loop [a 1 n 5000] (if (= n 0) (list 'quote a) (recur [a] (- n 1))))) (= (m) (m))")
	          .exhausted());
	CHECK(errorOf("(defmacro m [] (loop [a 1 n 2000] (if (= n 0) a (recur (list 'inc a) (- n 1))))) (+ (m))")
	          .message()
	          .starts_with("nested too deeply to compile"));
}

TEST_CASE("lisp: errors name the place, and only the error's own form") {
	const lisp::LispError error = errorOf("(def a 1)\n\n(defn broken [x]\n  (first x))\n(broken 5)");
	CHECK(error.file() == "test");
	CHECK(error.line() == 4);
	CHECK(error.column() == 3);
	CHECK(error.definition() == "broken");
	CHECK(std::string(error.what()) == "AffrescoUI lisp: test: line 4, column 3: in broken: \"first\" takes something "
	                                   "with items, and was given a number");
	// An error inside the language's own function names its caller
	const lisp::LispError inside = errorOf("(defn g [] (reduce 1 [1 2]))\n(g)");
	CHECK(inside.line() == 1);
	CHECK(inside.definition() == "g");
	CHECK(errorOf("[1\n 2\n 12px]").line() == 3);
	CHECK(errorOf("\"never").message() == "a string that never ends: expected '\"'");
	// What an error quotes is short
	CHECK(errorOf(std::string(100000, 'x')).message().size() < 100);
	CHECK(errorOf("(" + std::string(100000, 'x') + ")").message().size() < 100);
}

TEST_CASE("lisp: the language has no way out") {
	for(const char* name : {"slurp",     "spit",       "eval", "load",   "require", "exec",  "system", "shell", "open",
	                        "read-file", "write-file", "now",  "time",   "random",  "rand",  "getenv", "call",  "send",
	                        "connect",   "require",    "ffi",  "dlopen", "socket",  "print", "println"}) {
		CAPTURE(name);
		lisp::Interpreter interpreter;
		CHECK_FALSE(interpreter.global(name).has_value());
	}
}

TEST_CASE("lisp: the host's primitives pass through one gate, which sees who is on the stack") {
	lisp::Interpreter interpreter;
	std::vector<std::vector<std::string>> seen;
	std::vector<std::string> capabilities;
	interpreter.definePrimitive("effect", {.capability = "control",
	                                       .pure = false,
	                                       .call = [](lisp::Interpreter&, std::span<const lisp::Value> arguments) {
		                                       return arguments.empty() ? lisp::Value() : arguments[0];
	                                       }});
	interpreter.setGate([&](const lisp::CallContext& call) {
		seen.emplace_back(call.authors.begin(), call.authors.end());
		capabilities.emplace_back(call.capability);
		CHECK(call.primitive == "effect");
		CHECK(call.place.line != 0);
	});
	using Authors = std::vector<std::string>;
	const auto last = [&] { return seen.back(); };
	// Directly, through apply, in a variable, in a closure, through map: each passes through the gate
	CHECK(interpreter.print(interpreter.eval("(effect 1)", "a.lisp", "person")) == "1");
	CHECK(last() == Authors{"person"});
	static_cast<void>(interpreter.eval("(apply effect [1])", "a.lisp", "person"));
	CHECK(last() == Authors{"person"});
	static_cast<void>(interpreter.eval("(let [f effect] (f 1))", "a.lisp", "person"));
	CHECK(last() == Authors{"person"});
	static_cast<void>(interpreter.eval("(map effect [1])", "a.lisp", "person"));
	CHECK(last() == Authors{"person"});
	CHECK(capabilities.back() == "control");
	// An agent's function called by a person's: both on the stack; after it returns, only the person
	static_cast<void>(interpreter.eval("(defn helper [] (effect 1))", "b.lisp", "agent"));
	static_cast<void>(interpreter.eval("(defn command [] (+ 1 (or (helper) 0)))", "a.lisp", "person"));
	static_cast<void>(interpreter.eval("(command)", "a.lisp", "person"));
	CHECK(last() == Authors{"agent", "person"});
	static_cast<void>(interpreter.eval("(defn after [] (helper) (effect 2))", "a.lisp", "person"));
	static_cast<void>(interpreter.eval("(after)", "a.lisp", "person"));
	CHECK(last() == Authors{"person"});
	// A tail call doesn't launder: the agent's frame is replaced, and stays on the record
	static_cast<void>(interpreter.eval("(defn trusted [] (effect 1) nil)", "a.lisp", "person"));
	static_cast<void>(interpreter.eval("(defn launder [] (trusted))", "b.lisp", "agent"));
	static_cast<void>(interpreter.eval("(launder)", "b.lisp", "agent"));
	CHECK(last() == Authors{"agent", "person"});
	// A closure keeps its author, wherever it goes
	const lisp::Value closure = interpreter.eval("(fn [] (effect 1) nil)", "b.lisp", "agent");
	CHECK(interpreter.authorOf(closure) == "agent");
	static_cast<void>(interpreter.call(closure, {}));
	CHECK(last() == Authors{"agent"});
	static_cast<void>(interpreter.eval("(defn caller [f] (f) nil)", "a.lisp", "person"));
	static_cast<void>(
	    interpreter.call(interpreter.global("caller").value_or(lisp::Value()), std::vector<lisp::Value>{closure}));
	CHECK(last() == Authors{"agent", "person"});
	// A macro's output counts its author: an agent's macro writes a person's function
	static_cast<void>(interpreter.eval("(defmacro written [] `(effect 1))", "b.lisp", "agent"));
	static_cast<void>(interpreter.eval("(defn mine [] (written))", "a.lisp", "person"));
	static_cast<void>(interpreter.eval("(mine)", "a.lisp", "person"));
	CHECK(last() == Authors{"agent", "person"});
	// The least authority, by the host's ranking
	std::optional<std::string> least;
	interpreter.setGate([&](const lisp::CallContext&) {
		least = interpreter.leastAuthority([](std::string_view author) { return author == "person" ? 2 : 1; });
	});
	static_cast<void>(interpreter.eval("(mine)", "a.lisp", "person"));
	CHECK(least == "agent");
	// A refusal is a condition, and names the place
	interpreter.setGate([](const lisp::CallContext& call) {
		if(call.capability == "control") throw lisp::Refusal("not allowed");
	});
	CHECK(interpreter.print(interpreter.eval("(try (effect 1) (catch e (:message e)))", "a.lisp", "person")) ==
	      "\"not allowed\"");
	CHECK_THROWS_WITH_AS(static_cast<void>(interpreter.eval("(effect 1)", "a.lisp", "person")),
	                     "AffrescoUI lisp: a.lisp: line 1, column 1: not allowed", lisp::LispError);
	CHECK(interpreter.describe("trusted").value_or(lisp::DefinitionInfo{}).author == "person");
	CHECK(interpreter.describe("launder").value_or(lisp::DefinitionInfo{}).author == "agent");
	CHECK(interpreter.describe("nope") == std::nullopt);
}

TEST_CASE("lisp: JSON is viewed, not converted") {
	lisp::Interpreter interpreter;
	const auto state = std::make_shared<const json>(
	    json::parse(R"({"rooms": [{"id": 1, "name": "Hall"}], "big": 18446744073709551615, "x": 1.5})"));
	const lisp::Value view = interpreter.viewJson(state);
	CHECK(view.isView());
	CHECK(view.kind() == lisp::Kind::Map);
	CHECK(view.lookup("rooms").isView());
	CHECK(view.lookup("rooms").at(0).lookup("name").text() == "Hall");
	CHECK(interpreter.toTree(view) == *state);
	CHECK(interpreter.toTree(view).dump() == state->dump());
	static_cast<void>(interpreter.eval("(defn names [s] (map :name (:rooms s))) (defn rest-of [v] (rest (:rooms v)))"));
	const std::vector<lisp::Value> arguments = {view};
	CHECK(interpreter.print(interpreter.call(interpreter.global("names").value_or(lisp::Value()), arguments)) ==
	      "[\"Hall\"]");
	CHECK(interpreter.print(interpreter.call(interpreter.global("rest-of").value_or(lisp::Value()), arguments)) ==
	      "[]");
	CHECK(interpreter.print(view) == "{:rooms [{:id 1 :name \"Hall\"}] :big 18446744073709551615 :x 1.5}");
}

TEST_CASE("lisp: a program is split into entries, its text kept exactly") {
	const std::string text = ";;; a header\n\n(def a 1) ; one\n\n;; the function\n;; below\n(defn f \"doc\" []\n  "
	                         "a)\n#_ (old thing)\n(defkey \"ctrl+k\" f)\n(add-hook after-save f)\n; at the end\n";
	const std::vector<lisp::Entry> entries = lisp::splitEntries(text, "a.lisp", "me");
	REQUIRE(entries.size() == 4);
	CHECK(entries[0].name == "a");
	CHECK(entries[0].kind == "def");
	CHECK(entries[0].text == ";;; a header\n\n(def a 1) ; one");
	CHECK(entries[0].line == 1);
	CHECK(entries[1].name == "f");
	CHECK(entries[1].kind == "defn");
	CHECK(entries[1].text == ";; the function\n;; below\n(defn f \"doc\" []\n  a)");
	CHECK(entries[1].line == 5);
	CHECK(entries[1].author == "me");
	CHECK(entries[2].name == "defkey ctrl+k");
	CHECK(entries[2].text == "#_ (old thing)\n(defkey \"ctrl+k\" f)");
	CHECK(entries[3].name == "add-hook after-save f");
	CHECK(entries[3].text == "(add-hook after-save f)\n; at the end");
	CHECK(lisp::splitEntries("  \n ; only a comment\n", "a.lisp").empty());
	// Nothing is lost: every byte but white space is in an entry
	std::string joined;
	for(const lisp::Entry& entry : entries) joined += entry.text + "\n";
	const auto squeeze = [](std::string value) {
		std::erase_if(value, [](char c) { return c == ' ' || c == '\n'; });
		return value;
	};
	CHECK(squeeze(joined) == squeeze(text));
	CHECK_THROWS_AS(static_cast<void>(lisp::splitEntries("[1]\n[2]", "a.lisp")), lisp::LispError);
	CHECK_THROWS_AS(static_cast<void>(lisp::splitEntries("(defn)", "a.lisp")), lisp::LispError);
	// The kinds of file
	CHECK(lisp::isProgram("(defn f [] 1)"));
	CHECK(lisp::isProgram("(def a 1) (def b 2)"));
	CHECK(lisp::isProgram("(panel) (panel)"));
	CHECK_FALSE(lisp::isProgram("(panel#x (defn))"));
	CHECK_FALSE(lisp::isProgram("(node :type defn)"));
	CHECK_FALSE(lisp::isProgram("{:a 1}"));
	CHECK_FALSE(lisp::isProgram("; nothing"));
	CHECK(lisp::needsRunning("; a\n\n(defn f [] 1)") == "\"f\", at line 1");
	CHECK(lisp::needsRunning("(panel :text ~(f))") == "the ~ at line 1, column 14");
	CHECK(lisp::needsRunning("(panel :text \"~(f)\")") == std::nullopt);
	CHECK(lisp::needsRunning("(((") == std::nullopt);
	CHECK(lisp::needsRunning("(defn)") == "the form at line 1");
}

TEST_CASE("lisp: a program builds the format's sections, whatever the order of its definitions") {
	const std::string text = R"lisp(
(defn card "One room." [room]
  `(panel :id ~(str "room-" (:id room))
     (label :text ~(:name room))))

(def rooms [{:id "a" :name "Kitchen"} {:id "b" :name "Hall"}])

(defcommand toggle "Toggles it." :title "Toggle" :category "View" [times] (other times))
(defcommand another :title "Another" [] nil)

(defkey "mod+b" toggle)
(defkey "mod+n" {:run another})

(defcustom list-size 60 "How many." :type integer :minimum 1)

(defscreen extra (panel#extra))

(defscreen main "The main screen." :window {:title "Rooms"} :state {:count ~(count rooms)}
  (column#rooms ~@(map card rooms)))

(add-hook after-save "After saving." toggle)
(advice toggle :around another)
)lisp";
	lisp::Program program;
	program.addFile(text, "a.lisp", "me");
	const json tree = program.build();
	const json expected = json::parse(R"({
		"format": "affrescoui-screen",
		"version": 2,
		"window": {"title": "Rooms"},
		"state": {"count": 2},
		"root": {"type": "column", "id": "rooms", "children": [
			{"type": "panel", "id": "room-a", "children": [{"type": "label", "text": "Kitchen"}]},
			{"type": "panel", "id": "room-b", "children": [{"type": "label", "text": "Hall"}]}]},
		"screens": {"extra": {"type": "panel", "id": "extra"}},
		"actions": {
			"another": {"title": "Another", "do": [{"lisp": "another"}]},
			"toggle": {"title": "Toggle", "category": "View", "description": "Toggles it.", "do": [{"lisp": "toggle"}]}},
		"keys": {"mod+b": "toggle", "mod+n": {"run": "another"}},
		"settings": {"list-size": {"type": "integer", "default": 60, "minimum": 1, "description": "How many."}}
	})");
	CHECK(tree.dump() == expected.dump());
	REQUIRE(program.extensions().size() == 2);
	CHECK(program.extensions()[0].kind == "add-hook");
	CHECK(program.extensions()[0].name == "add-hook after-save toggle");
	CHECK(program.extensions()[0].doc == "After saving.");
	CHECK(program.extensions()[0].targets == std::vector<std::string>{"after-save", "toggle"});
	CHECK(program.extensions()[1].name == "advice toggle around another");
	CHECK(program.extensions()[1].author == "me");
	// A name only a function's body uses isn't an error until it runs
	REQUIRE(program.undefinedNames().size() == 1);
	CHECK(program.undefinedNames()[0].name == "other");
	CHECK(program.undefinedNames()[0].definition == "toggle");

	// The same source gives the same tree, and moving definitions changes nothing
	lisp::Program moved;
	std::vector<lisp::Entry> entries = lisp::splitEntries(text, "a.lisp", "me");
	std::ranges::reverse(entries);
	for(lisp::Entry& entry : entries) moved.addEntry(std::move(entry));
	CHECK(moved.build().dump() == expected.dump());
	CHECK(lisp::buildText(text, "a.lisp").dump() == expected.dump());

	// Macros first, wherever they are, using functions below them
	CHECK(lisp::buildText(
	          "(defscreen main (rows ~(wrap 1)))\n(defn helper [x] [x])\n(defmacro wrap [x] `(helper ~x))")["root"]
	          .dump() == R"({"type":"rows","children":[[1]]})");
	// A macro that gives definitions counts as one
	const json made = lisp::buildText("(defmacro twin [name] `(do (defn ~name [] 1) (defcommand ~(symbol (str name "
	                                  "\"-too\")) :title \"T\" [] nil)))\n(twin thing)");
	CHECK(made["actions"].contains("thing-too"));
	// A value depending on a later one, and on itself
	CHECK(lisp::buildText("(defscreen main (a :v ~later))\n(def later (+ earlier 1))\n(def earlier 1)")["root"]["v"] ==
	      2);
	CHECK(buildError("(def a b)\n(def b a)\n(defscreen main (x :v ~a))").message().contains("depends on itself"));
}

TEST_CASE("lisp: what a program can get wrong names both places") {
	const lisp::LispError twice = buildError("(defn f [] 1)\n\n(def f 2)");
	CHECK(twice.message() == "\"f\" is defined twice: at test.lisp: line 1 and at test.lisp: line 3");
	CHECK(twice.line() == 3);
	lisp::Program program;
	program.addFile("(defn f [] 1)", "a.lisp");
	try {
		program.addFile("\n(defn f [] 2)", "b.lisp");
		FAIL("it took it");
	} catch(const lisp::LispError& error) {
		CHECK(error.message() == "\"f\" is defined twice: at a.lisp: line 1 and at b.lisp: line 2");
	}
	CHECK(buildError("(defmacro m [a] `(def x 1))\n(m a)(def x 2)").message().contains("defined twice"));
	CHECK(buildError("(def a 1) (foo b)").message() ==
	      "a program's top level holds definitions: \"foo\" is neither a definition nor a macro");
	CHECK(buildError("(def a 1) [2]").message().contains("each with a name"));
	CHECK(buildError("(defscreen main)").message() == "defscreen is missing what it defines");
	CHECK(buildError("(defscreen main (a) (b))").message() == "defscreen holds one root node, after its options");
	CHECK(buildError("(defcommand c :title \"T\")").message() ==
	      "defcommand takes its parameters, a vector, before its body");
	CHECK(buildError("(defn f)").message() == "defn is missing what it defines");
	CHECK(buildError("(def 1 2)").message() == "def takes a name first");
	CHECK(buildError("(defn %x [] 1)").message() == "a name starting with % is the language's own");
	CHECK(buildError("(defn f [] (defn g [] 1))").message() == "\"defn\" goes at the top level of a program");
	CHECK(buildError("(defscreen main (a :b))").message() == "the key \"b\" has no value");
	CHECK(buildError("(defscreen main (a ~(first 5)))").definition() == "main");
	// An entry given by hand is what it says
	lisp::Entry entry;
	entry.file = "a.lisp";
	entry.name = "g";
	entry.kind = "defn";
	entry.text = "(defn f [] 1)";
	lisp::Program wrong;
	CHECK_THROWS_AS(wrong.addEntry(entry), lisp::LispError);
	entry.name = "f";
	entry.text = "(defn f [] 1) (defn g [] 1)";
	lisp::Program two;
	CHECK_THROWS_AS(two.addEntry(entry), lisp::LispError);
	CHECK(two.entries().empty());
}

TEST_CASE("lisp: building is pure") {
	lisp::Program program;
	program.definePure("double", {.pure = true, .call = [](lisp::Interpreter&, std::span<const lisp::Value> arguments) {
		                              return lisp::Value::integer(arguments[0].asInteger() * 2);
	                              }});
	program.addFile("(defscreen main (x :v ~(double 2)))", "a.lisp");
	CHECK(program.build()["root"]["v"] == 4);
	CHECK_THROWS_AS(program.definePure("effect", {.capability = "control", .pure = true}), std::invalid_argument);
	CHECK_THROWS_AS(program.definePure("effect", {.pure = false}), std::invalid_argument);

	// An impure primitive, with or without a gate, is refused while pure
	lisp::Interpreter interpreter;
	bool ran = false;
	interpreter.definePrimitive("effect",
	                            {.pure = false, .call = [&ran](lisp::Interpreter&, std::span<const lisp::Value>) {
		                             ran = true;
		                             return lisp::Value();
	                             }});
	interpreter.setPure(true);
	CHECK_THROWS_AS(static_cast<void>(interpreter.eval("(effect)")), lisp::LispError);
	CHECK_FALSE(ran);
	interpreter.setPure(false);
	static_cast<void>(interpreter.eval("(effect)"));
	CHECK(ran);

	// A function body that isn't called doesn't run; one that is can't reach what isn't there
	CHECK(buildError("(defn f [] (kill))\n(def x (f))\n(defscreen main (a))").message() == "\"kill\" is not defined");
	CHECK(lisp::buildText("(defn f [] (kill))\n(defscreen main (a))")["root"]["type"] == "a");
	// The same text, again and again, the same tree
	const std::string text = "(defmacro m [] (let [g (gensym)] `(quote ~g)))\n(defscreen main (a :x ~(str (m))))";
	const std::string first = lisp::buildText(text).dump();
	CHECK(first.contains("g__1"));
	for(int i = 0; i < 3; ++i) CHECK(lisp::buildText(text).dump() == first);
	// A build that runs out of work refuses, with its place
	lisp::Limits low;
	low.budget.operations = 1000;
	const lisp::LispError spent = buildError("(defscreen main\n  (a ~(loop [] (recur))))", low);
	CHECK(spent.exhausted());
	CHECK(spent.definition() == "main");
}

TEST_CASE("lisp: a data file is a template") {
	CHECK(lisp::buildText("(label#a :text \"x\")").dump() == R"({"type":"label","id":"a","text":"x"})");
	CHECK(lisp::buildText("(panel #a :color transparent (node #b :style caption))").dump() ==
	      R"({"type":"panel","id":"a","color":"transparent","children":[{"id":"b","style":"caption"}]})");
	CHECK(lisp::buildText("(node#a :type row)").dump() == R"({"id":"a","type":"row"})");
	CHECK(lisp::buildText("[a Cut @x.y $t state.items -1 1.5e3 true nil]").dump() ==
	      R"(["a","Cut","@x.y","$t","state.items",-1,1500.0,true,null])");
	CHECK(lisp::buildText("{:a b \"c d\" {:e []}}").dump() == R"({"a":"b","c d":{"e":[]}})");
	// ~ drops into code
	CHECK(lisp::buildText("(label :text ~(str \"a\" 1))").dump() == R"({"type":"label","text":"a1"})");
	CHECK(lisp::buildText("(row ~@(map (fn [i] {:i i}) [1 2]))").dump() ==
	      R"({"type":"row","children":[{"i":1},{"i":2}]})");
	CHECK(lisp::buildText("{:a ~(+ 1 2)}").dump() == R"({"a":3})");
	CHECK(lisp::buildText("~(+ 1 2)") == 3);
	// A list that comes out of code is a node too
	CHECK(lisp::buildText("(row ~(list 'label :text \"x\"))").dump() ==
	      R"({"type":"row","children":[{"type":"label","text":"x"}]})");
	// A word the language runs isn't a node type; written as a type, it is
	const lisp::LispError error = buildError("{:a 1\n :when\n   (not state.x)}");
	CHECK(error.line() == 3);
	CHECK(
	    error.message() ==
	    "\"not\" is not a node type; to run it, write ~(not ...); for a node of that type, write (node :type not ...)");
	CHECK(std::string(error.what()).contains("test.lisp: line 3"));
	CHECK(buildError("(= a b)").message() == "\"=\" is not a node type; to run it, write ~(= ...)");
	CHECK(lisp::buildText("(node :type not :x 1)").dump() == R"({"type":"not","x":1})");
	CHECK(lisp::buildText("(node#a :type map)").dump() == R"({"id":"a","type":"map"})");
	CHECK(lisp::buildText("(list#a)").dump() == R"({"type":"list","id":"a"})"); // list is a node type
	CHECK(lisp::buildText("(a :when ~(not false))")["when"] == true);
	// The notation's limits and mistakes
	CHECK(buildError("").message() == "expected a value, found the end of the text");
	CHECK(buildError("(row :children [] (a))").message() == "a node with :children and children after its properties");
	CHECK(buildError("(row (a) :gap 1)").message() == "':gap' after the node's children: properties come first");
	CHECK(buildError("(row #a #b)").message() == "the key \"id\" twice: a key has one value");
	CHECK(buildError("{:a 1 :a 2}").message() == "the key \"a\" twice: a key has one value");
	CHECK(buildError("()").message().starts_with("expected the node's type"));
	CHECK(buildError("(1)").message().starts_with("expected the node's type"));
	CHECK(buildError("{a 1}").message() == "a map's key is a keyword or a string, not a symbol");
	std::string nodes;
	for(int i = 0; i < 129; ++i) nodes += "(row ";
	CHECK(buildError(nodes + std::string(129, ')')).message() == "nested more than 256 levels deep");
	CHECK(affresco::notation::read(nodes.substr(5) + std::string(128, ')'), affresco::notation::Notation::Lisp) ==
	      lisp::buildText(nodes.substr(5) + std::string(128, ')')));
}

TEST_CASE("lisp: every example's Lisp file builds to the tree the notation's reader gives") {
	int files = 0;
	for(const auto& entry : std::filesystem::recursive_directory_iterator(AFFRESCOUI_EXAMPLES_DIRECTORY)) {
		if(entry.path().extension() != ".lisp") continue;
		const std::string text = textOf(entry.path());
		CAPTURE(entry.path().string());
		if(lisp::isProgram(text)) continue; // an app written as a program: the tests of lisp_runtime.cpp
		const json read = affresco::notation::read(text, affresco::notation::Notation::Lisp);
		CHECK(lisp::buildText(text, entry.path().string()).dump() == read.dump());
		CHECK(lisp::needsRunning(text) == std::nullopt);
		++files;
	}
	CHECK(files > 150);
}

TEST_CASE("lisp: the notation's list of the language's words is the language's") {
	lisp::Interpreter interpreter;
	// Every plain word the language gives a meaning to is on the list, but list, which is a node type
	const std::vector<std::string> forms = {"quote",     "backquote",  "unquote", "unquote-splicing",
	                                        "if",        "do",         "when",    "cond",
	                                        "and",       "or",         "let",     "loop",
	                                        "recur",     "fn",         "try",     "catch",
	                                        "throw",     "def",        "defn",    "defmacro",
	                                        "defscreen", "defcommand", "defkey",  "defcustom",
	                                        "add-hook",  "advice"};
	for(const std::string& form : forms) {
		CAPTURE(form);
		CHECK(affresco::notation::isLanguageWord(form));
	}
	std::vector<std::string> functions(affresco::notation::languageWords.begin(),
	                                   affresco::notation::languageWords.end());
	std::erase_if(functions, [&](const std::string& word) { return std::ranges::find(forms, word) != forms.end(); });
	for(const std::string& word : functions) {
		CAPTURE(word);
		CHECK(interpreter.global(word).has_value());
	}
	CHECK(interpreter.global("list").has_value());
	CHECK_FALSE(affresco::notation::isLanguageWord("list"));
	// And the words of the standard library the list could miss
	for(const char* word : {"map",   "filter", "reduce", "find",   "first", "rest",     "cons",  "conj",
	                        "assoc", "dissoc", "get",    "get-in", "merge", "keys",     "vals",  "count",
	                        "str",   "range",  "apply",  "not",    "mod",   "identity", "gensym"}) {
		CAPTURE(word);
		CHECK(affresco::notation::isLanguageWord(word));
		CHECK(interpreter.global(word).has_value());
	}
}
