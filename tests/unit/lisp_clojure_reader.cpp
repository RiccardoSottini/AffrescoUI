/// @file
/// @brief Unit tests for the Clojure forms the Lisp language reads (modules/lisp/reference/reader.md): function
/// literals, reader conditionals, metadata, characters, Clojure's numbers, the refusals and their messages, and the
/// forms the compiler takes on from Clojure: functions of many arities, catch with a class, finally, :strs, and repeat.
#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>
#include <affresco/notation/Notation.hpp>
#include <doctest/doctest.h>

#include <string>
#include <string_view>
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
/// @return The error, or an empty one if it ran.
lisp::LispError errorOf(std::string_view text) {
	try {
		lisp::Interpreter interpreter;
		static_cast<void>(interpreter.eval(text, "test", "tester"));
	} catch(const lisp::LispError& error) {
		return error;
	}
	return {"", 0, 0, "", "it ran"};
}

/// @brief What a binding's form builds in a program: `(defscreen main (node :x ~form))`'s `x`.
/// @param form The form.
/// @return The value.
json binding(const std::string& form) {
	return lisp::buildText("(defscreen main (node :x ~" + form + "))", "test.lisp")["root"]["x"];
}

/// @brief The forms of a text, as text.
/// @param text The text.
/// @return Each form's text.
std::vector<std::string> formTexts(std::string_view text) {
	std::vector<std::string> texts;
	for(const lisp::FormAt& form : lisp::formsOf(text, "test.lisp")) texts.push_back(form.text);
	return texts;
}

} // namespace

TEST_CASE("lisp clojure: a function literal is a function of %, %2 ... and %&") {
	CHECK(run("(map #(+ % 1) [1 2])") == "[2 3]");
	CHECK(run("(#(+ %1 %2) 1 2)") == "3");
	CHECK(run("(#(list %3) 1 2 3)") == "(3)"); // %1 and %2 are parameters still
	CHECK(run("(#(count %&) 1 2 3)") == "3");
	CHECK(run("(#(list % %&) 1 2 3)") == "(1 [2 3])");
	CHECK(run("(#(+ %.a 1) {:a 2})") == "3");
	CHECK(run("(quote #(%map %2))") == "(fn [% %2] (%map %2))"); // %map is a word of its own
	CHECK(run("(#(str \"a\") )") == "\"a\"");
	CHECK(errorOf("#(+ #(+ % 1) 1)").message() ==
	      "a #(...) inside another: name the inner function's parameter with (fn [x] ...)");
	CHECK(errorOf("#(+ %21)").message() == "a #(...) takes at most 20 parameters");
	CHECK(errorOf("#(+ %0)").message() == "a #(...) names its parameters %, %1 to %20, and %&");
	CHECK(errorOf("#(+ %0)").column() == 5);
}

TEST_CASE("lisp clojure: a function literal in a binding builds the notation's item function") {
	CHECK(binding("(count (filter #(> %.price 10) state.items))") ==
	      "{{length(filter(state.items, (it.price > 10)))}}");
}

TEST_CASE("lisp clojure: a reader conditional chooses :affresco or :default, the first of them") {
	CHECK(run("#?(:clj 1 :default 2)") == "2");
	CHECK(run("#?(:affresco 1 :default 2)") == "1");
	CHECK(run("#?(:default 2 :affresco 1)") == "2");
	CHECK(run("[1 #?@(:default [2 3]) 4]") == "[1 2 3 4]");
	CHECK(run("(list #?(:clj 1) 2)") == "(2)");
	CHECK(run("#?(:clj 1) 7") == "7"); // chooses nothing: the text goes on
	CHECK(run("{:a 1 #?(:default :b) 2}") == "{:a 1 :b 2}");
	CHECK(run("(#?(:default #(+ %2 1) :clj #(%5)) 1 2)") == "3"); // a branch not chosen adds no parameter
	CHECK(errorOf("#?@(:default [1])").message() ==
	      "#?@ splices its items into the list or the vector around it, and here there is none");
	CHECK(errorOf("[#?@(:default 1)]").message() == "#?@ splices the items of a vector, and chose a number");
	CHECK(errorOf("#?(1 2)").message() == "a reader conditional holds pairs of a platform's keyword and a form, "
	                                      "such as :affresco 1 :default 2, and found '1'");
	CHECK(errorOf("#?(:a)").message() == "the keyword ':a' has no form after it");
}

TEST_CASE("lisp clojure: metadata is read and dropped, and is part of its form's text") {
	CHECK(run("^:private [1 2]") == "[1 2]");
	CHECK(run("(+ 1 ^{:a 1} 2)") == "3");
	CHECK(run("^:a ^:b 3") == "3");
	CHECK(run("[#_ ^:x 5 6]") == "[6]"); // the metadata goes with the form #_ discards
	CHECK(errorOf("[1 ^]").message() == "metadata (^) stands before a form, and found ']'");
	CHECK(errorOf("[1 ^:a]").message() == "metadata (^) stands before a form, and found ']'");
	CHECK(formTexts("^:private (defn f [] 1) #(+ % 1) #?(:default 2)") ==
	      std::vector<std::string>{"^:private (defn f [] 1)", "#(+ % 1)", "#?(:default 2)"});
}

TEST_CASE("lisp clojure: a character is a string of it") {
	CHECK(run("\\a") == "\"a\"");
	CHECK(run("[\\( \\) \\space \\newline \\tab]") == "[\"(\" \")\" \" \" \"\\n\" \"\\t\"]");
	CHECK(run("\\u0041") == "\"A\"");
	CHECK(run("\\\xC3\xA9") == "\"\xC3\xA9\"");
	CHECK(errorOf("\\foo").message() ==
	      "unknown character '\\foo': write one character after the backslash, \\newline, \\space, \\tab, \\return, "
	      "\\formfeed, \\backspace or \\u and four hexadecimal digits");
}

TEST_CASE("lisp clojure: Clojure's numbers read as the nearest JSON number") {
	CHECK(run("12N") == "12");
	CHECK(run("18446744073709551615N") == "18446744073709551615");
	CHECK(run("1.5M") == "1.5");
	CHECK(run("1/2") == "0.5");
	CHECK(run("4/2") == "2");
	CHECK(run("-1/2") == "-0.5");
	CHECK(run("0x1F") == "31");
	CHECK(run("-0x10") == "-16");
	CHECK(errorOf("99999999999999999999N").message() == "the number '99999999999999999999N' doesn't fit 64 bits");
	CHECK(errorOf("1/0").message() == "the ratio '1/0' divides by zero");
}

TEST_CASE("lisp clojure: what has no JSON or AffrescoUI meaning is refused with a way to write it") {
	CHECK(errorOf("##Inf").message() == "JSON has no infinity or not-a-number");
	CHECK(errorOf("[##NaN]").message() == "JSON has no infinity or not-a-number");
	CHECK(errorOf("#'foo").message() == "there are no vars: name the function itself");
	CHECK(errorOf("::foo").message() == "a keyword has no namespace here: write :name, or :\"a/b\"");
	CHECK(errorOf(":a/b").message() == "a keyword has no namespace here: write :name, or :\"a/b\"");
	CHECK(run(":\"a/b\"") == ":a/b");
	CHECK(errorOf("#{1 2}").message() == "a set literal #{...} isn't in the language: write a vector");
}

TEST_CASE("lisp clojure attack: deep or long runs of the new forms end with an error at a place") {
	std::string nested;
	for(int i = 0; i < 300; ++i) nested += "#(";
	nested += "%";
	for(int i = 0; i < 300; ++i) nested += ")";
	const lisp::LispError lambda = errorOf(nested);
	CHECK(lambda.line() == 1);
	CHECK(lambda.column() == 3);
	const lisp::LispError metadata = errorOf(std::string(100000, '^') + "1");
	CHECK(metadata.message().starts_with("expected a form after the ^ at"));
	std::string conditionals;
	for(int i = 0; i < 100000; ++i) conditionals += "#?(:clj 1) ";
	CHECK(run(conditionals + "1") == "1");
}

TEST_CASE("lisp clojure: the notation's reader reads none of the language's forms") {
	CHECK_THROWS(static_cast<void>(affresco::notation::read("(node :x #(1))", affresco::notation::Notation::Lisp)));
	CHECK_THROWS(static_cast<void>(affresco::notation::read("(node :x ^:a 1)", affresco::notation::Notation::Lisp)));
	CHECK_THROWS(static_cast<void>(affresco::notation::read("(node :x \\a)", affresco::notation::Notation::Lisp)));
}
