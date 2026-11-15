/// @file
/// @brief Unit tests for the language's everyday functions (modules/lisp/reference/functions.md): the tests on numbers
/// and kinds, the sequence functions, the functions that take functions, the widened `map`, `mapcat`, `sort`,
/// `sort-by`, `reduce` and `conj`, a keyword equal to the string of its text (decision D2) and a map's key kept as text
/// (decision D3). Each name has its usual case, an edge and an error.

#include "LispApp.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace lisp = affresco::lisp;
using affresco::json;

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
/// @return Its message, or "it ran".
std::string errorOf(std::string_view text) {
	lisp::Interpreter interpreter;
	try {
		static_cast<void>(interpreter.eval(text, "test", "tester"));
	} catch(const lisp::LispError& error) {
		return error.message();
	}
	return "it ran";
}

} // namespace

TEST_CASE("lisp functions: the tests on numbers") {
	CHECK(run("[(even? 4) (even? -3) (odd? 3) (odd? 4.0) (even? 0)]") == "[true false true false true]");
	CHECK(errorOf("(even? 1.5)") == "\"even?\" takes a whole number");
	CHECK(errorOf("(odd? \"1\")") == "\"odd?\" takes a whole number");
	CHECK(run("[(zero? 0) (zero? 0.0) (zero? 1) (pos? 2) (pos? 0) (neg? -0.5) (neg? 0)]") ==
	      "[true true false true false true false]");
	CHECK(run("(let [n (- (* 1e308 10) (* 1e308 10))] [(pos? n) (neg? n) (zero? n)])") == "[false false false]");
	CHECK(errorOf("(zero? nil)").find("\"zero?\"") != std::string::npos);
	CHECK(run("[(true? true) (true? 1) (false? false) (false? nil)]") == "[true false true false]");
	CHECK(run("[(integer? 1) (integer? 1.0) (int? 18446744073709551615) (float? 1.5) (double? 1)]") ==
	      "[true false true true false]");
	CHECK(run("[(nat-int? 0) (nat-int? -1) (pos-int? 1) (pos-int? 0) (neg-int? -1) (neg-int? -1.0)]") ==
	      "[true false true false true false]");
	CHECK(errorOf("(integer? 1 2)") == "a test takes one value");
}

TEST_CASE("lisp functions: quot, int, double, == and boolean") {
	CHECK(run("[(quot 7 2) (quot -7 2) (quot 7 -2) (quot 7.5 2)]") == "[3 -3 -3 3.0]");
	CHECK(run("(quot -9223372036854775808 -1)") == "9.223372036854776e+18");
	CHECK(errorOf("(quot 1 0)") == "dividing by zero");
	CHECK(run("[(int 3.9) (int -3.9) (int 7)]") == "[3 -3 7]");
	CHECK(errorOf("(int 1e30)") == "\"int\" takes a number within an integer's range");
	CHECK(errorOf("(int \"1\")").find("\"int\"") != std::string::npos);
	CHECK(run("[(double 2) (double 1.5)]") == "[2.0 1.5]");
	CHECK(run("[(== 1 1.0) (== 1 1 1) (== 1 2) (== 1) (== 18446744073709551615 -1)]") ==
	      "[true true false true false]");
	CHECK(errorOf("(== 1 :a)").find("\"==\"") != std::string::npos);
	CHECK(run("[(boolean nil) (boolean false) (boolean 0) (boolean \"\")]") == "[false false true true]");
	CHECK(errorOf("(boolean)") == "\"boolean\" takes one value");
}

TEST_CASE("lisp functions: compare") {
	CHECK(run("[(compare 1 2) (compare 2 2.0) (compare 3 1) (compare \"a\" \"b\") (compare :b \"a\")]") ==
	      "[-1 0 1 -1 1]");
	CHECK(run("[(compare nil 1) (compare nil nil) (compare \"a\" nil)]") == "[-1 0 1]");
	CHECK(errorOf("(compare 1 \"a\")") ==
	      "\"compare\" compares numbers with numbers and text with text, and was given a number and a string");
	CHECK(errorOf("(compare true false)").find("\"compare\"") != std::string::npos);
}

TEST_CASE("lisp functions: the bit operations") {
	CHECK(run("[(bit-and 12 10) (bit-or 12 10) (bit-xor 12 10) (bit-and 7 6 4) (bit-not 0)]") == "[8 14 6 4 -1]");
	CHECK(run("[(bit-shift-left 1 3) (bit-shift-right -16 2) (bit-shift-left 1 63)]") == "[8 -4 -9223372036854775808]");
	CHECK(errorOf("(bit-and 1)").find("\"bit-and\"") != std::string::npos);
	CHECK(errorOf("(bit-or 1.5 1)") == "\"bit-or\" takes whole numbers");
	CHECK(errorOf("(bit-shift-left 1 64)") == "\"bit-shift-left\" shifts by 0 to 63 places");
}

TEST_CASE("lisp functions: the tests on kinds") {
	CHECK(run("[(coll? []) (coll? {}) (coll? \"a\") (seq? '(1)) (seq? [1])]") == "[true true false true false]");
	CHECK(run("[(sequential? [1]) (sequential? {}) (associative? {}) (associative? [1]) (associative? '(1))]") ==
	      "[true false true true false]");
	CHECK(run("[(ident? :a) (ident? 'a) (ident? \"a\") (ifn? :a) (ifn? {}) (ifn? inc) (ifn? 1)]") ==
	      "[true true false true true true false]");
	CHECK(errorOf("(coll?)") == "a test takes one value");
}

TEST_CASE("lisp functions: seq, next and their family") {
	CHECK(run("[(seq [1 2]) (seq '(1)) (seq []) (seq nil) (seq \"\")]") == "[[1 2] (1) nil nil nil]");
	CHECK(run("(seq {:a 1})") == "[[\"a\" 1]]");
	CHECK(run("(seq \"ab\")") == "[\"a\" \"b\"]");
	CHECK(errorOf("(seq 1)").find("a number") != std::string::npos);
	CHECK(run("[(next [1 2 3]) (next [1]) (next nil) (nnext [1 2 3]) (nnext [1 2])]") == "[[2 3] nil nil [3] nil]");
	CHECK(run("[(ffirst [[1 2] [3]]) (fnext [1 2]) (fnext [1]) (nfirst [[1 2 3]]) (nfirst [[1]])]") ==
	      "[1 2 nil [2 3] nil]");
	CHECK(run("[(nthrest [1 2 3] 2) (nthrest [1] 5) (nthnext [1 2 3] 2) (nthnext [1] 5) (nthrest [1 2] -1)]") ==
	      "[[3] [] [3] nil [1 2]]");
	CHECK(errorOf("(nthrest [1] :a)").find("\"nthrest\" takes a whole number") != std::string::npos);
}

TEST_CASE("lisp functions: peek, pop, subvec and the ends of a collection") {
	CHECK(run("[(peek [1 2 3]) (peek '(1 2 3)) (peek []) (peek nil)]") == "[3 1 nil nil]");
	CHECK(run("[(pop [1 2 3]) (pop '(1 2 3)) (pop nil)]") == "[[1 2] (2 3) nil]");
	CHECK(errorOf("(pop [])") == "\"pop\" was given an empty vector");
	CHECK(errorOf("(peek {})").find("\"peek\" takes a list or a vector") != std::string::npos);
	CHECK(run("[(subvec [1 2 3 4] 1 3) (subvec [1 2 3] 1) (subvec [1 2] 2)]") == "[[2 3] [2 3] []]");
	CHECK(errorOf("(subvec [1 2] 1 5)") == "\"subvec\": an index outside the vector");
	CHECK(errorOf("(subvec '(1 2) 0)").find("\"subvec\" takes a vector") != std::string::npos);
	CHECK(run("[(butlast [1 2 3]) (butlast [1]) (take-last 2 [1 2 3]) (take-last 0 [1]) (take-last 5 [1 2])]") ==
	      "[[1 2] nil [2 3] nil [1 2]]");
	CHECK(run("[(drop-last [1 2 3]) (drop-last 2 [1 2 3]) (drop-last 5 [1]) (drop-last [])]") == "[[1 2] [1] [] []]");
	CHECK(run("[(take-nth 2 [1 2 3 4 5]) (take-nth 1 []) (split-at 2 [1 2 3]) (split-at 9 [1])]") ==
	      "[[1 3 5] [] [[1 2] [3]] [[1] []]]");
	CHECK(errorOf("(take-nth 0 [1])").find("at least 1") != std::string::npos);
}

TEST_CASE("lisp functions: interpose, interleave and partitions") {
	CHECK(run("[(interpose 0 [1 2 3]) (interpose 0 []) (interleave [1 2 3] [:a :b]) (interleave)]") ==
	      "[[1 0 2 0 3] [] [1 :a 2 :b] []]");
	CHECK(run("(partition 2 [1 2 3 4 5])") == "[[1 2] [3 4]]");
	CHECK(run("(partition 3 1 [1 2 3 4])") == "[[1 2 3] [2 3 4]]");
	CHECK(run("(partition 2 2 [:x] [1 2 3])") == "[[1 2] [3 :x]]");
	CHECK(run("(partition 3 3 [] [1 2 3 4])") == "[[1 2 3] [4]]");
	CHECK(run("[(partition-all 2 [1 2 3]) (partition-all 2 1 [1 2 3]) (partition 2 [])]") ==
	      "[[[1 2] [3]] [[1 2] [2 3] [3]] []]");
	CHECK(errorOf("(partition 0 [1])").find("at least 1") != std::string::npos);
}

TEST_CASE("lisp functions: distinct, dedupe and flatten") {
	CHECK(run("(distinct [1 2 1 3 2])") == "[1 2 3]");
	CHECK(run("(distinct [1 1.0 :a \"a\" [1 2] '(1 2) {:a 1} {\"a\" 1}])") == "[1 :a [1 2] {:a 1}]");
	CHECK(run("[(distinct []) (distinct nil)]") == "[[] []]");
	CHECK(run("[(dedupe [1 1 2 1 1]) (dedupe [])]") == "[[1 2 1] []]");
	CHECK(run("[(flatten [1 [2 [3 '(4)]] []]) (flatten 1) (flatten nil)]") == "[[1 2 3 4] [] []]");
	CHECK(errorOf("(distinct 1)").find("a number") != std::string::npos);
}

TEST_CASE("lisp functions: building maps keeps a key as text (decision D3)") {
	CHECK(run("(frequencies [:a :b :a \"a\"])") == "{:a 3 :b 1}");
	CHECK(errorOf("(frequencies [1 1 2])") ==
	      "a map's key is a keyword or a string, not a number: give the key as text, (frequencies (map str xs))");
	CHECK(run("(frequencies (map str [1 1 2]))") == "{\"1\" 2 \"2\" 1}");
	CHECK(run("[(zipmap [:a :b] [1 2 3]) (zipmap [] [1]) (zipmap [:a :a] [1 2])]") == "[{:a 1 :b 2} {} {:a 2}]");
	CHECK(errorOf("(zipmap [1] [2])").find("(zipmap (map str ks) vs)") != std::string::npos);
	CHECK(run("[(hash-map :a 1 \"b\" 2) (hash-map)]") == "[{:a 1 :b 2} {}]");
	CHECK(errorOf("(hash-map :a)") == "\"hash-map\" takes pairs of a key and a value");
	CHECK(run("[(select-keys {:a 1 :b 2} [:a :c]) (select-keys nil [:a]) (select-keys {\"a\" 1} [:a])]") ==
	      "[{:a 1} {} {:a 1}]");
	CHECK(errorOf("(select-keys [1] [0])").find("\"select-keys\" takes a map") != std::string::npos);
	CHECK(errorOf("(group-by count [[1]])").find("(group-by (comp str f) xs)") != std::string::npos);
	CHECK(errorOf("(group-by even? [1 2])") ==
	      "a map's key is a keyword or a string, not a boolean: give the key as text, (group-by (comp str f) xs)");
	CHECK(run("(group-by (fn [x] (if (even? x) :even :odd)) [1 2 3 4])") == "{:odd [1 3] :even [2 4]}");
	CHECK(run("(group-by (comp str even?) [1 2 3])") == "{\"false\" [1 3] \"true\" [2]}");
}

TEST_CASE("lisp functions: into, vector, list*, not-empty, rseq, key, val, doall, dorun") {
	CHECK(run("[(into [1] '(2 3)) (into '(1) [2 3]) (into nil [1]) (into {} [[:a 1] {:b 2} nil])]") ==
	      "[[1 2 3] (3 2 1) [1] {:a 1 :b 2}]");
	CHECK(run("(into {:a 1} {:a 2})") == "{:a 2}");
	CHECK(errorOf("(into 1 [])").find("\"into\" takes a vector, a list, a map, a set or nil") != std::string::npos);
	CHECK(errorOf("(into [] inc [1])").find("not a transducer") != std::string::npos);
	CHECK(errorOf("(into {} [[1 2]])").find("(conj m [(str key) value])") != std::string::npos);
	CHECK(run("[(vector 1 2) (vector) (list* 1 2 [3]) (list* []) (list* nil)]") == "[[1 2] [] (1 2 3) nil nil]");
	CHECK(run("[(not-empty []) (not-empty [1]) (not-empty \"\") (not-empty {}) (not-empty nil)]") ==
	      "[nil [1] nil nil nil]");
	CHECK(errorOf("(not-empty 1)").find("\"not-empty\"") != std::string::npos);
	CHECK(run("[(rseq [1 2 3]) (rseq [])]") == "[[3 2 1] nil]");
	CHECK(errorOf("(rseq '(1))").find("\"rseq\" takes a vector") != std::string::npos);
	CHECK(run("(map (fn [e] [(key e) (val e)]) {:a 1})") == "[[\"a\" 1]]");
	CHECK(errorOf("(key 1)").find("\"key\" takes a [key value] pair") != std::string::npos);
	CHECK(run("[(doall [1 2]) (doall 1 [1]) (dorun [1])]") == "[[1 2] [1] nil]");
}

TEST_CASE("lisp functions: map, mapcat, reduce and conj, widened") {
	CHECK(run("(map + [1 2 3] [10 20])") == "[11 22]");
	CHECK(run("(map vector [1 2] '(:a :b) \"xy\")") == "[[1 :a \"x\"] [2 :b \"y\"]]");
	CHECK(run("(map + [] [1])") == "[]");
	CHECK(run("(mapcat (fn [a b] [a b]) [1 2] [3 4])") == "[1 3 2 4]");
	CHECK(run("(mapv inc [1 2])") == "[2 3]");
	CHECK(run("[(reduce + []) (reduce * []) (reduce + [5]) (reduce + 1 [])]") == "[0 1 5 1]");
	CHECK(errorOf("(reduce max [])").find("\"max\"") != std::string::npos);
	CHECK(run("(conj {:a 1} [:b 2] {:c 3} nil)") == "{:a 1 :b 2 :c 3}");
	CHECK(errorOf("(conj {} [1 2])") ==
	      "a map's key is a keyword or a string, not a number: give the key as text, (conj m [(str key) value])");
	CHECK(errorOf("(conj {} 1)").find("[key value] pairs or maps") != std::string::npos);
}

TEST_CASE("lisp functions: sort and sort-by with a comparator") {
	CHECK(run("[(sort [3 1 2]) (sort > [3 1 2]) (sort < [3 1 2]) (sort compare [\"b\" :a])]") ==
	      "[[1 2 3] [3 2 1] [1 2 3] [:a \"b\"]]");
	CHECK(run("(sort (fn [a b] (> a b)) [3 1 2 5 4 0])") == "[5 4 3 2 1 0]");
	CHECK(run("(sort (fn [a b] (compare b a)) [1 3 2])") == "[3 2 1]");
	// Stable: equal items keep their order, with any comparator
	CHECK(run("(sort (fn [a b] (< (first a) (first b))) [[1 :a] [0 :b] [1 :c] [0 :d]])") ==
	      "[[0 :b] [0 :d] [1 :a] [1 :c]]");
	CHECK(run("[(sort (fn [a b] true) []) (sort (fn [a b] true) [1])]") == "[[] [1]]");
	CHECK(errorOf("(sort (fn [a b] :x) [1 2])") == "a comparator gives a boolean or a number, and gave a keyword");
	CHECK(errorOf("(sort < [1 \"a\"])") == "\"<\" takes numbers, and was given a string");
	CHECK(run("(sort-by :n > [{:n 1} {:n 3} {:n 2}])") == "[{:n 3} {:n 2} {:n 1}]");
	CHECK(run("(sort-by :n (fn [a b] (compare b a)) [{:n 1} {:n 3}])") == "[{:n 3} {:n 1}]");
	CHECK(run("(sort-by count [[1 2] [1] []])") == "[[] [1] [1 2]]");
}

TEST_CASE("lisp functions: keep, take-while and the splits") {
	CHECK(run("(keep (fn [x] (if (even? x) (* x x))) [1 2 3 4])") == "[4 16]");
	CHECK(run("(keep (fn [x] x) [false nil 1])") == "[false 1]");
	CHECK(run("(keep-indexed (fn [i x] (if (even? i) x)) [:a :b :c])") == "[:a :c]");
	CHECK(run("(filterv odd? [1 2 3])") == "[1 3]");
	CHECK(run("[(take-while pos? [1 2 -1 3]) (take-while pos? []) (drop-while pos? [1 2 -1 3])]") ==
	      "[[1 2] [] [-1 3]]");
	CHECK(run("(split-with pos? [1 -2 3])") == "[[1] [-2 3]]");
	CHECK(run("[(partition-by even? [1 3 2 4 5]) (partition-by even? [])]") == "[[[1 3] [2 4] [5]] []]");
	CHECK(errorOf("(take-while pos? 1)").find("a number") != std::string::npos);
}

TEST_CASE("lisp functions: reductions, reduce-kv, merge-with, update-vals and update-keys") {
	CHECK(run("[(reductions + [1 2 3]) (reductions + 10 [1 2]) (reductions + [])]") == "[[1 3 6] [10 11 13] [0]]");
	CHECK(run("(reduce-kv (fn [m k v] (assoc m k (inc v))) {} {:a 1 :b 2})") == "{:a 2 :b 3}");
	CHECK(run("(reduce-kv (fn [s i x] (+ s (* i x))) 0 [5 6 7])") == "20");
	CHECK(run("[(merge-with + {:a 1} {:a 2 :b 3} nil) (merge-with +) (merge-with + nil)]") == "[{:a 3 :b 3} nil nil]");
	CHECK(run("[(update-vals {:a 1 :b 2} inc) (update-vals {} inc)]") == "[{:a 2 :b 3} {}]");
	CHECK(run("(update-keys {:a 1 :b 2} (fn [k] (str k \"!\")))") == "{\"a!\" 1 \"b!\" 2}");
	CHECK(errorOf("(update-keys {:a 1} count)") ==
	      "a map's key is a keyword or a string, not a number: give the key as text, (update-keys m (comp str f))");
}

TEST_CASE("lisp functions: functions that make functions") {
	CHECK(run("[((partial + 1 2) 3) ((partial str) \"a\")]") == "[6 \"a\"]");
	CHECK(run("[((comp inc inc) 1) ((comp) 5) ((comp str +) 1 2)]") == "[3 5 \"3\"]");
	CHECK(run("((juxt :a :b) {:a 1})") == "[1 nil]");
	CHECK(run("((juxt inc dec) 5)") == "[6 4]");
	CHECK(run("[((complement even?) 3) ((constantly 7) 1 2) ((constantly nil))]") == "[true 7 nil]");
	CHECK(run("[((fnil inc 0) nil) ((fnil + 0 10) nil nil) ((fnil inc 0) 5)]") == "[1 10 6]");
	CHECK(run("[((every-pred pos? even?) 2 4) ((every-pred pos?) 1 -1) ((some-fn even? neg?) 1 3)]") ==
	      "[true false nil]");
	CHECK(run("((some-fn even? neg?) 1 -3)") == "true");
	CHECK(run("[(not-any? even? [1 3]) (not-every? even? [2 3])]") == "[true true]");
	CHECK(run("[(max-key count [1] [1 2] [3]) (min-key count [1] [1 2] [3]) (max-key - 4)]") == "[[1 2] [3] 4]");
	CHECK(run("(run! inc [1 2])") == "nil");
	CHECK(errorOf("((partial 1) 2)").find("this isn't a function") != std::string::npos);
}

TEST_CASE("lisp functions: a keyword is the string of its text, in a function as in a binding (decision D2)") {
	CHECK(run("[(= :a \"a\") (= \"a\" :a) (not= :a \"a\") (= :a :b) (= [:a {:b :c}] [\"a\" {\"b\" \"c\"}])]") ==
	      "[true true false false true]");
	CHECK(run("(= 'a \"a\")") == "false");
	const lispapp::App app(R"lisp(
(defn done? [status] (= status :done))
(defscreen main :state {:status "done"}
  (column#root
    (label#function :text ~(str (done? state.status)))
    (label#binding :text ~(str (= state.status :done)))))
)lisp");
	const auto text = [&](const char* id) {
		const auto* label = dynamic_cast<affresco::Label*>(app.window->root().findById(id));
		return label ? label->text() : std::string("none");
	};
	CHECK(text("function") == "true");
	CHECK(text("binding") == text("function"));
}

TEST_CASE("lisp functions: a list or a vector of JSON is read where it is, not copied") {
	lisp::Interpreter interpreter;
	const lisp::Value view =
	    interpreter.viewJson(std::make_shared<const json>(json::parse(R"({"items": [1, 2, 3], "tags": ["a", "a"]})")));
	const std::vector<lisp::Value> items = {view.lookup("items")};
	const lisp::Value seq = interpreter.call(interpreter.global("seq").value_or(lisp::Value()), items);
	CHECK(seq.isView());
	CHECK(interpreter.print(seq) == "[1 2 3]");
	const lisp::Value rest = interpreter.call(interpreter.global("next").value_or(lisp::Value()), items);
	CHECK(interpreter.print(rest) == "[2 3]");
	const std::vector<lisp::Value> tags = {view.lookup("tags")};
	CHECK(interpreter.print(interpreter.call(interpreter.global("frequencies").value_or(lisp::Value()), tags)) ==
	      "{:a 2}");
}

TEST_CASE("lisp functions: a host's primitive passed to partial, comp or juxt still passes through the gate") {
	lisp::Interpreter interpreter;
	std::vector<std::string> seen;
	interpreter.definePrimitive("effect", {.capability = "control",
	                                       .pure = false,
	                                       .call = [](lisp::Interpreter&, std::span<const lisp::Value> arguments) {
		                                       return arguments.empty() ? lisp::Value() : arguments[0];
	                                       }});
	interpreter.setGate([&](const lisp::CallContext& call) {
		CHECK(call.primitive == "effect");
		seen.emplace_back(call.authors.empty() ? std::string() : std::string(call.authors.front()));
	});
	for(const std::string_view text : {"((partial effect) 1)", "((comp effect inc) 1)", "((juxt effect) 1)",
	                                   "(keep effect [1])", "((fnil effect 0) nil)", "(sort-by effect [2 1])"}) {
		CAPTURE(text);
		const std::size_t before = seen.size();
		static_cast<void>(interpreter.eval(text, "a.lisp", "person"));
		CHECK(seen.size() > before);
		CHECK(seen.back() == "person");
	}
}

TEST_CASE("lisp functions: a program's definition of a name keeps its own, and the language's functions theirs") {
	lisp::Interpreter interpreter;
	static_cast<void>(interpreter.eval("(defn distinct [x] :mine) (defn partition [n xs] :mine)", "a.lisp", "person"));
	CHECK(interpreter.print(interpreter.eval("(distinct [1 1])", "a.lisp", "person")) == ":mine");
	// group-by and sort-by, written in the language, use the language's own map and sort, not the program's
	static_cast<void>(interpreter.eval("(defn map [f xs] :mine) (defn sort [xs] :mine)", "a.lisp", "person"));
	CHECK(interpreter.print(interpreter.eval("(sort-by - [1 2])", "a.lisp", "person")) == "[2 1]");
	CHECK(interpreter.print(interpreter.eval("(group-by str [1 2])", "a.lisp", "person")) == "{\"1\" [1] \"2\" [2]}");
}

TEST_CASE("lisp functions: the work a function does is charged against the budget") {
	lisp::Limits limits;
	limits.budget.operations = 5000;
	lisp::Interpreter interpreter(limits);
	CHECK_THROWS_AS(static_cast<void>(interpreter.eval("(distinct (range 100000))", "a.lisp", "person")),
	                lisp::LispError);
	CHECK_THROWS_AS(static_cast<void>(interpreter.eval("(frequencies (map str (range 100000)))", "a.lisp", "person")),
	                lisp::LispError);
}

TEST_CASE("lisp functions: a long key is charged for each time a map is built with it") {
	// 2,000 times a 64 kB key, made in 16 steps: a few thousand operations, but 128 MB to hash and compare
	const std::string key = "(let [k (loop [k \"k\" n 16] (if (zero? n) k (recur (str k k) (dec n))))] ";
	lisp::Limits limits;
	limits.budget.operations = 100000;
	for(const std::string_view uses : {"(frequencies (repeat 2000 k)))", "(zipmap (repeat 2000 k) (range 2000)))",
	                                   "(group-by (fn [_] k) (range 2000)))", "(select-keys {k 1} (repeat 2000 k)))"}) {
		CAPTURE(uses);
		lisp::Interpreter interpreter(limits);
		CHECK_THROWS_WITH_AS(static_cast<void>(interpreter.eval(key + std::string(uses), "a.lisp", "person")),
		                     doctest::Contains("the budget for work ran out"), lisp::LispError);
	}
}
