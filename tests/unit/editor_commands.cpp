/// @file
/// @brief Unit tests for the editor's named commands: line operations, case conversion, comment toggling and
/// selections.

#include "Headless.hpp"

#include <affresco/render/Font.hpp>
#include <affresco/render/TextEditor.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

using affresco::TextEditor;
using affresco::Window;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A headless window holding one focused editor, 400 by 300.
struct CommandWindow {
	/// @brief Opens the window, adds the editor and gives it a text.
	/// @param text The text.
	explicit CommandWindow(const std::string& text) {
		editor.setFont(affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 14.0f));
		editor.setText(text);
		editor.setSize(affresco::px(400.0f), affresco::px(300.0f));
		window->render();
		window->setFocus(&editor);
	}

	/// @brief Puts the caret at a place, with nothing selected.
	/// @param line The line, from 0.
	/// @param column The column, from 0.
	void at(std::size_t line, std::size_t column) { editor.selectRange({line, column}, {line, column}); }

	ManualClock clock;                                                      ///< The window's clock.
	std::unique_ptr<Window> window = createHeadlessWindow(800, 600, clock); ///< The window.
	TextEditor& editor = window->root().add<TextEditor>();                  ///< The editor.
};

} // namespace

TEST_CASE("editor commands: every name runs, and an unknown name is refused") {
	CommandWindow test("a\nb");
	for(const std::string_view name : TextEditor::commandNames()) CHECK(test.editor.runCommand(name));
	CHECK_FALSE(test.editor.runCommand("noSuchCommand"));
}

TEST_CASE("editor commands: duplicate, delete and move lines") {
	CommandWindow test("one\ntwo\nthree");
	test.at(1, 1);
	REQUIRE(test.editor.runCommand("duplicateLines"));
	CHECK(test.editor.text() == "one\ntwo\ntwo\nthree");
	CHECK(test.editor.caret() == TextEditor::Position{2, 1});

	REQUIRE(test.editor.runCommand("deleteLines"));
	CHECK(test.editor.text() == "one\ntwo\nthree");
	CHECK(test.editor.caret() == TextEditor::Position{2, 1});

	REQUIRE(test.editor.runCommand("moveLinesUp"));
	CHECK(test.editor.text() == "one\nthree\ntwo");
	CHECK(test.editor.caret() == TextEditor::Position{1, 1});

	test.at(0, 0);
	REQUIRE(test.editor.runCommand("moveLinesDown"));
	CHECK(test.editor.text() == "three\none\ntwo");
	CHECK(test.editor.caret() == TextEditor::Position{1, 0});
}

TEST_CASE("editor commands: join, sort, reverse and unique lines") {
	CommandWindow test("b\n    a\nb");
	test.at(0, 0);
	REQUIRE(test.editor.runCommand("joinLines"));
	CHECK(test.editor.text() == "b a\nb");

	CommandWindow sorted("c\na\nb\na");
	REQUIRE(sorted.editor.runCommand("sortLines"));
	CHECK(sorted.editor.text() == "a\na\nb\nc");
	REQUIRE(sorted.editor.runCommand("uniqueLines"));
	CHECK(sorted.editor.text() == "a\nb\nc");
	REQUIRE(sorted.editor.runCommand("reverseLines"));
	CHECK(sorted.editor.text() == "c\nb\na");
}

TEST_CASE("editor commands: case conversion acts on the selection, or the word at the caret") {
	CommandWindow test("hello big world");
	test.at(0, 7);
	REQUIRE(test.editor.runCommand("upperCase"));
	CHECK(test.editor.text() == "hello BIG world");

	test.editor.selectRange({0, 0}, {0, 15});
	REQUIRE(test.editor.runCommand("titleCase"));
	CHECK(test.editor.text() == "Hello Big World");
	REQUIRE(test.editor.runCommand("swapCase"));
	CHECK(test.editor.text() == "hELLO bIG wORLD");
	REQUIRE(test.editor.runCommand("lowerCase"));
	CHECK(test.editor.text() == "hello big world");
}

TEST_CASE("editor commands: toggle comment comments the lines at their smallest indentation, then uncomments them") {
	CommandWindow test("  a\n\n    b");
	test.editor.selectRange({0, 0}, {2, 5});
	test.editor.toggleComment("//");
	CHECK(test.editor.text() == "  // a\n\n  //   b");
	test.editor.toggleComment("//");
	CHECK(test.editor.text() == "  a\n\n    b");

	CommandWindow hash("x = 1");
	hash.editor.toggleComment("#");
	CHECK(hash.editor.text() == "# x = 1");
}

TEST_CASE("editor commands: expand to line and split into lines") {
	CommandWindow test("one\ntwo\nthree");
	test.at(0, 1);
	REQUIRE(test.editor.runCommand("expandToLine"));
	CHECK(test.editor.anchor() == TextEditor::Position{0, 0});
	CHECK(test.editor.caret() == TextEditor::Position{1, 0});

	test.editor.selectRange({0, 0}, {2, 5});
	REQUIRE(test.editor.runCommand("splitIntoLines"));
	CHECK(test.editor.cursors().size() == 2);
}

TEST_CASE("editor commands: select range clamps to the text") {
	CommandWindow test("ab\ncd");
	test.editor.selectRange({0, 1}, {9, 9});
	CHECK(test.editor.anchor() == TextEditor::Position{0, 1});
	CHECK(test.editor.caret() == TextEditor::Position{1, 2});
}
