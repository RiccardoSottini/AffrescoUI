/// @file
/// @brief An editor's "patterns": each line coloured by the leftmost match of regular expressions, without a grammar.

#include "Headless.hpp"
#include "PatternHighlighter.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

using affresco::Color;
using affresco::format::PatternHighlighter;

namespace {

const Color plain{0, 0, 0, 1};    ///< The text colour.
const Color comment{1, 0, 0, 1};  ///< The comments'.
const Color keyword{0, 0, 1, 1};  ///< The keywords'.

/// @brief Colours one line with a comment and a keyword pattern.
/// @param text The line.
/// @return One colour per code point.
std::vector<Color> colorsOf(const std::u32string& text) {
	PatternHighlighter highlighter;
	REQUIRE_FALSE(highlighter.setRules({{";.*", "comment", comment}, {"\\b(defun|let)\\b", "keyword", keyword}}));
	affresco::TextModel model;
	std::vector<Color> colors(text.size(), plain);
	highlighter.colorLine(model, 0, text, colors);
	return colors;
}

} // namespace

TEST_CASE("editor patterns: the leftmost match colours, then the search goes on after it") {
	const auto colors = colorsOf(U"(defun f) ; let it be");
	CHECK(colors[0] == plain);
	CHECK(colors[1] == keyword); // d
	CHECK(colors[5] == keyword); // n
	CHECK(colors[6] == plain);
	CHECK(colors[10] == comment); // ;
	CHECK(colors[12] == comment); // "let" is inside the comment, which started first
	CHECK(colors.back() == comment);
}

TEST_CASE("editor patterns: columns count code points, not bytes") {
	const auto colors = colorsOf(U"(let ((è 1))) ; ok");
	CHECK(colors[1] == keyword);
	CHECK(colors[7] == plain);    // è, two bytes
	CHECK(colors[14] == plain);   // the space after the parentheses
	CHECK(colors[15] == comment); // ;
}

TEST_CASE("editor patterns: a pattern that doesn't compile is an error, and an editor takes them as a key") {
	PatternHighlighter highlighter;
	CHECK(highlighter.setRules({{"(", "comment", comment}}).has_value());

	affresco::detail::ManualClock clock;
	auto window = affresco::detail::createHeadlessWindow(300, 200, clock);
	auto screen = affresco::Screen::fromString(R"json({ "format": "affrescoui-screen", "version": 2,
		"colors": { "syntax.comment": "#b22222" }, "root": { "id": "root", "children": [{ "type": "editor",
		"text": ";; a note\n(+ 1 2)", "width": 280, "height": 160,
		"patterns": [{ "match": ";.*", "style": "comment" }, { "match": "[0-9]+" }] }] } })json");
	CHECK_NOTHROW(screen.build(*window));
	window->render();
}
