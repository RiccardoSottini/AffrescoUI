/// @file
/// @brief Unit tests for the language's library and its seams (modules/lisp, README.md "The library"): entries compiled
/// on first use and blind to a program's definitions, a library macro's guard, qualified names, hints for names the
/// language doesn't have, the reader's forms after '#', and that no name of the language is a host's step or
/// expression function but for today's overlaps.

#include "../../modules/lisp/src/Library.hpp"
#include "../../modules/lisp/src/Machine.hpp"
#include "ExpressionFunctions.hpp"
#include "StepRegistry.hpp"

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>
#include <affresco/notation/Words.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lisp = affresco::lisp;

namespace {

/// @brief Evaluates a text and prints the result.
/// @param interpreter The interpreter.
/// @param text The text.
/// @return The printed value.
std::string run(lisp::Interpreter& interpreter, std::string_view text) {
	return interpreter.print(interpreter.eval(text, "test", "tester"));
}

/// @brief Evaluates a text in a new interpreter and prints the result.
/// @param text The text.
/// @return The printed value.
std::string run(std::string_view text) {
	lisp::Interpreter interpreter;
	return run(interpreter, text);
}

/// @brief The error a text gives when it is evaluated.
/// @param interpreter The interpreter.
/// @param text The text.
/// @return Its message, or "it ran".
std::string errorOf(lisp::Interpreter& interpreter, std::string_view text) {
	try {
		static_cast<void>(interpreter.eval(text, "test", "tester"));
	} catch(const lisp::LispError& error) {
		return error.message();
	}
	return "it ran";
}

/// @brief The error a text gives when it is evaluated in a new interpreter.
/// @param text The text.
/// @return Its message, or "it ran".
std::string errorOf(std::string_view text) {
	lisp::Interpreter interpreter;
	return errorOf(interpreter, text);
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

/// A macro of the language only these tests define: its expansion calls `not` by word.
constexpr auto testMacro = std::to_array<lisp::LibraryEntry>({
    {.name = "test-unless",
	 .source = "(defmacro test-unless [test & body] `(if (not ~test) (do ~@body)))",
	 .macro = true,
	 .uses = "not"},
});

/// @brief The words of the language a host's step is called by (format's Programs::defineSteps()).
/// @return Them.
std::set<std::string, std::less<>> stepWords() {
	// The steps a program says with the language's own forms, and the two the language has another word for
	constexpr auto unspoken = std::to_array<std::string_view>({"if", "forEach", "while", "stop", "note", "lisp"});
	std::set<std::string, std::less<>> words;
	for(const affresco::format::StepGroup& group : affresco::format::builtInSteps()->groups())
		for(const affresco::format::StepType& type : group.steps) {
			if(std::ranges::find(unspoken, type.name) != unspoken.end()) continue;
			words.insert(type.name == "remove"   ? std::string("remove-state")
			             : type.name == "update" ? std::string("update-record")
			                                     : std::string(type.name));
		}
	return words;
}

/// @brief The names of the expressions' functions a host's program calls (format's Programs::defineFunctions()).
/// @return Them.
std::set<std::string, std::less<>> expressionFunctions() {
	std::set<std::string, std::less<>> names;
	for(const affresco::format::detail::Function& function : affresco::format::detail::functions())
		if(function.apply && function.group == affresco::format::detail::Group::Functions && function.name != "lisp")
			names.emplace(function.name);
	return names;
}

/// @brief Every word on the notation's lists of the language's words.
/// @return Them.
std::vector<std::string_view> languageWordsOnLists() {
	std::vector<std::string_view> words;
	for(const std::span<const std::string_view> list : affresco::notation::languageWordLists)
		words.insert(words.end(), list.begin(), list.end());
	return words;
}

} // namespace

TEST_CASE("lisp library: an entry is compiled the first time something uses it") {
	lisp::Interpreter interpreter;
	const lisp::Global* map = interpreter.machine().findGlobal("map");
	REQUIRE(map != nullptr);
	CHECK(map->library);
	CHECK(map->takesFunctions);
	CHECK(map->state == lisp::Global::State::Deferred);
	CHECK(run(interpreter, "(map inc [1 2])") == "[2 3]");
	CHECK(map->state == lisp::Global::State::Ready);
	// An entry that uses another compiles it first, and a value of the language passed around is the entry
	CHECK(run(interpreter, "(mapcat (fn [x] [x x]) [1 2])") == "[1 1 2 2]");
	CHECK(run(interpreter, "(let [f update-in] (f {:a {:b 1}} [:a :b] inc))") == "{:a {:b 2}}");
}

TEST_CASE("lisp library: a program's definitions never change the language's own functions") {
	// `last` uses `count`, `remove` uses `filter` and `not`, `mapcat` uses `map` and `concat`
	CHECK(run("(defn count [x] 0) (last [1 2 3])") == "3");
	CHECK(run("(defn not [x] x) (defn filter [f xs] []) (remove (fn [x] (> x 1)) [1 2 3])") == "[1]");
	CHECK(run("(defn map [f xs] []) (mapcat (fn [x] [x x]) [1 2])") == "[1 1 2 2]");
	// Defined first, used later, compiled only then: still the language's
	lisp::Interpreter interpreter;
	static_cast<void>(run(interpreter, "(defn concat [& xs] :mine)"));
	CHECK(run(interpreter, "(mapcat (fn [x] [x]) [1 2])") == "[1 2]");
	// A program defining a name of the library has its own, where the program calls it
	CHECK(run("(defn mapcat [f xs] :mine) (mapcat inc [1])") == ":mine");
	CHECK(run("(defn second [xs] :mine) (second [1 2])") == ":mine");
}

TEST_CASE("lisp library: a macro of the language refuses a local or a definition hiding a word it calls") {
	lisp::Interpreter interpreter;
	lisp::defineLibrary(interpreter.machine(), testMacro);
	CHECK(run(interpreter, "(test-unless false 1)") == "1");
	CHECK(run(interpreter, "(test-unless true 1)") == "nil");
	CHECK(errorOf(interpreter, "(let [not identity] (test-unless false 1))") ==
	      "\"test-unless\" uses the language's \"not\", which a local hides here: rename the local");
	CHECK(errorOf(interpreter, "(defn not [x] x) (test-unless false 1)") ==
	      "\"test-unless\" uses the language's \"not\", which this program defines: name yours otherwise");
}

TEST_CASE("lisp library: qualified names") {
	CHECK(run("(str/upper-case \"a\")") == "\"A\"");
	CHECK(run("(string/lower-case \"A\")") == "\"a\"");
	CHECK(run("(clojure.string/join \", \" [\"a\" \"b\"])") == "\"a, b\"");
	CHECK(run("(clojure.core/inc 1)") == "2");
	CHECK(run("(map str/upper-case [\"a\" \"b\"])") == "[\"A\" \"B\"]");
	CHECK(run("(let [inc dec] (clojure.core/inc 1))") == "2"); // a qualified name is never a local
	CHECK(run("(clojure.core/when true 1)") == "1");           // a form, by its qualified name
	CHECK(run("(/ 6 3)") == "2");                              // division is no namespace
	CHECK(
	    errorOf("(foo/bar 1)") ==
	    "\"foo/bar\": no namespace \"foo\" here; the language has clojure.core, clojure.string (str/) and clojure.set "
	    "(set/)");
	CHECK(errorOf("(str/nothing \"a\")") == "\"clojure.string/nothing\" is not defined");
	CHECK(errorOf("clojure.core/if") == "\"clojure.core/if\" is a form of the language, not a value");
}

TEST_CASE("lisp library: a host adds a namespace and an alias for it") {
	lisp::Interpreter interpreter;
	interpreter.addNamespace("app.text", false);
	interpreter.addAlias("t", "app.text");
	interpreter.definePrimitive("app.text/shout",
	                            {.pure = true, .call = [](lisp::Interpreter& in, std::span<const lisp::Value>) {
		                             return in.makeString("HEY");
	                             }});
	CHECK(run(interpreter, "(t/shout)") == "\"HEY\"");
	CHECK(run(interpreter, "(app.text/shout)") == "\"HEY\"");
	CHECK_THROWS_AS(interpreter.addAlias("u", "app.none"), std::invalid_argument);
	interpreter.addNamespace("app.core", true); // bare: its functions are the globals of their own names
	CHECK(run(interpreter, "(app.core/inc 1)") == "2");
}

TEST_CASE("lisp library: a name another dialect has says what to write instead") {
	CHECK(errorOf("(atom 1)").starts_with("\"atom\" is not defined: the language has no mutable state"));
	CHECK(errorOf("(println 1)").starts_with("\"println\" is not defined: the language prints nothing"));
	CHECK(errorOf("(rand-int 3)").starts_with("\"rand-int\" is not defined: the language has no randomness"));
	CHECK(errorOf("(require 'x)").starts_with("\"require\" is not defined: a program loads another file"));
	CHECK(errorOf("(iterate inc 0)").starts_with("\"iterate\" is not defined: the language has no lazy"));
	CHECK(errorOf("(re-find 1 2)").starts_with("\"re-find\" is not defined: the language has no regular"));
	CHECK(errorOf("(nothing-here 1)") == "\"nothing-here\" is not defined");
	for(const std::string_view name : {"atom", "declare", "set", "swap!", "volatile!"}) {
		CAPTURE(name);
		CHECK_FALSE(lisp::hintFor(name).empty());
	}
}

TEST_CASE("lisp library: the forms after '#' are read, and say what the language has instead") {
	CHECK(run("(#(+ % 1) 1)") == "2"); // more in lisp_clojure_reader.cpp
	CHECK(run("#{1 2}") == "#{1 2}");  // more in lisp_clojure_sets.cpp
	CHECK(run("[#?(:clj 1) #?@(:default [2])]") == "[2]");
	// Discarded whole, as the form they start
	CHECK(run("[#_ #(a b) 3]") == "[3]");
	CHECK(run("[#_ #{a b} 3 #_ #?(:clj 1) 4]") == "[3 4]");
	CHECK(lisp::formsOf("#_ #(a b) c").size() == 1);
	CHECK(lisp::formsOf("#(a b) c").size() == 2);
}

TEST_CASE("lisp library: no name of the language is a step's word or an expression function, but today's") {
	// The overlaps there are, all expression functions written natively: in a program the language's wins
	// (Programs::defineFunctions()). A new one would also compile a library entry whenever a host starts
	const std::set<std::string_view> overlaps = {"abs",   "concat", "join",  "keys",   "max",
	                                             "merge", "min",    "range", "reverse"};
	const std::set<std::string, std::less<>> steps = stepWords();
	const std::set<std::string, std::less<>> functions = expressionFunctions();
	REQUIRE(steps.contains("set"));
	REQUIRE(functions.contains("clamp"));
	const auto words = languageWordsOnLists();
	for(const std::string_view word : words) {
		CAPTURE(word);
		if(overlaps.contains(word)) continue;
		CHECK_FALSE(steps.contains(word));
		CHECK_FALSE(functions.contains(word));
	}
	for(const std::string_view word : overlaps) { // each still is one, or it comes off the list
		CAPTURE(word);
		CHECK(std::ranges::find(words, word) != words.end());
		CHECK((steps.contains(word) || functions.contains(word)));
	}
}

TEST_CASE("lisp library: every Lisp file of the examples and the tests still reads") {
	int files = 0;
	for(const char* directory : {AFFRESCOUI_EXAMPLES_DIRECTORY, AFFRESCOUI_EXAMPLES_DIRECTORY "/../tests"})
		for(const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
			if(entry.path().extension() != ".lisp") continue;
			const std::string text = textOf(entry.path());
			CAPTURE(entry.path().string());
			if(lisp::isProgram(text)) CHECK_NOTHROW(static_cast<void>(lisp::formsOf(text, entry.path().string())));
			else CHECK_NOTHROW(static_cast<void>(lisp::buildText(text, entry.path().string())));
			++files;
		}
	CHECK(files > 300);
}
