/// @file
/// @brief Unit tests for the notation module: the Lisp notation spells exactly the JSON tree (every example file, key
/// order included), is written canonically, reads the edge cases right, and refuses bad text with its line and column
/// within its limits.

#include <affresco/core/Json.hpp>
#include <affresco/notation/Notation.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::notation::Notation;
using affresco::notation::NotationError;

namespace {

/// @brief Writes a value in the Lisp notation.
/// @param value The value.
/// @return Its text.
std::string lisp(const json& value) { return affresco::notation::write(value, Notation::Lisp); }

/// @brief Reads a text in the Lisp notation.
/// @param text The text.
/// @return Its value.
json fromLisp(std::string_view text) { return affresco::notation::read(text, Notation::Lisp); }

/// @brief Reads a file's text.
/// @param file The file.
/// @return Its bytes.
std::string textOf(const std::filesystem::path& file) {
	const std::ifstream in(file, std::ios::binary);
	std::stringstream text;
	text << in.rdbuf();
	return text.str();
}

/// @brief Checks that a value reads back from its Lisp text exactly, key order included, and that writing it again
/// gives the same text.
/// @param value The value.
void roundTrips(const json& value) {
	const std::string text = lisp(value);
	const json back = fromLisp(text);
	CHECK(back.dump() == value.dump()); // dump(): ordered_json's == would also compare, but this shows the difference
	CHECK(lisp(back) == text);
}

/// @brief The error reading a text gives.
/// @param text The text.
/// @return The error, or a NotationError at line 0 if it read.
NotationError errorOf(std::string_view text) {
	try {
		static_cast<void>(fromLisp(text));
	} catch(const NotationError& error) {
		return error;
	}
	return {"", 0, 0, "it read"};
}

} // namespace

TEST_CASE("notation: every example file reads back exactly from its Lisp text, which is canonical") {
	int files = 0;
	for(const auto& entry : std::filesystem::recursive_directory_iterator(AFFRESCOUI_EXAMPLES_DIRECTORY)) {
		if(entry.path().extension() != ".json") continue;
		CAPTURE(entry.path().string());
		roundTrips(affresco::parseUntrusted(textOf(entry.path())));
		++files;
	}
	CHECK(files > 150);
}

TEST_CASE("notation: a node is written with its type as its head, its id after #, its children one per line") {
	const json node = json::parse(R"({"type": "node", "id": "x"})"); // the word node can't be a head: :type node
	CHECK(lisp(node) == "(node :type node #x)\n");
	const json insights = json::parse(R"({"type": "panel", "id": "insights", "color": "transparent", "children": [
		{"id": "title", "style": "caption", "anchors": {"left": 0, "top": 0}, "text": "{{t('burndown.title')}}"},
		{"type": "column", "id": "list", "gap": 4}]})");
	CHECK(lisp(insights) ==
	      "(panel #insights :color transparent\n"
	      "  (node #title :style caption :anchors {:left 0 :top 0} :text \"{{t('burndown.title')}}\")\n"
	      "  (column #list :gap 4))\n");
	roundTrips(insights);
	// Every key keeps its place: an id before the type, a key before the id, children that aren't last
	CHECK(lisp(json::parse(R"({"id": "a", "type": "row"})")) == "(node #a :type row)\n");
	CHECK(lisp(json::parse(R"({"use": "chip", "id": "a"})")) == "(node :use chip #a)\n");
	roundTrips(json::parse(R"({"type": "row", "id": "a", "children": [{"id": "b"}], "gap": 2})"));
	roundTrips(json::parse(R"({"type": "row", "id": "a", "children": "not a list"})"));
	CHECK(lisp(json::parse(R"({"type": "row", "id": "a", "children": []})")) == "(row #a :children [])\n");
	// An object without an id as a string is written with braces, as is a type that isn't a plain word
	CHECK(lisp(json::parse(R"({"id": 3, "type": "row"})")) == "{:id 3 :type row}\n");
	CHECK(lisp(json::parse(R"({"type": "my row", "id": "a"})")) == "(node :type \"my row\" #a)\n");
}

TEST_CASE("notation: strings are bare words or quoted, and read back the same") {
	const std::vector<std::pair<std::string, std::string>> strings = {{"true", R"("true")"},
	                                                                  {"false", R"("false")"},
	                                                                  {"nil", R"("nil")"},
	                                                                  {"node", "node"},
	                                                                  {"", R"("")"},
	                                                                  {"a b", R"("a b")"},
	                                                                  {"1", R"("1")"},
	                                                                  {"-1", R"("-1")"},
	                                                                  {"-", R"("-")"},
	                                                                  {"12px", R"("12px")"},
	                                                                  {":a", R"(":a")"},
	                                                                  {"#a", R"("#a")"},
	                                                                  {"a;b", R"("a;b")"},
	                                                                  {"(a)", R"~("(a)")~"},
	                                                                  {"end\n", R"("end\n")"},
	                                                                  {"\n", R"("\n")"},
	                                                                  {"tab\there", R"("tab\there")"},
	                                                                  {"quote\"s", R"("quote\"s")"},
	                                                                  {"back\\slash", R"("back\\slash")"},
	                                                                  {"x\x01y", R"("x\u0001y")"},
	                                                                  {"caf\u00e9", "\"caf\u00e9\""},
	                                                                  {"\U0001F600", "\"\U0001F600\""},
	                                                                  {"a.b-c_d@e$f", "a.b-c_d@e$f"},
	                                                                  {"@color.accent", "@color.accent"},
	                                                                  {"$t", "$t"}};
	for(const auto& [value, written] : strings) {
		CAPTURE(written);
		CHECK(lisp(value) == written + "\n");
		roundTrips(value);
	}
	// Keys and ids that aren't plain words are quoted too
	const json keys = json::parse(R"({"#root/color": 1, "": 2, "true": 3, "a b": 4, "ok": 5})");
	CHECK(lisp(keys) == "{:\"#root/color\" 1 :\"\" 2 :\"true\" 3 :\"a b\" 4 :ok 5}\n");
	roundTrips(keys);
	const json id = json::parse(R"({"type": "label", "id": "{{'nav-' + tab}}"})");
	CHECK(lisp(id) == "(label #\"{{'nav-' + tab}}\")\n");
	roundTrips(id);
	CHECK(fromLisp("\"\\u00e9\\ud83d\\ude00\"") == "\u00e9\U0001F600"); // escapes, a surrogate pair
}

TEST_CASE("notation: numbers, true, false, nil and the empty values") {
	const json values = json::parse(R"([0, -0, 1, -1, 1.5, -2.25, 1e10, 1E-7, -3.5e+300, 18446744073709551615,
		-9223372036854775808, 0.1, true, false, null, {}, [], [[]], {"a": {}}])");
	roundTrips(values);
	CHECK(lisp(json::parse("[1, -2.5, true, null, {}, []]")) == "[1 -2.5 true nil {} []]\n");
	CHECK(fromLisp("1e2") == json(100.0));
	CHECK(fromLisp("-0.5").get<double>() == -0.5);
	for(const char* scalar : {"\"a\"", "1", "nil", "true", "word"}) roundTrips(fromLisp(scalar));
	// A number that isn't finite, which only a computed value can hold, is nil, as JSON writes it null; never the
	// bare word null, which would read back as a string
	const json computed = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), 1.0};
	CHECK(lisp(computed) == "[nil nil 1.0]\n");
	CHECK(fromLisp(lisp(computed)) == json::parse(computed.dump()));
}

TEST_CASE("notation: long values break over lines, the same way every time") {
	json row = {{"type", "row"}, {"id", "r"}};
	for(int i = 0; i < 30; ++i) row["key" + std::to_string(i)] = i;
	const std::string text = lisp(row);
	CHECK(text.starts_with("(row #r\n  :key0 0\n  :key1 1\n"));
	for(std::size_t start = 0, end = 0; start < text.size(); start = end + 1) {
		end = text.find('\n', start);
		CHECK(end - start <= 120);
	}
	roundTrips(row);
	const json list = json::array({std::string(200, 'a'), std::string(10, 'b')});
	CHECK(lisp(list) == "[" + std::string(200, 'a') + "\n  " + std::string(10, 'b') + "]\n");
}

TEST_CASE("notation: comments and spaces are only between values") {
	CHECK(fromLisp("; a screen\n(label #a ; its id\n  :text \"a ; not a comment\") ; done\n") ==
	      json::parse(R"({"type": "label", "id": "a", "text": "a ; not a comment"})"));
	CHECK(fromLisp("\xEF\xBB\xBF[1\t2\r\n3]") == json::array({1, 2, 3})); // a byte order mark, tabs and CRLF
	CHECK(fromLisp("(row #a(label #b)[1])") == json::parse(R"({"type":"row","id":"a","children":[{"type":"label",
		"id":"b"},[1]]})"));
}

TEST_CASE("notation: a problem is reported at its line and column") {
	const NotationError open = errorOf("(row #a\n  (label #b :text hi)\n");
	CHECK(open.line() == 3);
	CHECK(open.column() == 1);
	CHECK(open.message() == "expected ')' to close the node opened at line 1, column 1");
	CHECK(std::string(open.what()) ==
	      "AffrescoUI screen: line 3, column 1: expected ')' to close the node opened at line 1, column 1");

	const NotationError unicode = errorOf("[\"\u65e5\u672c\" 12px]");
	CHECK(unicode.line() == 1);
	CHECK(unicode.column() == 7); // characters, not bytes
	CHECK(unicode.message().starts_with("unexpected '12px'"));

	CHECK(errorOf("\"never ends").message() == "a string that never ends: expected '\"'");
	CHECK(errorOf("{:a 1 :a 2}").message() == "the key \"a\" twice: a key has one value");
	CHECK(errorOf("(row #a #b)").message() == "the key \"id\" twice: a key has one value");
	CHECK(errorOf("(row #a (label #b) :gap 2)").message() == "':gap' after the node's children: properties come first");
	CHECK(errorOf("(row :children [] (label #b))").message() ==
	      "a node with :children and children after its properties");
	CHECK(errorOf("{:a}").message() == "expected a value, found '}'");
	CHECK(errorOf("{a 1}").message() == "expected a key (:name) or '}', found 'a'");
	CHECK(errorOf(":a 1").message() == "a key (':a') outside an object or a node");
	CHECK(errorOf("#a").message() == "an id ('#a') outside a node");
	CHECK(errorOf("(1 #a)").message() == "expected the node's type, or node, after '(', found '1'");
	CHECK(errorOf("(true)").message().starts_with("expected the node's type"));
	CHECK(errorOf("()").message().starts_with("expected the node's type"));
	CHECK(errorOf("{:1 2}").message().starts_with("expected a word or a string in quotes after ':'"));
	CHECK(errorOf("").message() == "expected a value, found the end of the text");
	CHECK(errorOf("1 2").message() == "expected the end of the text after the value, found '2'");
	CHECK(errorOf("]").message() == "expected a value, found ']'");
	CHECK(errorOf("\"a\"b").message() == "expected a space or a bracket after the string's closing quote");
	CHECK(errorOf("\"\\x\"").message().starts_with("a string in quotes that JSON can't read"));
	CHECK(errorOf("\"\x01\"").message().starts_with("a string in quotes that JSON can't read"));
	CHECK(errorOf("\"\xFF\"").message().starts_with("a string in quotes that JSON can't read"));
	CHECK(errorOf("01").message().starts_with("unexpected '01'"));
	CHECK(errorOf("1.").message().starts_with("unexpected '1.'"));
	CHECK(errorOf("caf\xC3\xA9").message().starts_with("unexpected")); // a bare word is ASCII
	CHECK(errorOf("1e400").message() == "a number too large: '1e400'");
	CHECK(errorOf(std::string(41, '1')).message() == "a number longer than 40 characters");
	// What an error quotes is short, and cut between characters: a key or a word can be as long as the text
	const std::string key(100000, 'k');
	CHECK(errorOf("{:" + key + " 1 :" + key + " 2}").message() ==
	      "the key \"" + std::string(40, 'k') + "...\" twice: a key has one value");
	const NotationError cut = errorOf("[" + std::string(39, 'a') + "\u00e9" + std::string(10, 'b') + "]");
	CHECK(cut.message() == "unexpected '" + std::string(39, 'a') +
	                           "...': not a word, a number, true, false or nil (a "
	                           "string with other characters goes in quotes)");
}

TEST_CASE("notation: nesting stops at maxNesting, counted as JSON counts it") {
	const auto nested = [](std::size_t levels) { return std::string(levels, '[') + std::string(levels, ']'); };
	CHECK(affresco::nestingDepth(fromLisp(nested(static_cast<std::size_t>(affresco::maxNesting)))) ==
	      affresco::maxNesting);
	CHECK(errorOf(nested(static_cast<std::size_t>(affresco::maxNesting) + 1)).message() ==
	      "nested more than 256 levels deep");
	CHECK(errorOf(nested(100000)).message() == "nested more than 256 levels deep"); // no recursion that deep
	// A node's children are a list inside it: two levels, as in JSON
	std::string nodes;
	for(int i = 0; i < affresco::maxNesting / 2; ++i) nodes += "(row #r" + std::to_string(i) + " ";
	CHECK(affresco::nestingDepth(fromLisp(nodes + "1" + std::string(affresco::maxNesting / 2, ')'))) ==
	      affresco::maxNesting);
	CHECK(errorOf(nodes + "(row #last 1)" + std::string(affresco::maxNesting / 2, ')')).message() ==
	      "nested more than 256 levels deep");
	// A value deeper than that is never written
	json deep = json::array();
	for(int i = 0; i < affresco::maxNesting; ++i) deep = json::array({deep});
	CHECK_THROWS_AS(static_cast<void>(lisp(deep)), affresco::DocumentError);
}

TEST_CASE("notation: garbage is refused, never crashes") {
	for(const char* text :
	    {"(",         ")",   "[",    "{",    "#",       ":",          "#\"",  ":\"",      "\\", "(row",       "(row #",
	     "(row #a :", "{:a", "[1 2", "\"\\", "\"\\u12", "(node #a (", "\x00", "\x80\x81", ";",  "  \n ; only"}) {
		CAPTURE(text);
		CHECK_THROWS_AS(static_cast<void>(fromLisp(text)), NotationError);
	}
	CHECK_THROWS_AS(static_cast<void>(fromLisp(std::string(affresco::notation::maxLispBytes + 1, ' '))), NotationError);
}

TEST_CASE("notation: the notation of a file is its extension, and JSON reads as before") {
	using affresco::notation::notationOf;
	CHECK(notationOf("board.screen.lisp") == Notation::Lisp);
	CHECK(notationOf("parts/BOARD.LISP") == Notation::Lisp);
	CHECK(notationOf("board.screen.json") == Notation::Json);
	CHECK(notationOf("board") == Notation::Json);
	CHECK(affresco::notation::notationNamed("lisp") == Notation::Lisp);
	CHECK(affresco::notation::notationNamed("json") == Notation::Json);
	CHECK_FALSE(affresco::notation::notationNamed("yaml"));
	CHECK(affresco::notation::nameOf(Notation::Lisp) == "lisp");

	// JSON goes through parseUntrusted(), errors and all; its text is written compactly
	CHECK_THROWS_AS(static_cast<void>(affresco::notation::read("{", Notation::Json)), json::parse_error);
	CHECK_THROWS_AS(static_cast<void>(affresco::notation::readDocument("{", "a.json")), json::parse_error);
	CHECK(affresco::notation::write(json::parse(R"({"b": 1, "a": [true]})"), Notation::Json) ==
	      R"({"b":1,"a":[true]})");
	CHECK(affresco::notation::jsonText("{ \"kept\" : 1 }", "a.screen.json") == "{ \"kept\" : 1 }");
	CHECK(affresco::notation::jsonText("(label #a)", "a.screen.lisp") == R"({"type":"label","id":"a"})");

	// A file's error names the file
	try {
		static_cast<void>(affresco::notation::readDocument("\n  (label", "parts/a.lisp"));
		FAIL("it read");
	} catch(const NotationError& error) {
		CHECK(std::string(error.what()) ==
		      "AffrescoUI screen: parts/a.lisp: line 2, column 9: expected ')' to close the node opened at line 2, "
		      "column 3");
	}
}
