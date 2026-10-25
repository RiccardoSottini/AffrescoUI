/// @file
/// @brief Unit tests for the editor's scroll bar marks: which rows they mark and in what colour, that they follow the
/// text and the query, and the editor node's `scrollMarks` key.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Font.hpp>
#include <affresco/render/TextEditor.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using affresco::Color;
using affresco::EditorDecoration;
using affresco::TextEditor;
using affresco::Window;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A headless window holding one editor with ten lines.
struct MarkWindow {
	/// @brief Opens the window and adds the editor.
	MarkWindow() {
		editor.setFont(affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 14.0f));
		editor.setText("a\nb\nc\nb\ne\nf\ng\nh\ni\nj");
		editor.setSize(affresco::px(400.0f), affresco::px(100.0f));
		editor.setMatchColor(Color::hex(0x00ff00).withAlpha(0.3f));
		editor.setCaretColor(Color::hex(0x0000ff));
		window->render();
	}

	/// @brief The rows marked.
	[[nodiscard]] std::vector<std::size_t> rows() const {
		std::vector<std::size_t> marked;
		for(const TextEditor::ScrollMark& mark : editor.scrollMarkList()) marked.push_back(mark.row);
		return marked;
	}

	ManualClock clock;                                                      ///< The window's clock.
	std::unique_ptr<Window> window = createHeadlessWindow(400, 300, clock); ///< The window.
	TextEditor& editor = window->root().add<TextEditor>();                  ///< The editor.
};

} // namespace

TEST_CASE("editor: the scroll bar marks decorations, find's matches and the caret, opaque, by row") {
	MarkWindow test;
	CHECK(test.editor.scrollMarkList().empty()); // off by default
	test.editor.setScrollMarks(true);
	CHECK(test.rows() == std::vector<std::size_t>{0}); // the caret only

	EditorDecoration squiggle;
	squiggle.line = 7;
	squiggle.endLine = 7;
	squiggle.kind = EditorDecoration::Kind::Squiggle;
	squiggle.color = Color::hex(0xff0000).withAlpha(0.5f);
	EditorDecoration note = squiggle;
	note.line = note.endLine = 2;
	note.kind = EditorDecoration::Kind::Inline; // after the line's text: not marked
	test.editor.setDecorations({squiggle, note});
	CHECK(test.editor.setFind("b", {}) == std::nullopt);

	const std::vector<TextEditor::ScrollMark> marks = test.editor.scrollMarkList();
	REQUIRE(marks.size() == 4);
	CHECK(test.rows() == std::vector<std::size_t>{0, 1, 3, 7});
	CHECK(marks[0].color == Color::hex(0x0000ff));
	CHECK(marks[1].color == Color::hex(0x00ff00));
	CHECK(marks[3].color == Color::hex(0xff0000)); // made opaque
	test.window->render();                         // drawn over the scroll bar

	test.editor.setDecorations({});
	test.editor.setText("b\nx");
	CHECK(test.rows() == std::vector<std::size_t>{0, 0}); // the match and the caret share a row, in two colours
	CHECK(test.editor.scrollMarkList().size() == 2);
	CHECK(test.editor.setFind("zzz", {}) == std::nullopt);
	CHECK(test.editor.scrollMarkList().size() == 1);
	test.editor.setScrollMarks(false);
	CHECK(test.editor.scrollMarkList().empty());
}

TEST_CASE("editor node: scrollMarks turns the marks on, and one that isn't true or false is refused") {
	ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	auto screen = affresco::Screen::fromString(R"({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [
			{ "type": "editor", "id": "code", "width": 300, "height": 200, "text": "a", "scrollMarks": true },
			{ "type": "editor", "id": "plain", "width": 300, "height": 200, "text": "b" }
		] }
	})");
	screen.build(*window);
	const auto* code = dynamic_cast<TextEditor*>(screen.find("code"));
	const auto* plain = dynamic_cast<TextEditor*>(screen.find("plain"));
	REQUIRE(code != nullptr);
	REQUIRE(plain != nullptr);
	CHECK(code->scrollMarks());
	CHECK_FALSE(plain->scrollMarks());

	std::string error;
	CHECK_FALSE(screen.applyRevision(R"({"set": {"#code/scrollMarks": "yes"}})", &error));
	CHECK(error.find("scrollMarks") != std::string::npos);
	CHECK(code->scrollMarks());
}
