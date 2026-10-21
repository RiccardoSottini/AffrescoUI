/// @file
/// @brief Unit tests for render's TextModel: edits, the line index, positions and offsets, undo as operations, change
/// events, invalid UTF-8, and the cost of a 50 MB text (open, edit, memory).

#include <affresco/render/TextModel.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using affresco::TextChange;
using affresco::TextLines;
using affresco::TextModel;
using affresco::TextPosition;

namespace {

/// @brief A model's lines, decoded and encoded again, so a test compares strings.
/// @param model The model.
std::vector<std::string> linesOf(const TextModel& model) {
	std::vector<std::string> out;
	std::string all = model.text();
	std::size_t start = 0;
	for(std::size_t feed = all.find('\n'); feed != std::string::npos; feed = all.find('\n', start)) {
		out.push_back(all.substr(start, feed - start));
		start = feed + 1;
	}
	out.push_back(all.substr(start));
	return out;
}

} // namespace

TEST_CASE("text model: inserts and erases keep the text and its lines") {
	TextModel model;
	CHECK(model.lineCount() == 1);
	CHECK(model.size() == 0);
	model.assign("first\nsecond\nthird");
	CHECK(model.lineCount() == 3);
	CHECK(model.lineLength(1) == 6);

	CHECK(model.insert({1, 3}, "--\n--") == TextPosition{2, 2});
	CHECK(model.text() == "first\nsec--\n--ond\nthird");
	CHECK(model.lineCount() == 4);
	model.erase({0, 2}, {2, 2});
	CHECK(model.text() == "fiond\nthird");
	CHECK(linesOf(model) == std::vector<std::string>{"fiond", "third"});
	CHECK(model.textIn({0, 3}, {1, 2}) == "nd\nth");
	CHECK(model.equals("fiond\nthird"));
	CHECK_FALSE(model.equals("fiond\nthirds"));
}

TEST_CASE("text model: positions and offsets count code points, not bytes") {
	TextModel model;
	model.assign("caf\xC3\xA9\n\xE2\x82\xAC" "5");
	CHECK(model.lineLength(0) == 4);
	CHECK(model.offsetOf({0, 4}) == 5);
	CHECK(model.offsetOf({1, 1}) == 9);
	CHECK(model.offsetOf({1, 99}) == 10); // past the end of its line: the end
	CHECK(model.positionOf(9) == TextPosition{1, 1});
	std::u32string line;
	model.line(1, line);
	CHECK(line == U"€5");
	CHECK(model.longestLine() == 5);
}

TEST_CASE("text model: invalid UTF-8 becomes U+FFFD as it comes in") {
	TextModel model;
	model.assign("a\xFF" "b\xC3");
	CHECK(model.text() == "a\xEF\xBF\xBD" "b\xEF\xBF\xBD");
	model.insert({0, 1}, "\x80");
	CHECK(model.lineLength(0) == 5);
}

TEST_CASE("text model: many small edits in a large text agree with a string") {
	TextModel model;
	std::string expected;
	for(int line = 0; line < 2000; ++line) expected += "line " + std::to_string(line) + "\n";
	model.assign(expected);
	unsigned seed = 7;
	for(int i = 0; i < 3000; ++i) {
		seed = seed * 1103515245u + 12345u;
		const std::size_t offset = (seed >> 8) % (expected.size() + 1);
		if(i % 3 == 2 && offset < expected.size()) {
			const std::size_t length = std::min<std::size_t>(seed % 7 + 1, expected.size() - offset);
			model.erase(model.positionOf(offset), model.positionOf(offset + length));
			expected.erase(offset, length);
		} else {
			const std::string text = i % 5 == 0 ? "x\ny" : "ab";
			model.insert(model.positionOf(offset), text);
			expected.insert(offset, text);
		}
	}
	CHECK(model.text() == expected);
	CHECK(model.lineCount() == static_cast<std::size_t>(std::ranges::count(expected, '\n')) + 1);
	const std::vector<std::string> lines = linesOf(model);
	std::u32string decoded;
	for(std::size_t line = 0; line < lines.size(); line += 97) {
		model.line(line, decoded);
		CHECK(decoded.size() == lines[line].size()); // ASCII
		CHECK(model.offsetOf({line, 0}) + lines[line].size() == model.offsetOf({line, SIZE_MAX}));
	}
}

TEST_CASE("text model: undo and redo replay operations, restoring the caret") {
	TextModel model;
	model.assign("hello");
	model.beginStep(false, {0, 5}, {0, 5});
	model.insert({0, 5}, " world");
	model.endStep();
	model.beginStep(true, {0, 11}, {0, 11}); // typing on: the same step
	model.insert({0, 11}, "!");
	model.endStep();
	model.beginStep(false, {0, 0}, {0, 5});
	model.erase({0, 0}, {0, 6});
	model.endStep();
	CHECK(model.text() == "world!");
	CHECK(model.undoSteps() == 2);

	TextPosition caret{0, 0}, anchor{0, 0};
	CHECK(model.undo(false, caret, anchor));
	CHECK(model.text() == "hello world!");
	CHECK(caret == TextPosition{0, 0});
	CHECK(anchor == TextPosition{0, 5});
	CHECK(model.undo(false, caret, anchor));
	CHECK(model.text() == "hello");
	CHECK(caret == TextPosition{0, 5});
	CHECK_FALSE(model.undo(false, caret, anchor));
	CHECK(model.undo(true, caret, anchor));
	CHECK(model.text() == "hello world!");
	CHECK(model.undo(true, caret, anchor));
	CHECK(model.text() == "world!");
	CHECK_FALSE(model.undo(true, caret, anchor));

	model.insert({0, 0}, "x"); // an edit outside a step: the history no longer matches
	CHECK(model.undoSteps() == 0);
}

TEST_CASE("text model: change events give the range replaced") {
	TextModel model;
	model.assign("one\ntwo\nthree");
	std::vector<TextChange> changes;
	model.onChanged.connect([&changes](const TextChange& change) { changes.push_back(change); });
	model.erase({0, 1}, {2, 2});
	model.insert({0, 1}, "a\nb");
	REQUIRE(changes.size() == 2);
	CHECK(changes[0].from == TextPosition{0, 1});
	CHECK(changes[0].oldEnd == TextPosition{2, 2});
	CHECK(changes[0].newEnd == TextPosition{0, 1});
	CHECK(changes[0].removed == 9);
	CHECK(changes[1].newEnd == TextPosition{1, 1});
	CHECK(changes[1].inserted == 3);
}

TEST_CASE("text model: decoded lines are reused until the text changes") {
	TextModel model;
	model.assign("a\nb");
	const TextLines lines(model);
	CHECK(lines.size() == 2);
	CHECK(lines[1] == U"b");
	CHECK(&lines[1] == &lines[1]);
	model.insert({1, 1}, "c");
	CHECK(lines.back() == U"bc");
}

// The narrow test run in speed mode: a 50 MB text opens in a second, an edit costs 50 microseconds, memory 1.5 times
TEST_CASE("text model: 50 MB opens in 1 s, an edit takes 50 us, memory stays under 1.5 times the text") {
	using Clock = std::chrono::steady_clock;
	const std::string line = "    const value = compute(first, second) + offset; // a line of a large source file\n";
	std::string text;
	text.reserve(std::size_t{50} << 20);
	while(text.size() + line.size() <= (std::size_t{50} << 20)) text += line;

	TextModel model;
	const auto opening = Clock::now();
	model.assign(text);
	const double openSeconds = std::chrono::duration<double>(Clock::now() - opening).count();
	CHECK(openSeconds <= 1.0);
	CHECK(model.lineCount() == text.size() / line.size() + 1);

	constexpr int edits = 20000;
	unsigned seed = 1;
	const auto editing = Clock::now();
	for(int i = 0; i < edits; ++i) {
		seed = seed * 1103515245u + 12345u;
		const TextPosition at{(seed >> 4) % (model.lineCount() - 1), 12};
		model.beginStep(false, at, at);
		if(i % 2 == 0) model.insert(at, "x");
		else model.erase(at, {at.line, at.column + 1});
		model.endStep();
	}
	const double editMicroseconds = std::chrono::duration<double, std::micro>(Clock::now() - editing).count() / edits;
	CHECK(editMicroseconds <= 50.0);
	MESSAGE("open " << openSeconds << " s, edit " << editMicroseconds << " us, memory "
	                << static_cast<double>(model.memory()) / static_cast<double>(text.size()) << "x");
	CHECK(static_cast<double>(model.memory()) <= 1.5 * static_cast<double>(text.size()));
}
