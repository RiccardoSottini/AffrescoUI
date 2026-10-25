/// @file
/// @brief Unit tests for the editor's tab size: what Tab and Shift+Tab do with it, the tabs in a text it replaces, its
/// bounds, and the editor node's `tabSize` key.

#include "Headless.hpp"

#include <affresco/affresco.hpp>
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

/// @brief A headless window holding one focused editor.
struct TabWindow {
	/// @brief Opens the window and adds the editor, showing "x".
	TabWindow() {
		editor.setFont(affresco::Font::load(std::filesystem::path(AFFRESCOUI_FONTS_DIR) / "Roboto-Regular.ttf", 14.0f));
		editor.setText("x");
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

TEST_CASE("editor: Tab, Shift+Tab and a text's tabs follow the tab size") {
	TabWindow test;
	CHECK(test.editor.tabSize() == 4);
	test.editor.setTabSize(2);
	test.press(Key::Tab); // the caret is at the start: up to the next stop, two spaces on
	CHECK(test.editor.text() == "  x");
	test.press(Key::Tab, 0x0001); // Shift+Tab: one level less
	CHECK(test.editor.text() == "x");
	test.editor.setText("\tx\n\t\ty");
	CHECK(test.editor.text() == "  x\n    y");
	test.editor.setTabSize(8);
	test.editor.setText("\tz");
	CHECK(test.editor.text() == "        z");
}

TEST_CASE("editor: a tab size out of range is clamped to 1 to 16 spaces") {
	TabWindow test;
	test.editor.setTabSize(0);
	CHECK(test.editor.tabSize() == 1);
	test.editor.setTabSize(1'000'000);
	CHECK(test.editor.tabSize() == TextEditor::maxTabSize);
}

TEST_CASE("editor node: tabSize sets the tab size, and one that isn't a whole number from 1 to 16 is refused") {
	ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	auto screen = affresco::Screen::fromString(R"({
		"format": "affrescoui-screen", "version": 2,
		"root": { "id": "root", "children": [
			{ "type": "editor", "id": "code", "width": 300, "height": 200, "text": "a", "tabSize": 2 },
			{ "type": "editor", "id": "plain", "width": 300, "height": 200, "text": "b" }
		] }
	})");
	screen.build(*window);
	const auto* code = dynamic_cast<TextEditor*>(screen.find("code"));
	const auto* plain = dynamic_cast<TextEditor*>(screen.find("plain"));
	REQUIRE(code != nullptr);
	REQUIRE(plain != nullptr);
	CHECK(code->tabSize() == 2);
	CHECK(plain->tabSize() == 4);

	for(const char* wrong : {"0", "17", "2.5", "\"four\""}) {
		std::string error;
		CHECK_FALSE(screen.applyRevision(std::string(R"({"set": {"#code/tabSize": )") + wrong + "}}", &error));
		CHECK(error.find("tabSize") != std::string::npos);
	}
	CHECK(code->tabSize() == 2);
}
