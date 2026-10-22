/// @file
/// @brief Unit tests for the editor's minimap (the width it takes, scrolling by pressing and dragging it) and its
/// input method composition, in a headless window.

#include "Headless.hpp"

#include <affresco/render/Font.hpp>
#include <affresco/render/TextEditor.hpp>
#include <affresco/render/Window.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>

using affresco::Action;
using affresco::MouseButton;
using affresco::TextEditor;
using affresco::Vec2;
using affresco::Window;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A headless window holding one focused editor, 400 by 300, of numbered lines.
struct MinimapWindow {
	/// @brief Opens the window and adds the editor.
	/// @param lines How many lines the text has.
	explicit MinimapWindow(std::size_t lines) {
		editor.setFont(affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 14.0f));
		std::string text;
		for(std::size_t i = 0; i < lines; ++i) text += (i ? "\n" : "") + ("line " + std::to_string(i));
		editor.setText(text);
		editor.setSize(affresco::px(400.0f), affresco::px(300.0f));
		window->render();
		window->setFocus(&editor);
	}

	/// @brief Presses or releases the left button.
	/// @param action Press or Release.
	/// @param at Where.
	void button(Action action, Vec2 at) const { window->routeMouseButton({MouseButton::Left, action, {}, at}); }

	ManualClock clock;                                                      ///< The window's clock.
	std::unique_ptr<Window> window = createHeadlessWindow(800, 600, clock); ///< The window.
	TextEditor& editor = window->root().add<TextEditor>();                  ///< The editor.
};

} // namespace

TEST_CASE("editor minimap: pressing it scrolls to the row under the pointer, and dragging follows") {
	MinimapWindow test(1000);
	const float right = test.editor.frame().right;
	// Without the minimap, a press there places the caret
	test.button(Action::Press, {right - 30.0f, 200.0f});
	test.button(Action::Release, {right - 30.0f, 200.0f});
	CHECK(test.editor.scroll().y == 0.0f);
	test.editor.setMinimap(true);
	test.window->render();
	test.button(Action::Press, {right - 30.0f, 200.0f});
	const float pressed = test.editor.scroll().y;
	CHECK(pressed > 0.0f);
	test.window->routeMouseMove({right - 30.0f, 260.0f});
	CHECK(test.editor.scroll().y > pressed);
	test.button(Action::Release, {right - 30.0f, 260.0f});
	const float released = test.editor.scroll().y;
	test.window->routeMouseMove({right - 30.0f, 290.0f});
	CHECK(test.editor.scroll().y == released);
}

TEST_CASE("editor minimap: a wrapped text gets narrower rows with it, and the same without") {
	std::string longLine;
	for(int i = 0; i < 100; ++i) longLine += "word ";
	MinimapWindow test(1);
	test.editor.setText(longLine);
	test.editor.setWrap(true);
	test.window->render();
	const std::size_t rows = test.editor.rowCount();
	test.editor.setMinimap(true);
	test.window->render();
	CHECK(test.editor.rowCount() > rows);
	test.editor.setMinimap(false);
	test.window->render();
	CHECK(test.editor.rowCount() == rows);
}

TEST_CASE("editor minimap: a composition in UTF-8 shows without editing the text") {
	MinimapWindow test(1);
	test.editor.setComposition(std::string_view("\xe3\x81\x8b"));
	test.window->render();
	CHECK(test.editor.text() == "line 0");
	test.editor.setComposition(std::string_view());
	CHECK(test.editor.text() == "line 0");
}
