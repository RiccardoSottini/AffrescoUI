/// @file
/// @brief The syntax engine (affresco/services/Syntax.hpp, modules/services/src/Syntax*): each grammar's highlights,
/// folds and brackets; a text kept in step with its edits, highlighted as a fresh parse would and changed only where
/// the edits reach; its limits; and the `syntax` provider.

#include <affresco/services/Syntax.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace services = affresco::services;
using services::SyntaxDocument;
using services::SyntaxPoint;
using services::SyntaxSpan;

namespace {

/// @brief A text and its document, edited together.
struct Text {
	std::string bytes;                        ///< The text.
	std::unique_ptr<SyntaxDocument> document; ///< Its document.

	/// @brief A text in a language, parsed.
	/// @param language The language.
	/// @param text The text.
	Text(std::string_view language, std::string text)
	    : bytes(std::move(text)), document(SyntaxDocument::create(language)) {
		REQUIRE(document);
		REQUIRE(document->parse(reader(), bytes.size()) == services::SyntaxParse::Parsed);
	}

	/// @brief Reads the text from an offset to its end.
	[[nodiscard]] services::SyntaxReader reader() const {
		return [this](std::size_t byte, SyntaxPoint) { return std::string_view(bytes).substr(byte); };
	}

	/// @brief The point of an offset.
	/// @param byte The offset.
	[[nodiscard]] SyntaxPoint pointOf(std::size_t byte) const {
		SyntaxPoint at;
		for(std::size_t i = 0; i < byte; ++i)
			at = bytes[i] == '\n' ? SyntaxPoint{at.row + 1, 0} : SyntaxPoint{at.row, at.column + 1};
		return at;
	}

	/// @brief Replaces bytes, tells the document and parses again.
	/// @param offset Where.
	/// @param removed How many bytes go.
	/// @param inserted What comes in.
	/// @param changed Gets the rows the parse changed.
	void replace(std::size_t offset, std::size_t removed, std::string_view inserted,
	             std::vector<services::SyntaxRows>* changed = nullptr) {
		services::SyntaxEdit edit;
		edit.startByte = static_cast<std::uint32_t>(offset);
		edit.oldEndByte = static_cast<std::uint32_t>(offset + removed);
		edit.newEndByte = static_cast<std::uint32_t>(offset + inserted.size());
		edit.start = pointOf(offset);
		edit.oldEnd = pointOf(offset + removed);
		bytes.replace(offset, removed, inserted);
		edit.newEnd = pointOf(offset + inserted.size());
		document->edit(edit);
		REQUIRE(document->parse(reader(), bytes.size(), changed) == services::SyntaxParse::Parsed);
	}

	/// @brief The number of lines.
	[[nodiscard]] std::uint32_t rows() const { return static_cast<std::uint32_t>(std::ranges::count(bytes, '\n')) + 1; }

	/// @brief Every line's runs.
	[[nodiscard]] std::vector<SyntaxSpan> spans() const {
		std::vector<SyntaxSpan> out;
		CHECK(document->highlight(reader(), 0, rows() - 1, out));
		return out;
	}

	/// @brief The style names of the runs covering a byte of a line, outer first.
	/// @param row The line.
	/// @param column The byte.
	[[nodiscard]] std::vector<std::string> stylesAt(std::uint32_t row, std::uint32_t column) const {
		std::vector<SyntaxSpan> out;
		document->highlight(reader(), row, row, out);
		std::vector<std::string> names;
		for(const SyntaxSpan& span : out)
			if(span.start <= column && column < span.end) names.push_back(document->styles()[span.style]);
		return names;
	}

	/// @brief The innermost style at a byte of a line ("" if none).
	/// @param row The line.
	/// @param column The byte.
	[[nodiscard]] std::string styleAt(std::uint32_t row, std::uint32_t column) const {
		const std::vector<std::string> names = stylesAt(row, column);
		return names.empty() ? std::string() : names.back();
	}
};

/// @brief Compares runs.
/// @param a One.
/// @param b The other.
bool sameSpans(const std::vector<SyntaxSpan>& a, const std::vector<SyntaxSpan>& b) {
	return std::ranges::equal(a, b, [](const SyntaxSpan& x, const SyntaxSpan& y) {
		return x.row == y.row && x.start == y.start && x.end == y.end && x.style == y.style;
	});
}

/// @brief The runs of one line.
/// @param spans All runs.
/// @param row The line.
std::vector<SyntaxSpan> spansOf(const std::vector<SyntaxSpan>& spans, std::uint32_t row) {
	std::vector<SyntaxSpan> out;
	for(const SyntaxSpan& span : spans)
		if(span.row == row) out.push_back(span);
	return out;
}

const std::string cppSample = R"(#include <vector>
// A comment
namespace demo {
struct Point {
	int x = 0;
	int y = 0;
};
int sum(const std::vector<int>& values) {
	int total = 0;
	for(int value : values) {
		total += value;
	}
	return total; /* done */
}
const char* name = "demo";
} // namespace demo
)";

const std::string pythonSample = R"(import os

class Greeter:
    def greet(self, name):
        if name:
            return "hello " + name
        return None

VALUE = 42
)";

} // namespace

TEST_SUITE("syntax") {

	TEST_CASE("the languages, their names and extensions") {
		const std::vector<std::string> languages = SyntaxDocument::languages();
		CHECK(languages ==
		      std::vector<std::string>{"c", "cpp", "json", "python", "javascript", "typescript", "markdown"});
		CHECK(SyntaxDocument::canonical("C++") == "cpp");
		CHECK(SyntaxDocument::canonical("js") == "javascript");
		CHECK(SyntaxDocument::canonical("md") == "markdown");
		CHECK(SyntaxDocument::canonical("cobol").empty());
		CHECK(SyntaxDocument::languageFor("src/main.cpp") == "cpp");
		CHECK(SyntaxDocument::languageFor("a/b.PY") == "python");
		CHECK(SyntaxDocument::languageFor("README.md") == "markdown");
		CHECK(SyntaxDocument::languageFor("Makefile").empty());
		CHECK(SyntaxDocument::create("cobol") == nullptr);
		for(const std::string& language : languages) {
			CAPTURE(language);
			const auto document = SyntaxDocument::create(language);
			REQUIRE(document);
			CHECK(document->language() == language);
			CHECK_FALSE(document->styles().empty()); // the highlight query compiled
		}
	}

	TEST_CASE("C++: highlights, folds and brackets") {
		const Text text("cpp", cppSample);
		CHECK(text.document->tree().starts_with("(translation_unit"));
		CHECK(text.styleAt(0, 1) == "keyword"); // #include
		CHECK(text.styleAt(0, 10) == "string"); // <vector>
		CHECK(text.styleAt(1, 3) == "comment");
		CHECK(text.styleAt(2, 0) == "keyword");  // namespace
		CHECK(text.styleAt(3, 7) == "type");     // Point
		CHECK(text.styleAt(4, 5) == "property"); // x
		CHECK(text.styleAt(4, 9) == "number");
		CHECK(text.styleAt(7, 4) == "function"); // sum
		CHECK(text.styleAt(12, 1) == "keyword"); // return
		CHECK(text.styleAt(14, 20) == "string");
		// A block folds up to the line before its closing brace
		const auto body = text.document->fold(7);
		REQUIRE(body);
		CHECK(body->last == 12);
		const auto loop = text.document->fold(9);
		REQUIRE(loop);
		CHECK(loop->last == 10);
		CHECK_FALSE(text.document->fold(8));
		std::vector<services::SyntaxFold> folds;
		text.document->folds(0, text.rows() - 1, folds);
		CHECK(folds.size() >= 4);
		CHECK(std::ranges::is_sorted(folds, {}, &services::SyntaxFold::first));
		// Brackets pair from either end
		const auto pair = text.document->brackets({7, 40});
		REQUIRE(pair);
		CHECK(pair->open == SyntaxPoint{7, 40});
		CHECK(pair->close == SyntaxPoint{13, 0});
		const auto back = text.document->brackets({13, 0});
		REQUIRE(back);
		CHECK(back->open == SyntaxPoint{7, 40});
		const auto call = text.document->brackets({7, 7});
		REQUIRE(call);
		CHECK(call->close == SyntaxPoint{7, 38});
		CHECK_FALSE(text.document->brackets({4, 1}));
	}

	TEST_CASE("C: a macro name is a constant (#match?)") {
		const Text text("c", "#define LIMIT 4\nint f(void) { return LIMIT; }\n");
		CHECK(text.styleAt(0, 0) == "keyword");
		CHECK(text.styleAt(1, 21) == "constant");
		CHECK(text.styleAt(1, 14) == "keyword");
	}

	TEST_CASE("JSON: keys, strings, numbers and folds") {
		const Text text("json", "{\n  \"name\": \"demo\",\n  \"list\": [\n    1,\n    true\n  ]\n}\n");
		CHECK(text.styleAt(1, 3) == "string.special.key");
		CHECK(text.styleAt(1, 11) == "string");
		CHECK(text.styleAt(3, 4) == "number");
		CHECK(text.styleAt(4, 4) == "constant.builtin");
		const auto object = text.document->fold(0);
		REQUIRE(object);
		CHECK(object->last == 5);
		const auto list = text.document->fold(2);
		REQUIRE(list);
		CHECK(list->last == 4);
		CHECK(text.document->brackets({2, 10}).has_value());
	}

	TEST_CASE("Python: classes, functions, constants and indented folds") {
		const Text text("python", pythonSample);
		CHECK(text.styleAt(0, 0) == "keyword");
		CHECK(text.styleAt(2, 6) == "constructor"); // Greeter (capitalised)
		CHECK(text.styleAt(3, 8) == "function");
		CHECK(text.styleAt(5, 20) == "string");
		CHECK(text.styleAt(6, 15) == "constant.builtin"); // None
		// VALUE: the upstream query's first match wins, the capitalised-name pattern before the all-caps one
		CHECK(text.styleAt(8, 0) == "constructor");
		CHECK(text.styleAt(8, 8) == "number");
		const auto classFold = text.document->fold(2);
		REQUIRE(classFold);
		CHECK(classFold->last == 6);
		const auto method = text.document->fold(3);
		REQUIRE(method);
		CHECK(method->last == 6);
		const auto branch = text.document->fold(4);
		REQUIRE(branch);
		CHECK(branch->last == 5);
	}

	TEST_CASE("JavaScript: functions, builtins (#match?, #eq?) and a plain variable underneath") {
		const Text text("javascript",
		                "const total = require('x');\nfunction add(a, b) {\n  console.log(a);\n  return a + b;\n}\n");
		CHECK(text.styleAt(0, 0) == "keyword");
		CHECK(std::ranges::contains(text.stylesAt(0, 15), std::string("function"))); // require, a call
		CHECK(text.styleAt(0, 23) == "string");
		CHECK(text.styleAt(1, 9) == "function");
		CHECK(text.styleAt(2, 3) == "variable.builtin"); // console
		CHECK(text.styleAt(3, 2) == "keyword");
		const auto body = text.document->fold(1);
		REQUIRE(body);
		CHECK(body->last == 3);
	}

	TEST_CASE("TypeScript: types and interfaces") {
		const Text text("typescript", "interface Shape {\n  area(): number;\n}\nlet s: Shape;\n");
		CHECK(text.styleAt(0, 0) == "keyword");
		CHECK(text.styleAt(1, 10) == "type.builtin");
		CHECK(text.styleAt(3, 8) == "type");
		const auto body = text.document->fold(0);
		REQUIRE(body);
		CHECK(body->last == 1);
	}

	TEST_CASE("Markdown: headings, code and sections that fold") {
		const Text text("markdown", "# Title\n\nSome text.\n\n```cpp\nint x;\n```\n\n## Next\n\nMore.\n");
		CHECK(text.styleAt(0, 0) == "punctuation.special");
		CHECK(text.styleAt(0, 3) == "text.title");
		// The fence is literal text; its content is left to an injected language (@none) on top
		CHECK(std::ranges::contains(text.stylesAt(5, 1), std::string("text.literal")));
		CHECK(text.styleAt(5, 1) == "none");
		const auto section = text.document->fold(0);
		REQUIRE(section);
		CHECK(section->last >= 7);
		const auto code = text.document->fold(4);
		REQUIRE(code);
		CHECK(code->last == 6);
		CHECK_FALSE(text.document->brackets({0, 0})); // no bracket query
	}

	TEST_CASE("incremental highlighting after edits matches a fresh parse") {
		struct Sample {
			std::string_view language;
			const std::string* text;
		};
		const std::string js = "function f(a) {\n  return [a, 'b', 3];\n}\nconst x = { y: `t${1}` };\n";
		const std::vector<Sample> samples = {{"cpp", &cppSample}, {"python", &pythonSample}, {"javascript", &js}};
		const std::vector<std::string_view> pieces = {"x",  " ",  "\"", "{",  "}", "(", "\n",
		                                              "/*", "*/", "1",  "if", ";", "#", "'"};
		for(const Sample& sample : samples) {
			CAPTURE(sample.language);
			Text text(sample.language, *sample.text);
			std::mt19937 random(7);
			for(int round = 0; round < 60; ++round) {
				CAPTURE(round);
				const std::size_t offset = random() % (text.bytes.size() + 1);
				const std::size_t removed = std::min<std::size_t>(random() % 3, text.bytes.size() - offset);
				const std::string_view inserted = pieces[random() % pieces.size()];
				std::vector<services::SyntaxRows> changed;
				text.replace(offset, removed, inserted, &changed);
				CHECK_FALSE(changed.empty());
				const Text fresh(sample.language, text.bytes);
				CHECK(text.document->tree() == fresh.document->tree());
				CHECK(sameSpans(text.spans(), fresh.spans()));
			}
		}
	}

	TEST_CASE("an edit within a line re-highlights only the rows the parse reports changed") {
		Text text("cpp", cppSample);
		std::mt19937 random(11);
		for(int round = 0; round < 40; ++round) {
			CAPTURE(round);
			const std::vector<SyntaxSpan> before = text.spans();
			// A one-line edit: rows keep their numbers, so a row outside the changed ones must keep its runs
			const std::uint32_t row = static_cast<std::uint32_t>(random() % (text.rows() - 1));
			std::size_t start = 0;
			for(std::uint32_t r = 0; r < row; ++r) start = text.bytes.find('\n', start) + 1;
			const std::size_t end = text.bytes.find('\n', start);
			const std::size_t offset = start + random() % (end - start + 1);
			const std::string_view inserted = round % 3 == 0 ? "\"" : (round % 3 == 1 ? "/*" : "x1");
			std::vector<services::SyntaxRows> changed;
			text.replace(offset, 0, inserted, &changed);
			const std::vector<SyntaxSpan> after = text.spans();
			for(std::uint32_t r = 0; r < text.rows(); ++r) {
				const bool inChanged = std::ranges::any_of(
				    changed, [r](const services::SyntaxRows& rows) { return rows.first <= r && r <= rows.last; });
				if(!inChanged) CHECK_MESSAGE(sameSpans(spansOf(before, r), spansOf(after, r)), "row ", r);
			}
		}
	}

	TEST_CASE("a comment opened then closed colours the lines after it, then stops") {
		// The last line ends with a comment, so "/*" typed at the start runs to its "*/"
		Text text("cpp", "int a;\nint b;\nint c; /* end */\n");
		CHECK(text.styleAt(1, 0) == "type");
		std::vector<services::SyntaxRows> changed;
		text.replace(0, 0, "/*", &changed);
		CHECK(text.styleAt(1, 0) == "comment");
		CHECK(text.styleAt(2, 0) == "comment");
		REQUIRE_FALSE(changed.empty());
		CHECK(std::ranges::any_of(changed, [](const services::SyntaxRows& rows) { return rows.last >= 2; }));
		text.replace(8, 0, "*/"); // after "/*int a;": closes it on line 0
		CHECK(text.styleAt(1, 0) == "type");
		CHECK(text.styleAt(2, 0) == "type");
	}

	TEST_CASE("limits: too large, over budget, captures cut short") {
		services::SyntaxLimits small;
		small.maxBytes = 16;
		const auto document = SyntaxDocument::create("c", small);
		const std::string text = "int main(void) { return 0; }\n";
		const services::SyntaxReader read = [&](std::size_t byte, SyntaxPoint) {
			return std::string_view(text).substr(byte);
		};
		CHECK(document->parse(read, text.size()) == services::SyntaxParse::TooLarge);
		CHECK_FALSE(document->parsed());
		std::vector<SyntaxSpan> spans;
		CHECK(document->highlight(read, 0, 0, spans));
		CHECK(spans.empty());

		services::SyntaxLimits slow;
		slow.parseMilliseconds = 0;
		std::string big;
		for(int i = 0; i < 20000; ++i) big += "int f" + std::to_string(i) + "(int a) { return a * 2; }\n";
		const services::SyntaxReader readBig = [&](std::size_t byte, SyntaxPoint) {
			return std::string_view(big).substr(byte);
		};
		const auto late = SyntaxDocument::create("c", slow);
		CHECK(late->parse(readBig, big.size()) == services::SyntaxParse::OverBudget);
		CHECK_FALSE(late->parsed());

		services::SyntaxLimits few;
		few.maxCaptures = 3;
		const auto cut = SyntaxDocument::create("c", few);
		REQUIRE(cut->parse(read, text.size()) == services::SyntaxParse::Parsed);
		CHECK_FALSE(cut->highlight(read, 0, 0, spans));
		CHECK(spans.size() <= 3);
	}

	TEST_CASE("a reader that gives a line at a time") {
		const std::string text = cppSample;
		const services::SyntaxReader byLine = [&](std::size_t byte, SyntaxPoint) {
			const std::size_t end = text.find('\n', byte);
			return std::string_view(text).substr(byte, end == std::string::npos ? std::string::npos : end + 1 - byte);
		};
		const auto document = SyntaxDocument::create("cpp");
		REQUIRE(document->parse(byLine, text.size()) == services::SyntaxParse::Parsed);
		const Text whole("cpp", text);
		CHECK(document->tree() == whole.document->tree());
		std::vector<SyntaxSpan> spans;
		CHECK(document->highlight(byLine, 0, whole.rows() - 1, spans));
		CHECK(sameSpans(spans, whole.spans()));
	}

} // TEST_SUITE
