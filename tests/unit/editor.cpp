/// @file
/// @brief Unit tests for render's TextEditor in a headless window: what a document can make it do with sizes far out
/// of range, and the bound on what its undo steps keep.

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

/// @brief A headless window holding one focused editor, 400 by 300, with three lines of text.
struct EditorWindow {
	/// @brief Opens the window and adds the editor.
	EditorWindow() {
		editor.setFont(affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 14.0f));
		editor.setText("one\ntwo\nthree");
		editor.setSize(affresco::px(400.0f), affresco::px(300.0f));
		window->render(); // laid out, so it is shown and takes keys
		window->setFocus(&editor);
	}

	/// @brief Presses a key, as the user would.
	/// @param key The key.
	/// @param mods The modifier keys held (GLFW_MOD_* bits).
	void press(Key key, int mods = 0) const { window->routeKey({key, 0, Action::Press, affresco::Mods{mods}}); }

	ManualClock clock;                                                      ///< The window's clock.
	std::unique_ptr<Window> window = createHeadlessWindow(800, 600, clock); ///< The window.
	TextEditor& editor = window->root().add<TextEditor>();                  ///< The editor.
};

} // namespace

// Undefined behaviour before: the number of rows (or the row under the pointer) was converted to an integer as it was
TEST_CASE("editor: a frame far larger than any integer pages and clicks to the end of the text") {
	EditorWindow test;
	test.editor.setPosition(affresco::px(0.0f), affresco::px(-1e30f));
	test.editor.setSize(affresco::px(400.0f), affresco::px(2e30f));
	test.window->render();

	test.press(Key::PageDown); // a page holds the whole text: to its end
	CHECK(test.editor.caret() == TextEditor::Position{2, 5});
	test.press(Key::PageUp);
	CHECK(test.editor.caret().line == 0);
	for(const Action action : {Action::Press, Action::Release}) // far below the first line, in the frame's terms
		test.window->routeMouseButton({affresco::MouseButton::Left, action, {}, {100.0f, 100.0f}});
	CHECK(test.editor.caret() == TextEditor::Position{2, 5});
}

TEST_CASE("editor: undo steps keep at most 256 MiB, the oldest dropped first") {
	EditorWindow test;
	// A step keeps the text its edit removed: here each text, 73,000 lines of 63 characters (about 4.45 MiB), so about
	// 57 fit, where 500 steps of a small text would be kept. Short lines, so the caret's line is quick to shape
	std::string lines;
	for(int line = 0; line < 73000; ++line) lines += std::string(63, 'a') + "\n";
	constexpr int texts = 62; // more than fit
	for(int i = 0; i < texts; ++i) test.editor.setText(std::to_string(i) + "\n" + lines);

	int undone = 0;
	for(std::string before = test.editor.text(); undone <= texts; ++undone) {
		test.press(Key::Z, 0x0002 | 0x0008); // Ctrl+Z, or Cmd+Z
		if(test.editor.text() == before) break;
		before = test.editor.text();
	}
	CHECK(undone >= 50);
	CHECK(undone <= 60);
	CHECK(test.editor.text().starts_with(std::to_string(texts - 1 - undone) + "\n"));
}
