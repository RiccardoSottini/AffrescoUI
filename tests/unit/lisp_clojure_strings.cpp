/// @file
/// @brief Unit tests for the language's text functions under Clojure's names (modules/lisp/reference/strings.md):
/// `clojure.string` through its three spellings, `format`, errors as values (`ex-info`, `ex-message`, `ex-data`) and
/// the `parse-` functions; and that a long text goes through `split`, `replace` and `join` in time linear in it,
/// within the budget or with the budget's error.

#include "Helpers.hpp"

#include <affresco/lisp/Interpreter.hpp>

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

namespace lisp = affresco::lisp;

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

/// @brief Tells whether a text's error says something.
/// @param text The text.
/// @param says What it should say.
/// @return true if it does.
bool failsSaying(std::string_view text, std::string_view says) { return errorOf(text).contains(says); }

/// What `split`, `replace` and `replace-first` say when given a regular expression.
constexpr std::string_view regularExpression = "a regular expression (#\"...\") isn't in this language";

/// @brief Calls a function of the language on texts, outside any program, timing it.
/// @param interpreter The interpreter.
/// @param name The function's name.
/// @param texts Its arguments, strings.
/// @return How long it took, or a negative time if it failed with the budget's error.
double secondsFor(lisp::Interpreter& interpreter, std::string_view name, const std::vector<std::string>& texts) {
	std::vector<lisp::Value> arguments;
	arguments.reserve(texts.size());
	for(const std::string& text : texts) arguments.push_back(interpreter.makeString(text));
	const auto started = std::chrono::steady_clock::now();
	try {
		static_cast<void>(interpreter.call(interpreter.global(name).value_or(lisp::Value()), arguments));
	} catch(const lisp::LispError& error) {
		CHECK(error.message().contains("budget"));
		return -std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
	}
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
}

/// @brief The most memory the process has held so far, where the system says (0 where it doesn't).
/// @return It, in bytes.
std::size_t peakMemory() {
#if defined(__unix__) || defined(__APPLE__)
	rusage usage{};
	getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
	return static_cast<std::size_t>(usage.ru_maxrss); // bytes
#else
	return static_cast<std::size_t>(usage.ru_maxrss) * 1024; // kilobytes
#endif
#else
	return 0;
#endif
}

} // namespace

TEST_CASE("lisp clojure.string: every name answers to str/, string/ and clojure.string/") {
	constexpr auto names = std::to_array<std::string_view>(
	    {"blank?", "capitalize", "ends-with?", "includes?", "index-of", "join", "last-index-of", "lower-case",
		 "replace", "replace-first", "reverse", "split", "split-lines", "starts-with?", "trim", "trim-newline", "triml",
		 "trimr", "upper-case"});
	for(const std::string_view name : names) {
		CAPTURE(name);
		std::string same = "(= str/";
		same.append(name).append(" string/").append(name).append(" clojure.string/").append(name).append(")");
		CHECK(run(same) == "true");
	}
	CHECK(run("(str/trim \"  a \")") == "\"a\"");
	CHECK(run("(string/trim \"  a \")") == "\"a\"");
	CHECK(run("(clojure.string/trim \"  a \")") == "\"a\"");
	CHECK(run("(= str/join join)") == "true"); // the same function as the bare one
}

TEST_CASE("lisp clojure.string: the functions made on first use are the same as those made at the start") {
	lisp::Interpreter interpreter;
	REQUIRE(interpreter.global("clojure.string/trim")); // a host looking one up before any program uses it
	REQUIRE(interpreter.global("parse-long"));
	for(int run = 0; run < 3; ++run) { // a run made each first, and the next ones use what it made
		CHECK(interpreter.print(interpreter.eval("(str/split \"a,b\" \",\")", "test", "tester")) == "[\"a\" \"b\"]");
		CHECK(interpreter.print(interpreter.eval("(format \"%d\" 7)", "test", "tester")) == "\"7\"");
		CHECK(interpreter.print(interpreter.eval("(str/index-of \"abc\" \"c\")", "test", "tester")) == "2");
	}
	CHECK(run("(defn format [x] (str \"mine \" x)) (format 1)") == "\"mine 1\""); // a program's, as before
	CHECK(run("(= parse-long parse-long)") == "true");
	CHECK(run("(fn? str/blank?)") == "true");
}

TEST_CASE("lisp clojure.string: blank? and capitalize") {
	CHECK(run("(str/blank? \"  \")") == "true");
	CHECK(run("(str/blank? nil)") == "true");
	CHECK(run("(str/blank? \"\")") == "true");
	CHECK(run("(str/blank? \" \\t\\n\\r\")") == "true");
	CHECK(run("(str/blank? \" a \")") == "false");
	CHECK(run("(str/blank? \"é\")") == "false");
	CHECK(failsSaying("(str/blank? 3)", "\"str/blank?\" takes a string or nil"));
	CHECK(run("(str/capitalize \"hELLO world\")") == "\"Hello world\"");
	CHECK(run("(str/capitalize \"\")") == "\"\"");
	CHECK(run("(str/capitalize \"éTÉx\")") == "\"étÉx\""); // ASCII letters only, as upper-case and lower-case
	CHECK(failsSaying("(str/capitalize nil)", "\"str/capitalize\" takes a string"));
}

TEST_CASE("lisp clojure.string: starts-with?, ends-with? and includes?") {
	CHECK(run("(str/starts-with? \"hello\" \"he\")") == "true");
	CHECK(run("(str/starts-with? \"hello\" \"\")") == "true");
	CHECK(run("(str/starts-with? \"he\" \"hello\")") == "false"); // a part longer than the text
	CHECK(run("(str/ends-with? \"héllo\" \"llo\")") == "true");
	CHECK(run("(str/ends-with? \"\" \"a\")") == "false");
	CHECK(run("(str/includes? \"a→b\" \"→\")") == "true");
	CHECK(run("(str/includes? \"abc\" \"\")") == "true");
	CHECK(run("(str/includes? \"abc\" \"abcd\")") == "false");
	CHECK(run("(str/includes? \"aaab\" \"aab\")") == "true"); // a part whose start repeats
	CHECK(failsSaying("(str/starts-with? nil \"a\")", "\"str/starts-with?\" takes a string and a part"));
	CHECK(failsSaying("(str/ends-with? \"a\")", "\"str/ends-with?\" takes a string and a part"));
	CHECK(failsSaying("(str/includes? \"a\" 1)", "\"str/includes?\" takes a string and a part"));
}

TEST_CASE("lisp clojure.string: index-of and last-index-of count characters") {
	CHECK(run("(str/index-of \"abcabc\" \"c\")") == "2");
	CHECK(run("(str/index-of \"abcabc\" \"c\" 3)") == "5");
	CHECK(run("(str/index-of \"abcabc\" \"c\" -4)") == "2");
	CHECK(run("(str/index-of \"abc\" \"x\")") == "nil");
	CHECK(run("(str/index-of \"ab\" \"abc\")") == "nil");
	CHECK(run("(str/index-of \"ab\" \"b\" 9)") == "nil");
	CHECK(run("(str/index-of \"ab\" \"\")") == "0");
	CHECK(run("(str/index-of \"é→x\" \"x\")") == "2");
	CHECK(run("(str/last-index-of \"abcabc\" \"c\")") == "5");
	CHECK(run("(str/last-index-of \"abcabc\" \"c\" 4)") == "2");
	CHECK(run("(str/last-index-of \"aaa\" \"aa\")") == "1"); // overlapping places count
	CHECK(run("(str/last-index-of \"abc\" \"c\" -1)") == "nil");
	CHECK(run("(str/last-index-of \"é→x→\" \"→\")") == "3");
	CHECK(run("(str/last-index-of \"abc\" \"\")") == "3");
	CHECK(run("(str/last-index-of \"\" \"a\")") == "nil");
	CHECK(failsSaying("(str/index-of \"a\" \"a\" \"0\")", "\"str/index-of\" takes a string, a part and maybe"));
	CHECK(failsSaying("(str/last-index-of nil \"a\")", "\"str/last-index-of\" takes a string, a part and maybe"));
}

TEST_CASE("lisp clojure.string: join, with a separator and without") {
	CHECK(run("(str/join \", \" [1 2])") == "\"1, 2\"");
	CHECK(run("(str/join [1 \"a\" nil :k])") == "\"1a:k\""); // as str shows each
	CHECK(run("(join [\"a\" \"b\"])") == "\"ab\"");          // the bare one takes one argument too
	CHECK(run("(join \"-\" [\"a\" \"b\"])") == "\"a-b\"");
	CHECK(run("(str/join \", \" nil)") == "\"\"");
	CHECK(run("(str/join [])") == "\"\"");
	CHECK(run("(str/join \"·\" [\"é\" \"→\"])") == "\"é·→\"");
	CHECK(failsSaying("(str/join 1 [1])", "\"join\" takes a list of items, and maybe a separator before it"));
	CHECK(failsSaying("(join)", "\"join\" takes a list of items"));
}

TEST_CASE("lisp clojure.string: lower-case and upper-case are the language's") {
	CHECK(run("(str/upper-case \"a\")") == "\"A\"");
	CHECK(run("(str/lower-case \"ÀB\")") == "\"Àb\"");
	CHECK(run("(str/upper-case \"\")") == "\"\"");
	CHECK(failsSaying("(str/upper-case nil)", "changing case takes a string"));
}

TEST_CASE("lisp clojure.string: replace and replace-first") {
	CHECK(run("(str/replace \"a-b-c\" \"-\" \"+\")") == "\"a+b+c\"");
	CHECK(run("(str/replace-first \"a-b-c\" \"-\" \"+\")") == "\"a+b-c\"");
	CHECK(run("(str/replace \"aaaa\" \"aa\" \"b\")") == "\"bb\""); // matches don't overlap
	CHECK(run("(str/replace \"abc\" \"abcd\" \"x\")") == "\"abc\"");
	CHECK(run("(str/replace \"\" \"a\" \"x\")") == "\"\"");
	CHECK(run("(str/replace \"é→é\" \"é\" \"e\")") == "\"e→e\"");
	CHECK(run("(str/replace \"ab\" \"\" \"-\")") == "\"-a-b-\""); // before every character and at the end
	CHECK(run("(str/replace-first \"ab\" \"\" \"-\")") == "\"-ab\"");
	CHECK(run("(str/replace \"é\" \"\" \"-\")") == "\"-é-\"");
	CHECK(run("(str/replace \"\" \"\" \"-\")") == "\"-\"");
	CHECK(run("(str/replace-first \"\" \"\" \"-\")") == "\"-\"");
	CHECK(failsSaying("(str/replace \"a\" \"a\")", "\"str/replace\" takes a string, the text to match"));
	CHECK(failsSaying("(str/replace-first nil \"a\" \"b\")", "\"str/replace-first\" takes a string"));
	CHECK(failsSaying("(str/replace \"a b\" #\" \" \"_\")", regularExpression));
	CHECK(failsSaying("(str/replace-first \"a b\" #\" \" \"_\")", regularExpression));
}

TEST_CASE("lisp clojure.string: split, as Clojure's with a string for the separator") {
	CHECK(run("(str/split \"a,b,,c,,\" \",\")") == "[\"a\" \"b\" \"\" \"c\"]");
	CHECK(run("(str/split \"a,b,c\" \",\" 2)") == "[\"a\" \"b,c\"]");
	CHECK(run("(str/split \"a,b,,\" \",\" -1)") == "[\"a\" \"b\" \"\" \"\"]");
	CHECK(run("(str/split \",a\" \",\")") == "[\"\" \"a\"]");
	CHECK(run("(str/split \",,\" \",\")") == "[]");
	CHECK(run("(str/split \"\" \",\")") == "[\"\"]");
	CHECK(run("(str/split \"abc\" \"abcd\")") == "[\"abc\"]");
	CHECK(run("(str/split \"é→x\" \"\")") == "[\"é\" \"→\" \"x\"]"); // an empty separator gives the characters
	CHECK(run("(str/split \"abc\" \"\" 2)") == "[\"a\" \"bc\"]");
	CHECK(run("(str/split \"a::b\" \"::\")") == "[\"a\" \"b\"]");
	CHECK(failsSaying("(str/split \"a b\" #\" \")", regularExpression));
	CHECK(failsSaying("(str/split \"a b\" #\" \" 2)", regularExpression));
	CHECK(failsSaying("(str/split \"a\" 1)", "\"str/split\" takes a string, a separator (a string) and maybe"));
	CHECK(failsSaying("(str/split \"a\" \",\" \"2\")", "\"str/split\" takes a string, a separator"));
}

TEST_CASE("lisp clojure.string: split-lines") {
	CHECK(run("(str/split-lines \"a\\nb\\r\\nc\")") == "[\"a\" \"b\" \"c\"]");
	CHECK(run("(str/split-lines \"a\\n\\n\")") == "[\"a\"]");
	CHECK(run("(str/split-lines \"a\\r\")") == "[\"a\\r\"]"); // a '\r' alone isn't a line break
	CHECK(run("(str/split-lines \"\")") == "[\"\"]");
	CHECK(run("(str/split-lines \"é\\n→\")") == "[\"é\" \"→\"]");
	CHECK(failsSaying("(str/split-lines nil)", "\"str/split-lines\" takes a string"));
}

TEST_CASE("lisp clojure.string: trim, triml, trimr, trim-newline and reverse") {
	CHECK(run("(str/trim \" \\t a b \\n\")") == "\"a b\"");
	CHECK(run("(str/trim \"   \")") == "\"\"");
	CHECK(run("(str/trim \"\")") == "\"\"");
	CHECK(run("(str/triml \"  é \")") == "\"é \"");
	CHECK(run("(str/trimr \"  é \")") == "\"  é\"");
	CHECK(run("(str/trim-newline \"a \\r\\n\\n\")") == "\"a \"");
	CHECK(run("(str/trim-newline \"\\n\")") == "\"\"");
	CHECK(run("(str/reverse \"abc\")") == "\"cba\"");
	CHECK(run("(str/reverse \"é→x\")") == "\"x→é\"");
	CHECK(run("(str/reverse \"\")") == "\"\"");
	CHECK(run("(reverse [1 2])") == "[2 1]"); // the bare reverse stays the sequence's
	CHECK(failsSaying("(str/trim nil)", "\"str/trim\" takes a string"));
	CHECK(failsSaying("(str/triml 1)", "\"str/triml\" takes a string"));
	CHECK(failsSaying("(str/trimr)", "\"str/trimr\" takes a string"));
	CHECK(failsSaying("(str/trim-newline :a)", "\"str/trim-newline\" takes a string"));
	CHECK(failsSaying("(str/reverse [1])", "\"str/reverse\" takes a string"));
}

TEST_CASE("lisp format: its directives, flags and widths") {
	CHECK(run("(format \"%d of %s (%.1f%%)\" 3 \"ten\" 30.0)") == "\"3 of ten (30.0%)\"");
	CHECK(run("(format \"%s has %d items\" \"cart\" 2)") == "\"cart has 2 items\"");
	CHECK(run("(format \"[%5d|%-5d|%05d|%-4s|%4s]\" 42 42 -42 \"é\" \"→\")") == "\"[   42|42   |-0042|é   |   →]\"");
	CHECK(run("(format \"%f %.0f %.3f\" 1.5 2.5 -0.25)") == "\"1.500000 2 -0.250\"");
	CHECK(run("(format \"%x %x %04x\" 255 -1 10)") == "\"ff ffffffffffffffff 000a\"");
	CHECK(run("(format \"a%nb\")") == "\"a\\nb\"");
	CHECK(run("(format \"%s|%s|%s\" nil [1 2] :k)") == "\"|[1 2]|:k\""); // %s as str shows it
	CHECK(run("(format \"%.2s\" \"é→x\")") == "\"é→\"");
	CHECK(run("(format \"%d\" 4.0)") == "\"4\""); // a whole number, as an index takes one
	CHECK(run("(format \"\")") == "\"\"");
	CHECK(run("(format \"%s\" \"a\" \"extra\")") == "\"a\"");
	CHECK(failsSaying("(format \"%q\" 1)", "format doesn't have %q: it has %s %d %f %x %%"));
	CHECK(failsSaying("(format \"%é\" 1)", "format doesn't have %é: it has"));
	CHECK(failsSaying("(format \"%d and %d\" 1)", "format's %d has no value"));
	CHECK(failsSaying("(format \"%5s\")", "format's %5s has no value"));
	CHECK(failsSaying("(format \"%d\" \"1\")", "format's %d takes a whole number, given a string"));
	CHECK(failsSaying("(format \"%d\" 1.5)", "format's %d takes a whole number"));
	CHECK(failsSaying("(format \"%.1f\" nil)", "format's %.1f takes a number"));
	CHECK(failsSaying("(format \"%1001d\" 1)", "format takes a width or a precision up to 1,000"));
	CHECK(failsSaying("(format \"%.99999999999999999999f\" 1)", "up to 1,000"));
	CHECK(failsSaying("(format \"%05s\" \"a\")", "the 0 flag is for numbers"));
	CHECK(failsSaying("(format \"%.2d\" 1)", "a precision is for %f and %s"));
	CHECK(failsSaying("(format \"50%\")", "ends inside the directive"));
	CHECK(failsSaying("(format 1)", "\"format\" takes a pattern (a string)"));
	CHECK(run("(count (format \"%1000d\" 1))") == "1000");
}

TEST_CASE("lisp format: a long result is charged to the budget") {
	lisp::Interpreter interpreter(lisp::Limits{.budget = {.operations = 10000000, .bytes = 1U << 20U}});
	std::vector<std::string> arguments = {""};
	for(int at = 0; at < 2000; ++at) {
		arguments.front() += "%1000s";
		arguments.emplace_back("a");
	}
	CHECK(secondsFor(interpreter, "format", arguments) < 0.0); // 2 MB of padding: the budget's error
}

TEST_CASE("lisp format: what its directives show is charged as it grows, not only when it is done") {
	// 400,000 directives of 1,300 bytes each grew 520 MB, and twice that in RSS, before a charge at the end stopped
	// them
	lisp::Interpreter interpreter(lisp::Limits{.budget = {.operations = 100000000, .bytes = 32U << 20U}});
	const std::size_t before = peakMemory();
	std::string error = "it ran";
	try {
		static_cast<void>(interpreter.eval(
		    "(apply format (apply str (repeat 400000 \"%.1000f\")) (repeat 400000 1e300))", "test", "tester"));
	} catch(const lisp::LispError& caught) {
		error = caught.message();
	}
	CHECK(error.contains("the budget for allocation ran out"));
	CHECK(peakMemory() - before < std::size_t{256} << 20U);
}

TEST_CASE("lisp errors as values: ex-info, ex-message and ex-data") {
	CHECK(run("(try (throw (ex-info \"no\" {:code 3})) (catch e (:code (ex-data e))))") == "3");
	CHECK(run("(try (throw (ex-info \"no\" {:code 3})) (catch e (ex-message e)))") == "\"no\"");
	CHECK(run("(ex-info \"no\" {})") == "{:message \"no\" :data {}}");
	CHECK(run("(try (/ 1 0) (catch e (ex-message e)))") == "\"dividing by zero\"");
	CHECK(run("(try (/ 1 0) (catch e (ex-data e)))") == "nil");
	CHECK(run("(try (throw \"plain\") (catch e (ex-message e)))") == "\"plain\"");
	CHECK(run("(ex-message 3)") == "nil");
	CHECK(run("(ex-message {})") == "nil");
	CHECK(run("(ex-data \"text\")") == "nil");
	CHECK(run("(ex-message \"é\")") == "\"é\"");
	CHECK(failsSaying("(ex-info \"no\" nil)", "\"ex-info\" takes a message (a string) and data (a map)"));
	CHECK(failsSaying("(ex-info \"no\")", "\"ex-info\" takes a message"));
	CHECK(failsSaying("(ex-message)", "\"ex-message\" takes one value"));
	CHECK(failsSaying("(ex-data 1 2)", "\"ex-data\" takes one value"));
}

TEST_CASE("lisp parse-long, parse-double and parse-boolean") {
	CHECK(run("(parse-long \"12\")") == "12");
	CHECK(run("(parse-long \"-12\")") == "-12");
	CHECK(run("(parse-long \"+7\")") == "7");
	CHECK(run("(parse-long \"9223372036854775807\")") == "9223372036854775807");
	CHECK(run("(parse-long \"-9223372036854775808\")") == "-9223372036854775808");
	CHECK(run("(parse-long \"9223372036854775808\")") == "nil");
	CHECK(run("(parse-long \"\")") == "nil");
	CHECK(run("(parse-long \"1.5\")") == "nil");
	CHECK(run("(parse-long \" 1\")") == "nil");
	CHECK(run("(parse-long \"١\")") == "nil"); // digits of another script aren't read
	CHECK(run("(parse-long \"00000000000000000000000000000000000000001\")") == "nil"); // longer than 40
	CHECK(failsSaying("(parse-long 12)", "\"parse-long\" takes a string"));
	CHECK(failsSaying("(parse-long nil)", "\"parse-long\" takes a string"));
	CHECK(run("(parse-double \"1.5\")") == "1.5");
	CHECK(run("(parse-double \"-2e3\")") == "-2000.0");
	CHECK(run("(parse-double \" .5 \")") == "0.5");
	CHECK(run("(parse-double \"5.\")") == "5.0");
	CHECK(run("(parse-double \"3\")") == "3.0");
	CHECK(run("(parse-double \"1.5d\")") == "1.5");
	CHECK(run("(parse-double \"1e-400\")") == "0.0");
	CHECK(run("(parse-double \"1e400\")") == "nil");
	CHECK(run("(parse-double \"NaN\")") == "nil");
	CHECK(run("(parse-double \"Infinity\")") == "nil");
	CHECK(run("(parse-double \".\")") == "nil");
	CHECK(run("(parse-double \"1e\")") == "nil");
	CHECK(run("(parse-double \"\")") == "nil");
	CHECK(run("(parse-double \"0x1F\")") == "nil");
	CHECK(failsSaying("(parse-double 1.5)", "\"parse-double\" takes a string"));
	CHECK(run("(parse-boolean \"true\")") == "true");
	CHECK(run("(parse-boolean \"false\")") == "false");
	CHECK(run("(parse-boolean \"True\")") == "nil");
	CHECK(run("(parse-boolean \"\")") == "nil");
	CHECK(failsSaying("(parse-boolean true)", "\"parse-boolean\" takes a string"));
}

// Linear time is a growth: four times the text takes less than eight times as long, where a naive search, which
// compares a part a sixteenth of the text long at every place, takes sixteen. A ratio, not a time, so a loaded or
// instrumented machine passes as an idle one does
TEST_CASE("lisp clojure.string: a 1 MiB text goes through split, replace and join in linear time") {
	constexpr std::size_t mebibyte = 1U << 20U;
	const auto as = [](std::size_t size) { return std::string(size, 'a'); };
	const auto part = [](std::size_t size) { return std::string(size / 16U, 'a') + "b"; };
	const auto commas = [](std::size_t size) {
		std::string text;
		for(std::size_t at = 0; at < size / 2U; ++at) text += "a,";
		return text;
	};
	using Arguments = std::function<std::vector<std::string>(std::size_t)>;
	const std::vector<std::pair<std::string, Arguments>> calls = {
	    {"clojure.string/split", [&](std::size_t size) { return std::vector{as(size), part(size)}; }},
	    {"clojure.string/replace", [&](std::size_t size) { return std::vector<std::string>{as(size), part(size), "x"}; }},
	    {"clojure.string/includes?", [&](std::size_t size) { return std::vector{as(size), part(size)}; }},
	    {"clojure.string/last-index-of", [&](std::size_t size) { return std::vector<std::string>{as(size), "aa"}; }},
	    {"clojure.string/split", [&](std::size_t size) { return std::vector<std::string>{commas(size), ","}; }},
	    {"clojure.string/split", [&](std::size_t size) { return std::vector<std::string>{as(size), ""}; }}};
	for(const auto& [name, arguments] : calls) {
		CAPTURE(name);
		CAPTURE(arguments(16).back());
		const double growth = affresco::test::growthOfFour(
		    [&](std::size_t size) {
			    const std::vector<std::string> texts = arguments(size);
			    lisp::Interpreter interpreter;
			    return std::abs(secondsFor(interpreter, name, texts));
		    },
		    mebibyte);
		CAPTURE(growth);
		CHECK(growth < 8.0);
	}
	lisp::Interpreter growing; // a long replacement at every match: the budget's error, before the text is made
	CHECK(secondsFor(growing, "clojure.string/replace", {as(mebibyte), "a", std::string(1024, 'x')}) < 0.0);
	// A long separator between many pieces, more than the budget at either size: the budget's error, as the text grows,
	// and so no more work for more pieces than for fewer
	const double joins = affresco::test::growthOfFour(
	    [](std::size_t pieces) {
		    lisp::Interpreter joining;
		    std::string items;
		    for(std::size_t at = 0; at < pieces; ++at) items += "ab,";
		    const std::array<lisp::Value, 2> toSplit = {joining.makeString(items), joining.makeString(",")};
		    const lisp::Value join = joining.global("clojure.string/join").value_or(lisp::Value());
		    const lisp::Value split = joining.global("clojure.string/split").value_or(lisp::Value());
		    const std::array<lisp::Value, 2> arguments = {joining.makeString(std::string(4096, '-')),
		                                                  joining.call(split, toSplit)};
		    bool refused = false;
		    const double seconds = affresco::test::secondsOf([&] {
			    try {
				    static_cast<void>(joining.call(join, arguments));
			    } catch(const lisp::LispError& error) {
				    refused = error.message().contains("budget");
			    }
		    });
		    CHECK(refused);
		    return seconds;
	    },
	    65536U);
	CAPTURE(joins);
	CHECK(joins < 8.0);
}

TEST_CASE("lisp clojure.string: the bare names an app's expressions have stay theirs") {
	for(const std::string_view name : {"split", "trim", "replace-first", "includes?", "blank?", "capitalize"}) {
		CAPTURE(name);
		CHECK(errorOf("(" + std::string(name) + " \"a\" \"b\")").contains("is not defined"));
	}
}
