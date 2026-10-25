/// @file
/// @brief Unit tests for the "languages" section (format/src/languages): reading a language and its limits, finding
/// it by name, alias or file, and the tokenizer on the languages shipped in examples/shared/languages, whose scopes
/// must give each kind of text the colour the editors gave it before (keywords, types, strings, numbers, comments,
/// headings).

#include "Helpers.hpp"
#include "Language.hpp"
#include "LanguageTokenizer.hpp"

#include <affresco/core/Json.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using affresco::DocumentError;
using affresco::json;
using affresco::format::Language;
using affresco::format::LanguageCatalog;
using affresco::format::LanguageState;
using affresco::format::LanguageTokenizer;

namespace {

/// @brief A shipped language file's catalog.
/// @param file Its name in examples/shared/languages.
/// @return Its languages.
std::shared_ptr<const LanguageCatalog> shipped(const std::string& file) {
	const std::ifstream in(std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "shared" / "languages" / file);
	std::stringstream text;
	text << in.rdbuf();
	return LanguageCatalog::read(json::parse(text.str())["languages"]);
}

/// @brief The letter of a scope's colour: k keyword, t type, s string, n number, c comment, h heading, '.' none.
/// @param scope The scope.
char letterOf(const std::string& scope) {
	const std::string base = scope.substr(0, scope.find('.'));
	if(base == "keyword") return 'k';
	if(base == "type") return 't';
	if(base == "string") return 's';
	if(base == "number") return 'n';
	if(base == "comment") return 'c';
	return base == "heading" ? 'h' : '.';
}

/// @brief Lines tokenized one after another, each byte as the letter of its innermost coloured scope.
/// @param language The language.
/// @param lines The lines.
/// @return One string of letters per line.
std::vector<std::string> paint(const std::shared_ptr<const Language>& language, const std::vector<std::string>& lines) {
	LanguageTokenizer tokenizer(language);
	LanguageState state;
	std::vector<std::string> painted;
	for(const std::string& line : lines) {
		const auto tokenized = tokenizer.tokenize(state, line);
		std::string letters(line.size(), '.');
		for(const auto& token : tokenized.tokens) {
			char letter = '.';
			for(auto scope = token.scopes.rbegin(); scope != token.scopes.rend() && letter == '.'; ++scope)
				letter = letterOf(language->scopes[*scope - 1]);
			for(std::uint32_t i = token.from; i < token.to && i < letters.size(); ++i) letters[i] = letter;
		}
		painted.push_back(letters);
		state = tokenized.end;
	}
	return painted;
}

/// @brief The letter a text of a line is painted in, '?' if it isn't one letter, '!' if the line lacks it.
/// @param lines The lines.
/// @param painted Their letters (paint()).
/// @param line The line.
/// @param text The text, its first place on the line.
char styleOf(const std::vector<std::string>& lines, const std::vector<std::string>& painted, std::size_t line,
             const std::string& text) {
	const std::size_t at = lines[line].find(text);
	if(at == std::string::npos) return '!';
	const std::string letters = painted[line].substr(at, text.size());
	return letters.find_first_not_of(letters.front()) == std::string::npos ? letters.front() : '?';
}

/// @brief Checks the letters texts of lines are painted in.
/// @param language The language.
/// @param lines The lines.
/// @param expected (line, text, letter) for each text.
void expectStyles(const std::shared_ptr<const Language>& language, const std::vector<std::string>& lines,
                  const std::vector<std::tuple<std::size_t, std::string, char>>& expected) {
	REQUIRE(language);
	const std::vector<std::string> painted = paint(language, lines);
	for(const auto& [line, text, letter] : expected) {
		INFO("line ", line, ": ", lines[line], " / ", text);
		CHECK(styleOf(lines, painted, line, text) == letter);
	}
}

/// @brief A catalog of one language.
/// @param language Its declaration.
std::shared_ptr<const LanguageCatalog> one(const char* language) {
	return LanguageCatalog::read(json::parse(std::string(R"({"x": )") + language + "}"));
}

} // namespace

TEST_CASE("languages: found by name, alias in any case, or a file's name or path") {
	const auto catalog = shipped("cmake.json");
	CHECK(catalog->find("cmake"));
	CHECK(catalog->find("CMake"));
	CHECK(catalog->find("src/CMakeLists.txt"));
	CHECK(catalog->find("tools/Find.CMAKE"));
	CHECK_FALSE(catalog->find("main.cpp"));
	CHECK_FALSE(catalog->find(""));
	CHECK(shipped("python.json")->find("app/main.py")->title == "Python");
	CHECK(shipped("typescript.json")->find("TS"));
}

TEST_CASE("languages: every shipped file reads, and includes them all") {
	for(const char* file : {"cpp.json", "javascript.json", "typescript.json", "python.json", "json.json",
	                        "cmake.json", "markdown.json", "shell.json"}) {
		INFO(file);
		CHECK(shipped(file)->languages().size() == 1);
	}
}

TEST_CASE("languages: a wrong language is an error naming its place") {
	CHECK_THROWS_WITH_AS(one(R"({"colour": 1})"), doctest::Contains("languages.x.colour"), DocumentError);
	CHECK_THROWS_WITH_AS(one(R"({"patterns": [{"match": "(", "scope": "a"}]})"),
	                     doctest::Contains("languages.x.patterns[0].match"), DocumentError);
	CHECK_THROWS_WITH_AS(one(R"({"patterns": [{"include": "#nowhere"}]})"), doctest::Contains("nowhere"),
	                     DocumentError);
	CHECK_THROWS_AS(one(R"j({"pairs": [["((", ")"]]})j"), DocumentError);
	CHECK_THROWS_AS(one(R"({"comments": {"block": ["/*"]}})"), DocumentError);
	CHECK_THROWS_AS(one(R"({"patterns": [{"begin": "a"}]})"), DocumentError); // a begin without its end
}

TEST_CASE("languages: the limits hold: rules, nesting, and the size of an expression") {
	json rules = json::array();
	for(std::size_t i = 0; i <= Language::maxRules; ++i) rules.push_back({{"match", "a"}, {"scope", "s"}});
	CHECK_THROWS_AS((void)LanguageCatalog::read({{"x", {{"patterns", rules}}}}), DocumentError);

	json nested = {{"match", "a"}};
	for(std::size_t i = 0; i <= Language::maxNesting; ++i) nested = {{"patterns", json::array({nested})}};
	CHECK_THROWS_AS((void)LanguageCatalog::read({{"x", {{"patterns", json::array({nested})}}}}), DocumentError);

	CHECK_THROWS_AS((void)LanguageCatalog::read({{"x", {{"wordPattern", std::string(1 << 20, 'a')}}}}), DocumentError);
	json many = json::object();
	for(std::size_t i = 0; i <= LanguageCatalog::maxLanguages; ++i) many["l" + std::to_string(i)] = json::object();
	CHECK_THROWS_AS((void)LanguageCatalog::read(many), DocumentError);
}

TEST_CASE("languages: a line that backtracks without end, matches nothing, or opens regions without end stops") {
	const auto catastrophic = one(R"({"patterns": [{"match": "(a+)+$", "scope": "keyword"}]})")->find("x");
	const auto started = std::chrono::steady_clock::now();
	LanguageTokenizer tokenizer(catastrophic);
	static_cast<void>(tokenizer.tokenize({}, std::string(5000, 'a') + "b"));
	const std::chrono::duration<double> spent = std::chrono::steady_clock::now() - started;
	CHECK(spent.count() < affresco::test::timeLimit(1.0));

	const auto empty = one(R"({"patterns": [{"match": "(?=a)|", "scope": "keyword"}]})")->find("x");
	LanguageTokenizer emptyTokenizer(empty);
	static_cast<void>(emptyTokenizer.tokenize({}, std::string(1000, 'a')));

	const auto deep =
	    one(R"j({"patterns": [{"begin": "\\(", "end": "\\)", "scope": "s", "patterns": [{"include": "$self"}]}]})j");
	LanguageTokenizer deepTokenizer(deep->find("x"));
	const auto line = deepTokenizer.tokenize({}, std::string(500, '('));
	CHECK(line.end.size() <= LanguageTokenizer::maxDepth);
	CHECK(deepTokenizer.tokenize(line.end, std::string(500, ')')).end.empty());
}

TEST_CASE("languages: C++ is coloured as before: directives, keywords, types, strings, numbers, comments") {
	const std::vector<std::string> lines = {"#include <vector>",
	                                        "int main() { return 0x1F; } // done",
	                                        "auto s = std::string(\"a\\\"b\") + Foo::bar('c');",
	                                        "/* one",
	                                        "two */ const double half = 0.5;",
	                                        "auto raw = R\"x(a)\" b",
	                                        "c)x\";"};
	expectStyles(shipped("cpp.json")->find("cpp"), lines,
	             {{0, "#include", 'k'},     {0, "<vector>", 's'},  {1, "int", 't'},     {1, "main", '.'},
	              {1, "return", 'k'},       {1, "0x1F", 'n'},      {1, "// done", 'c'}, {2, "std", 't'},
	              {2, "\"a\\\"b\"", 's'},   {2, "Foo", 't'},       {2, "'c'", 's'},     {3, "/* one", 'c'},
	              {4, "two */", 'c'},       {4, "const", 'k'},     {4, "double", 't'},  {4, "0.5", 'n'},
	              {5, "R\"x(a)\" b", 's'}, {6, "c)x\"", 's'},     {6, ";", '.'}});
}

TEST_CASE("languages: JSON keys are types, values strings, numbers and keywords") {
	const std::vector<std::string> lines = {R"({"name": "x", "size": -1.5e3, "on": true, "off": null})"};
	expectStyles(shipped("json.json")->find("data.json"), lines,
	             {{0, R"("name")", 't'}, {0, R"("x")", 's'}, {0, "-1.5e3", 'n'}, {0, "true", 'k'}, {0, "null", 'k'}});
}

TEST_CASE("languages: CMake commands are keywords, options and variables types") {
	const std::vector<std::string> lines = {"add_library(core STATIC ${SOURCES} \"a b\") # done",
	                                        "cmake_minimum_required(VERSION 3.20)"};
	expectStyles(shipped("cmake.json")->find("CMakeLists.txt"), lines,
	             {{0, "add_library", 'k'},
	              {0, "core", '.'},
	              {0, "STATIC", 't'},
	              {0, "${SOURCES}", 't'},
	              {0, "\"a b\"", 's'},
	              {0, "# done", 'c'},
	              {1, "VERSION", 't'},
	              {1, "3.20", 'n'}});
}

TEST_CASE("languages: Markdown headings, bullets, code spans and code blocks") {
	const std::vector<std::string> lines = {"# Title", "- item with `code`", "    indented code", "```", "x = 1", "```",
	                                        "after"};
	expectStyles(shipped("markdown.json")->find("README.md"), lines,
	             {{0, "# Title", 'h'},
	              {1, "-", 'k'},
	              {1, "item", '.'},
	              {1, "`code`", 's'},
	              {2, "indented code", 's'},
	              {4, "x = 1", 's'},
	              {6, "after", '.'}});
}

TEST_CASE("languages: Python, JavaScript, TypeScript and shell") {
	expectStyles(shipped("python.json")->find("py"),
	             {"class Shape(Base):", "    def area(self): return 3.14 # pi", "    doc = \"\"\"one", "two\"\"\" + 'x'"},
	             {{0, "class", 'k'},
	              {0, "Shape", 't'},
	              {1, "def", 'k'},
	              {1, "3.14", 'n'},
	              {1, "# pi", 'c'},
	              {2, "\"\"\"one", 's'},
	              {3, "two\"\"\"", 's'},
	              {3, "'x'", 's'}});
	expectStyles(shipped("javascript.json")->find("app.js"),
	             {"const total = `sum ${count + 1}`; // done", "await Promise.all(list);"},
	             {{0, "const", 'k'}, {0, "`sum ", 's'}, {0, "1", 'n'}, {0, "// done", 'c'}, {1, "Promise", 't'}});
	expectStyles(shipped("typescript.json")->find("view.tsx"), {"interface Size { width: number }"},
	             {{0, "interface", 'k'}, {0, "Size", 't'}, {0, "number", 't'}});
	expectStyles(shipped("shell.json")->find("build.sh"), {"if [ -n \"$HOME\" ]; then echo $1 'a'; fi # done"},
	             {{0, "if", 'k'},
	              {0, "then", 'k'},
	              {0, "\"", 's'}, // the quote; the variable inside it is a variable
	              {0, "$HOME", 't'},
	              {0, "$1", 't'},
	              {0, "'a'", 's'},
	              {0, "# done", 'c'}});
}
