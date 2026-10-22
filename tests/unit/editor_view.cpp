/// @file
/// @brief Unit tests for the editor's view in a headless window (wrapping, folds, decorations, extra cursors, find and
/// replace) and for core's TextPattern, with the bounds that untrusted input meets.

#include "Headless.hpp"

#include <affresco/core/TextPattern.hpp>
#include <affresco/render/Font.hpp>
#include <affresco/render/TextEditor.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::EditorDecoration;
using affresco::FoldRange;
using affresco::Key;
using affresco::PatternOptions;
using affresco::TextEditor;
using affresco::TextPattern;
using affresco::Window;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

constexpr int shiftKey = 0x1;   ///< GLFW_MOD_SHIFT.
constexpr int superKey = 0x8;   ///< GLFW_MOD_SUPER.
constexpr int altKey = 0x4;     ///< GLFW_MOD_ALT.

/// @brief A headless window holding one focused editor, 400 by 300.
struct ViewWindow {
	/// @brief Opens the window, adds the editor and gives it a text.
	/// @param text The text.
	explicit ViewWindow(const std::string& text) {
		editor.setFont(affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 14.0f));
		editor.setText(text);
		editor.setSize(affresco::px(400.0f), affresco::px(300.0f));
		window->render();
		window->setFocus(&editor);
	}

	/// @brief Presses a key, as the user would.
	/// @param key The key.
	/// @param mods The modifier keys held (GLFW_MOD_* bits).
	void press(Key key, int mods = 0) const { window->routeKey({key, 0, Action::Press, affresco::Mods{mods}}); }

	/// @brief Types text, one character at a time.
	/// @param text ASCII text.
	void type(std::string_view text) const {
		for(const char c : text) window->routeChar({static_cast<char32_t>(c)});
	}

	ManualClock clock;                                                      ///< The window's clock.
	std::unique_ptr<Window> window = createHeadlessWindow(800, 600, clock); ///< The window.
	TextEditor& editor = window->root().add<TextEditor>();                  ///< The editor.
};

/// @brief A text of numbered lines.
/// @param lines How many.
std::string numbered(std::size_t lines) {
	std::string text;
	for(std::size_t i = 0; i < lines; ++i) text += (i ? "\n" : "") + ("line " + std::to_string(i));
	return text;
}

} // namespace

TEST_CASE("editor view: a long line wraps into rows at the width, and unwraps") {
	std::string longLine;
	for(int i = 0; i < 200; ++i) longLine += "word ";
	ViewWindow test("short\n" + longLine + "\nend");
	CHECK(test.editor.rowCount() == 3);
	test.editor.setWrap(true);
	CHECK(test.editor.rowCount() > 6);
	const std::size_t wrapped = test.editor.rowCount();
	// Narrower: more rows
	test.editor.setSize(affresco::px(200.0f), affresco::px(300.0f));
	test.window->render();
	CHECK(test.editor.rowCount() > wrapped);
	test.editor.setWrap(false);
	CHECK(test.editor.rowCount() == 3);
}

TEST_CASE("editor view: typing in a wrapped line keeps the rows right") {
	ViewWindow test("a");
	test.editor.setWrap(true);
	for(int i = 0; i < 100; ++i) test.type("word ");
	test.window->render();
	CHECK(test.editor.rowCount() > 1);
	test.press(Key::A, superKey);
	test.press(Key::Backspace);
	CHECK(test.editor.rowCount() == 1);
}

TEST_CASE("editor view: folding hides the indented lines after a line, and unfolding shows them") {
	ViewWindow test("def f():\n    one\n    two\n\n    three\nafter");
	const auto range = test.editor.foldable(0);
	REQUIRE(range);
	CHECK(range->endLine == 4); // the blank line inside the block is folded with it
	CHECK_FALSE(test.editor.foldable(1));
	CHECK(test.editor.fold(0));
	CHECK(test.editor.rowCount() == 2);
	CHECK(test.editor.unfold(0));
	CHECK(test.editor.rowCount() == 6);
	test.editor.toggleFold(0);
	CHECK(test.editor.folds().size() == 1);
	test.editor.unfoldAll();
	CHECK(test.editor.folds().empty());
}

TEST_CASE("editor view: folds out of range are dropped, and a caret moved into a fold opens it") {
	ViewWindow test("a\n  b\n  c\nd");
	test.editor.setFolds({{0, 2}, {3, 1}, {0, 99}, {2, 2}});
	REQUIRE(test.editor.folds().size() == 1);
	CHECK(test.editor.folds()[0] == FoldRange{0, 2});
	// Down from line 0 skips the folded lines
	test.press(Key::Down);
	CHECK(test.editor.caret().line == 3);
	// Typing above the fold moves it down with its lines
	test.press(Key::Up);
	test.press(Key::Home);
	test.press(Key::Enter);
	REQUIRE(test.editor.folds().size() == 1);
	CHECK(test.editor.folds()[0] == FoldRange{1, 3});
}

TEST_CASE("editor view: folds a document offers again don't undo the user's folding") {
	ViewWindow test("a\n  b\nc\n  d");
	test.editor.offerFolds({{0, 1}});
	CHECK(test.editor.folds().size() == 1);
	test.editor.unfold(0);
	test.editor.offerFolds({{0, 1}}); // the same: the user's unfold stays
	CHECK(test.editor.folds().empty());
	test.editor.offerFolds({{2, 3}});
	CHECK(test.editor.folds().size() == 1);
}

TEST_CASE("editor view: decorations are clamped, capped per line and move with the text") {
	ViewWindow test("one\ntwo\nthree");
	std::vector<EditorDecoration> given;
	for(int i = 0; i < 20; ++i) given.push_back({.line = 1, .endLine = 1});
	given.push_back({.line = 0, .endLine = 99, .kind = EditorDecoration::Kind::Squiggle});
	given.push_back({.line = 50});
	given.push_back({.line = 2, .kind = EditorDecoration::Kind::Inline, .text = std::u32string(10'000, U'x')});
	test.editor.setDecorations(given);
	const auto& kept = test.editor.decorations();
	REQUIRE(kept.size() == 1 + TextEditor::maxDecorationsPerLine + 1);
	CHECK(kept.front().endLine == 2);
	CHECK(kept.back().text.size() <= 256);

	test.press(Key::Enter); // a line above line 1
	CHECK(test.editor.decorations()[1].line == 2);
}

TEST_CASE("editor view: extra cursors type, delete and move together") {
	ViewWindow test("ab\ncd\nef");
	test.press(Key::Down, superKey | altKey);
	test.press(Key::Down, superKey | altKey);
	CHECK(test.editor.cursors().size() == 2);
	test.type("x");
	CHECK(test.editor.text() == "xab\nxcd\nxef");
	test.press(Key::Right);
	test.press(Key::Backspace);
	CHECK(test.editor.text() == "xb\nxd\nxf");
	test.press(Key::Escape);
	CHECK(test.editor.cursors().empty());
	test.type("y");
	CHECK(test.editor.text() == "xyb\nxd\nxf");
}

TEST_CASE("editor view: Cmd+D selects the word, then each next occurrence, wrapping once") {
	ViewWindow test("cat dog cat\ncat");
	test.press(Key::D, superKey);
	CHECK(test.editor.cursors().empty());
	test.press(Key::D, superKey);
	test.press(Key::D, superKey);
	CHECK(test.editor.cursors().size() == 2);
	test.press(Key::D, superKey); // every occurrence has a cursor
	CHECK(test.editor.cursors().size() == 2);
	test.type("fox");
	CHECK(test.editor.text() == "fox dog fox\nfox");
}

TEST_CASE("editor view: a cursor where one is already isn't added, and the cap holds") {
	ViewWindow test("abc");
	CHECK_FALSE(test.editor.addCursor({{0, 0}, {0, 0}})); // the caret's own place
	CHECK(test.editor.addCursor({{0, 2}, {0, 2}}));
	CHECK_FALSE(test.editor.addCursor({{0, 2}, {0, 2}}));
	CHECK(test.editor.addCursor({{0, 99}, {0, 99}})); // clamped to the line's end
	CHECK(test.editor.cursors().back().caret == TextEditor::Position{0, 3});
}

TEST_CASE("editor view: find highlights, steps through and replaces plain, whole-word and regex matches") {
	ViewWindow test("foo food foo\nbar foo");
	CHECK_FALSE(test.editor.setFind("foo", {}));
	CHECK(test.editor.matchCount() == 4);
	CHECK_FALSE(test.editor.setFind("foo", {.wholeWord = true}));
	CHECK(test.editor.matchCount() == 3);
	CHECK(test.editor.findNext());
	CHECK(test.editor.caret() == TextEditor::Position{0, 3});
	CHECK(test.editor.replaceNext("baz"));
	CHECK(test.editor.text() == "baz food foo\nbar foo");
	CHECK(test.editor.caret() == TextEditor::Position{0, 12}); // the next match is selected

	CHECK_FALSE(test.editor.setFind("(\\w+) (foo)\\b", {.regex = true}));
	CHECK(test.editor.replaceAll("$2 $1") == 2);
	CHECK(test.editor.text() == "baz foo food\nfoo bar");
	test.press(Key::Z, superKey); // one undo step
	CHECK(test.editor.text() == "baz food foo\nbar foo");
}

TEST_CASE("editor view: Cmd+G steps backwards with Shift, and a bad pattern highlights nothing") {
	ViewWindow test("a1 a2 a3");
	CHECK_FALSE(test.editor.setFind("a\\d", {.regex = true}));
	test.press(Key::G, superKey);
	CHECK(test.editor.caret() == TextEditor::Position{0, 2});
	test.press(Key::G, superKey);
	test.press(Key::G, superKey | shiftKey);
	CHECK(test.editor.caret() == TextEditor::Position{0, 2});
	CHECK(test.editor.setFind("(unclosed", {.regex = true}));
	CHECK(test.editor.matchCount() == 0);
	CHECK_FALSE(test.editor.findNext());
}

TEST_CASE("editor view: replacing in a read-only editor changes nothing") {
	ViewWindow test("foo");
	test.editor.setReadOnly(true);
	CHECK_FALSE(test.editor.setFind("foo", {}));
	CHECK(test.editor.replaceAll("bar") == 0);
	CHECK(test.editor.text() == "foo");
}

TEST_CASE("editor view: Cmd+Alt+[ folds the caret's block and Cmd+Alt+] unfolds it") {
	ViewWindow test("a\n  b\nc");
	test.press(Key::LeftBracket, superKey | altKey);
	CHECK(test.editor.rowCount() == 2);
	test.press(Key::RightBracket, superKey | altKey);
	CHECK(test.editor.rowCount() == 3);
}

TEST_CASE("editor view: a large text folds, wraps and paints without measuring every line") {
	ViewWindow test(numbered(200'000));
	test.editor.setFolds({{10, 150'000}});
	CHECK(test.editor.rowCount() == 200'000 - 150'000 + 10);
	test.editor.setWrap(true);
	test.window->render();
	CHECK(test.editor.rowCount() == 200'000 - 150'000 + 10);
}

TEST_CASE("text pattern: plain, whole-word, case and regex matching, in bytes") {
	TextPattern plain;
	REQUIRE_FALSE(plain.compile("a.b", {}));
	CHECK(plain.find("xa.b").match->offset == 1);
	CHECK_FALSE(plain.find("axb").match); // the dot is literal

	TextPattern word;
	REQUIRE_FALSE(word.compile("cat", {.wholeWord = true, .caseSensitive = true}));
	CHECK(word.find("concat Cat cat").match->offset == 11);

	TextPattern regex;
	REQUIRE_FALSE(regex.compile("é+", {.regex = true}));
	const auto found = regex.find("aééb");
	REQUIRE(found.match);
	CHECK(found.match->offset == 1);
	CHECK(found.match->length == 4);
}

TEST_CASE("text pattern: expansion, errors and the limits that stop runaway patterns") {
	TextPattern pattern;
	REQUIRE_FALSE(pattern.compile("(?<year>\\d{4})-(\\d{2})", {.regex = true}));
	const std::string subject = "on 2024-05";
	const auto found = pattern.find(subject);
	REQUIRE(found.match);
	CHECK(pattern.expand(subject, *found.match, "$2/${year}") == "05/2024");

	CHECK(pattern.compile("(", {.regex = true}));
	CHECK_FALSE(pattern.valid());
	CHECK(pattern.compile(std::string(TextPattern::maxPatternBytes + 1, 'a'), {}));

	// Catastrophic backtracking ends at the step limit instead of hanging
	TextPattern runaway;
	REQUIRE_FALSE(runaway.compile("(a+)+$", {.regex = true}));
	const auto stopped = runaway.find(std::string(64, 'a') + "b");
	CHECK_FALSE(stopped.match);
	CHECK(stopped.exceeded);
}
