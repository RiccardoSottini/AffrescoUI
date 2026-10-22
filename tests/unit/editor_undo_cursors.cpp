/// @file
/// @brief Unit tests for the editor's undo and redo with extra cursors: each step brings back the cursors it had.

#include "Headless.hpp"

#include <affresco/render/Font.hpp>
#include <affresco/render/TextEditor.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

using affresco::Action;
using affresco::Key;
using affresco::TextEditor;
using affresco::Window;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

constexpr int shiftKey = 0x1; ///< GLFW_MOD_SHIFT.
constexpr int superKey = 0x8; ///< GLFW_MOD_SUPER.

/// @brief A headless window holding one focused editor, 400 by 300.
struct UndoWindow {
	/// @brief Opens the window, adds the editor and gives it a text.
	/// @param text The text.
	explicit UndoWindow(const std::string& text) {
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

} // namespace

TEST_CASE("editor undo cursors: undo brings back the extra cursors, and redo the ones after the step") {
	UndoWindow test("ab\nab\nab");
	REQUIRE(test.editor.addCursor({{1, 0}, {1, 0}}));
	REQUIRE(test.editor.addCursor({{2, 0}, {2, 0}}));
	test.type("x");
	CHECK(test.editor.text() == "xab\nxab\nxab");
	test.press(Key::Escape); // one caret again
	CHECK(test.editor.cursors().empty());

	test.press(Key::Z, superKey);
	CHECK(test.editor.text() == "ab\nab\nab");
	REQUIRE(test.editor.cursors().size() == 2);
	CHECK(test.editor.cursors()[0].caret == TextEditor::Position{1, 0});
	CHECK(test.editor.cursors()[1].caret == TextEditor::Position{2, 0});

	test.press(Key::Escape);
	test.press(Key::Z, superKey | shiftKey);
	CHECK(test.editor.text() == "xab\nxab\nxab");
	REQUIRE(test.editor.cursors().size() == 2);
	CHECK(test.editor.cursors()[1].caret == TextEditor::Position{2, 1});
}

TEST_CASE("editor undo cursors: a step typed with one caret undoes with one caret") {
	UndoWindow test("ab\nab");
	test.type("y");
	REQUIRE(test.editor.addCursor({{1, 0}, {1, 0}}));
	test.press(Key::Z, superKey);
	CHECK(test.editor.text() == "ab\nab");
	CHECK(test.editor.cursors().empty());
}
