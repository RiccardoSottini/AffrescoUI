/// @file
/// @brief The editor's caret styles (src/editor/TextEditorCaret.cpp): a document's "caretStyle" sets the line, block
/// or underline caret, a {{binding}} changes it, and any other value is an error.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

using affresco::TextEditor;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief Collects the errors logged while it lives.
class Errors {
public:
	/// @brief Starts collecting.
	Errors() {
		affresco::setLogSink([this](affresco::LogLevel level, std::string_view message) {
			if(level == affresco::LogLevel::Error) lines.emplace_back(message);
		});
	}
	/// @brief Gives the log back to stderr.
	~Errors() { affresco::setLogSink(nullptr); }
	Errors(const Errors&) = delete;
	Errors& operator=(const Errors&) = delete;
	Errors(Errors&&) = delete;
	Errors& operator=(Errors&&) = delete;

	std::vector<std::string> lines; ///< The errors, oldest first.
};

/// @brief A screen with a focused editor whose caret style is bound to state.caret, on a headless window.
struct Caret {
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	std::unique_ptr<affresco::Screen> screen; ///< The screen.

	/// @brief Builds the screen.
	/// @param style The editor's "caretStyle", as JSON, or empty to leave it out.
	explicit Caret(const std::string& style)
	    : window(createHeadlessWindow(400, 300, clock)),
	      screen(std::make_unique<affresco::Screen>(affresco::Screen::fromString(
	          R"({"version": 2, "state": {"caret": "block"}, "actions": {"under": [{"set": "state.caret", "to": "underline"}],
			"bar": [{"set": "state.caret", "to": "bar"}]},
			"root": {"id": "root", "children": [{"id": "code", "type": "editor", "text": "ab\n", "autofocus": true,
			"width": 300, "height": 100)" +
	          (style.empty() ? std::string() : R"(, "caretStyle": )" + style) + "}]}}"))) {
		screen->build(*window);
		window->render();
	}
	/// @brief Presses and releases the right arrow.
	void right() const {
		window->routeKey({affresco::Key::Right, 0, affresco::Action::Press, {}});
		window->routeKey({affresco::Key::Right, 0, affresco::Action::Release, {}});
	}
	/// @brief The editor.
	/// @return It.
	[[nodiscard]] TextEditor& editor() const { return dynamic_cast<TextEditor&>(*window->root().findById("code")); }
};

} // namespace

TEST_CASE("editor caret: the line caret is the default, and a document picks block or underline") {
	CHECK(Caret("").editor().caretStyle() == TextEditor::CaretStyle::Line);
	CHECK(Caret(R"("line")").editor().caretStyle() == TextEditor::CaretStyle::Line);
	CHECK(Caret(R"("block")").editor().caretStyle() == TextEditor::CaretStyle::Block);
	CHECK(Caret(R"("underline")").editor().caretStyle() == TextEditor::CaretStyle::Underline);
}

TEST_CASE("editor caret: a binding follows the state, and every style paints over a letter and past the line's end") {
	const Caret caret(R"("{{state.caret}}")");
	TextEditor& editor = caret.editor();
	CHECK(editor.caretStyle() == TextEditor::CaretStyle::Block);
	for(int n = 0; n < 2; ++n) { // over each letter, then at the line's end
		caret.window->render();
		caret.right();
	}
	CHECK(editor.caret() == TextEditor::Position{0, 2});
	caret.window->render();
	caret.screen->runAction("under", caret.window->root());
	caret.window->render();
	CHECK(editor.caretStyle() == TextEditor::CaretStyle::Underline);
	caret.right(); // the empty last line
	CHECK(editor.caret() == TextEditor::Position{1, 0});
	caret.window->render();
}

TEST_CASE("editor caret: another style is an error, in the document or from a binding") {
	CHECK_THROWS_WITH(Caret(R"("bar")"), doctest::Contains("caretStyle: expected line, block or underline"));
	const Caret caret(R"("{{state.caret}}")");
	const Errors errors;
	caret.screen->runAction("bar", caret.window->root());
	caret.window->render();
	REQUIRE_FALSE(errors.lines.empty());
	CHECK(errors.lines[0].contains("caretStyle: expected line, block or underline"));
}
