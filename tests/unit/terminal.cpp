/// @file
/// @brief The terminal node's model (libvterm): text, colours, the cursor, the bounded scrollback, a resize, the keys'
/// bytes, wide characters, and escape sequences kept inside it (the title only when allowed, no clipboard writes);
/// and the `terminal` node and its steps on a screen: what the steps check, that the checker knows them, and that a
/// revision can't add one.

#include "Checker.hpp"
#include "Headless.hpp"
#include "TerminalModel.hpp"
#include "TerminalView.hpp"

#include <affresco/affresco.hpp>
#include <affresco/core/Log.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::format::TerminalCell;
using affresco::format::TerminalModel;

TEST_CASE("terminal: text, colours and the cursor") {
	TerminalModel model(4, 20);
	model.write("hello\r\n\x1b[31mred\x1b[0m \x1b[7mrev\x1b[0m");
	CHECK(model.lineText(0) == U"hello");
	CHECK(model.lineText(1) == U"red rev");
	const TerminalCell plain = model.cell(0, 0);
	CHECK(plain.character == U'h');
	CHECK(plain.defaultForeground);
	CHECK(plain.defaultBackground);
	const TerminalCell red = model.cell(1, 0);
	CHECK_FALSE(red.defaultForeground);
	CHECK(red.defaultBackground);
	CHECK(model.cell(1, 4).reverse);
	CHECK(model.cursor().row == 1);
	CHECK(model.cursor().column == 7);
	model.write("\x1b[?25l");
	CHECK_FALSE(model.cursor().visible);
	CHECK(model.cell(0, 99).character == 0);
}

TEST_CASE("terminal: the scrollback is bounded, and lines come back when it grows taller") {
	TerminalModel model(3, 10, 5);
	for(int line = 0; line < 20; ++line) model.write("line " + std::to_string(line) + "\r\n");
	CHECK(model.scrollbackSize() == 5);
	CHECK(model.lineText(model.scrollbackSize() + 1) == U"line 19");
	model.resize(6, 10);
	CHECK(model.rows() == 6);
	CHECK(model.scrollbackSize() == 2);
	CHECK(model.lineText(model.scrollbackSize()) == U"line 15");
	model.setScrollbackLimit(1);
	CHECK(model.scrollbackSize() == 1);
	model.resize(0, 5000);
	CHECK(model.rows() == 1);
	CHECK(model.columns() == affresco::format::mostTerminalCells);
	model.write("\x1b[3J");
	CHECK(model.scrollbackSize() == 0);
}

TEST_CASE("terminal: keys become the bytes a program reads") {
	TerminalModel model(4, 20);
	CHECK(model.pressKey(affresco::Key::Enter));
	CHECK(model.takeOutput() == "\r");
	CHECK(model.pressKey(affresco::Key::Up));
	CHECK(model.takeOutput() == "\x1b[A");
	model.typeCharacter(U'c', affresco::Mods{0x0002});
	CHECK(model.takeOutput() == "\x03");
	model.typeCharacter(U'é');
	CHECK(model.takeOutput() == "\xC3\xA9");
	CHECK_FALSE(model.pressKey(affresco::Key::A));
	CHECK(model.takeOutput().empty());
}

TEST_CASE("terminal: the title only when allowed, and clipboard writes are dropped") {
	TerminalModel model(4, 20);
	model.write("\x1b]2;secret\x07");
	CHECK(model.title().empty());
	model.setAllowTitle(true);
	model.write("\x1b]2;build\x07");
	CHECK(model.title() == "build");
	model.write("\x1b]52;c;aGVsbG8=\x07");
	CHECK(model.takeOutput().empty());
	CHECK(model.lineText(0).empty());
	model.setAllowTitle(false);
	CHECK(model.title().empty());

	// A title past the limit is cut on a whole character, so it stays UTF-8
	model.setAllowTitle(true);
	std::string longTitle = "\x1b]2;x";
	for(int i = 0; i < 200; ++i) longTitle += "\xC3\xA9";
	model.write(longTitle + "\x07");
	CHECK(model.title().size() == 255);
	CHECK(static_cast<unsigned char>(model.title().back()) == 0xA9);

	// A write that ends after ESC and a control, inside the title, adds nothing to it (libvterm gave that last piece
	// the length -1, and read past the write: found by the terminal fuzzer)
	model.write("\x1b]2;ab\x1b");
	model.write(std::string(1, '\0'));
	CHECK(model.title() == "ab");
	model.write("\x07");
	CHECK(model.title() == "ab");
}

TEST_CASE("terminal: a wide character covers two columns") {
	TerminalModel model(2, 10);
	model.write("\xE4\xB8\xADx");
	CHECK(model.cell(0, 0).character == U'中');
	CHECK(model.cell(0, 0).width == 2);
	CHECK(model.cell(0, 1).width == 0);
	CHECK(model.cell(0, 2).character == U'x');
	CHECK(model.lineText(0) == U"中x");

	// A terminal has two columns at least, which a wide character covers (libvterm wrote outside a row of one)
	TerminalModel narrow(3, 1);
	CHECK(narrow.columns() == 2);
	narrow.write("\xE4\xB8\xAD\xE4\xB8\xAD");
	CHECK(narrow.lineText(0) == U"中");
	CHECK(narrow.lineText(1) == U"中");
	narrow.resize(3, 0);
	CHECK(narrow.columns() == 2);
}

TEST_CASE("terminal: a screen made shorter than its scroll region scrolls as a whole") {
	// libvterm kept the region's top past the new bottom, and scrolled a negative number of lines (found by the
	// terminal fuzzer)
	TerminalModel model(10, 20);
	model.write("\x1b[8;10r");
	model.resize(5, 20);
	for(int line = 0; line < 8; ++line) model.write("line " + std::to_string(line) + "\r\n");
	CHECK(model.lineText(model.scrollbackSize() + 3) == U"line 7");
}

TEST_CASE("terminal: a C1 control written in UTF-8 takes a column, and never moves the cursor backwards") {
	// libvterm's own width of U+009B is -1: at column 0 it moved the cursor off the screen, and a tab stop set there
	// wrote outside its table (found by the terminal fuzzer)
	for(const bool split : {false, true}) {
		CAPTURE(split);
		TerminalModel model(4, 11);
		if(split) {
			model.write("\xC2");
			model.write("\x9B");
		} else {
			model.write("\xC2\x9B");
		}
		CHECK(model.cursor().column == 1);
		CHECK(model.cell(0, 0).character == U'\uFFFD');
		model.write("\x1bH\tx");
		CHECK(model.cursor().row == 0);
		CHECK(model.cursor().column >= 2);
	}
}

TEST_CASE("terminal: repeating a glyph of no width ends") {
	// libvterm's REP stepped the last glyph's width until the column it was given, forever when that width was 0
	// (found by the terminal fuzzer)
	TerminalModel model(4, 20);
	model.write("\x1b[5b"); // before any glyph
	CHECK(model.cursor().column == 0);
	model.write("\u200B\x1b[5bx");
	CHECK(model.cursor().row == 0);
	CHECK(model.cursor().column <= 1);
	model.write("y\x1b[3b");
	CHECK(model.lineText(0).ends_with(U"yyyy"));
	// A wide glyph repeated up to the row's end stops where it no longer fits (libvterm wrote its second half past
	// the row)
	TerminalModel narrow(2, 4);
	narrow.write("a\u4E00\x1b[1b");
	CHECK(narrow.lineText(0) == U"a\u4E00");
	CHECK(narrow.cell(0, 3).character == 0);
}

TEST_CASE("terminal: a status request of bytes past 0x7F gets the invalid reply") {
	// libvterm shifted those bytes as negative chars (found by the terminal fuzzer)
	TerminalModel model(4, 20);
	model.write("\x1bP$qm\xA0\x1b\\");
	CHECK(model.takeOutput() == "\x1bP0$r\x1b\\");
}

TEST_CASE("terminal: a cursor restored after the terminal shrank is on the screen") {
	// libvterm put it back where it was saved, past the screen (found by the terminal fuzzer)
	TerminalModel model(6, 20);
	model.write("\x1b[5;12H\x1b" "7"); // ESC 7, saving it
	model.resize(2, 4);
	model.write("\x1b" "8"); // ESC 8, restoring it
	CHECK(model.cursor().row == 1);
	CHECK(model.cursor().column == 3);
	// leaving the alternate screen restores it too
	model.resize(6, 20);
	model.write("\x1b[5;12H\x1b[?1049h");
	model.resize(2, 4);
	model.write("\x1b[?1049l");
	CHECK(model.cursor().row == 1);
	CHECK(model.cursor().column == 3);
}

TEST_CASE("terminal: a long number in an escape sequence counts as a large one, and too many are dropped") {
	// libvterm's numbers overflowed (found by the terminal fuzzer): one past 5 digits takes no more
	TerminalModel model(4, 20);
	model.setAllowTitle(true);
	model.write("\x1b]" + std::string(40, '9') + "2;x\x07");
	CHECK(model.title().empty());
	model.write("\x1b[" + std::string(40, '9') + "C");
	CHECK(model.cursor().column == 19);
	model.write("\x1b[" + std::string(40, '9') + "D");
	CHECK(model.cursor().column == 0);

	// A CSI sequence's arguments past its 16th replace the 16th (libvterm wrote them past their array)
	std::string many = "\x1b[";
	for(int argument = 0; argument < 100; ++argument) many += "1;";
	model.write(many + "31mx");
	CHECK(model.lineText(0) == U"x");
	CHECK_FALSE(model.cell(0, 0).defaultForeground);
}

namespace {

/// @brief A screen with a terminal node and the actions that run, clear and close its session, and some that are wrong.
/// @return The document.
json terminalScreen() {
	const json run = {{"runInTerminal", "t1"}, {"text", "echo {{state.word}}"}};
	return {{"format", "affrescoui-screen"},
	        {"version", 2},
	        {"window", {{"title", "terminal"}, {"width", 300}, {"height", 200}}},
	        {"state", {{"word", "hello"}}},
	        {"actions",
	         {{"run", json::array({run})},
	          {"clear", json::array({{{"clearTerminal", "t1"}}})},
	          {"close", json::array({{{"closeTerminal", "t1"}}})},
	          {"notText", json::array({{{"runInTerminal", "t1"}, {"text", 5}}})},
	          {"noText", json::array({{{"runInTerminal", "t1"}}})},
	          {"noSession", json::array({{{"clearTerminal", ""}}})},
	          {"longSession", json::array({{{"closeTerminal", std::string(257, 'x')}}})}}},
	        {"root",
	         {{"type", "column"},
	          {"children", json::array({{{"type", "terminal"},
	                                     {"id", "shell"},
	                                     {"session", "t1"},
	                                     {"scrollback", 50},
	                                     {"width", 200},
	                                     {"height", 100}}})}}}};
}

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

} // namespace

TEST_CASE("terminal: the node shows a session, and the steps run, clear and close it, checking what they are given") {
	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(terminalScreen().dump());
	screen.build(*window);
	CHECK(dynamic_cast<affresco::format::TerminalView*>(screen.find("shell")) != nullptr);

	const Errors errors;
	// No program starts on a headless window: the steps still run, the line waits for one
	for(const char* name : {"run", "clear", "run", "close", "close"}) {
		CAPTURE(name);
		CHECK_NOTHROW(screen.runAction(name, window->root()));
	}
	CHECK(errors.lines.empty());

	const std::vector<std::pair<const char*, std::string>> wrong = {
	    {"notText", "actions.notText[0].text: expected a string"},
	    {"noText", "\"runInTerminal\" needs the \"text\""},
	    {"noSession", "actions.noSession[0].clearTerminal: a session's id has 1 to 256 bytes"},
	    {"longSession", "actions.longSession[0].closeTerminal: a session's id has 1 to 256 bytes"}};
	for(const auto& [name, message] : wrong) {
		CAPTURE(name);
		const std::size_t before = errors.lines.size();
		CHECK_NOTHROW(screen.runAction(name, window->root()));
		REQUIRE(errors.lines.size() == before + 1);
		CHECK(errors.lines.back().contains(message));
	}
}

TEST_CASE("terminal: the checker knows the steps and the node, and a revision can't add a step") {
	const json document = terminalScreen();
	const auto found = affresco::format::check(document, document["state"]);
	REQUIRE(found);
	for(const auto& finding : *found) CHECK_MESSAGE(!finding.text().contains("unknown"), finding.text());

	const ManualClock clock;
	const auto window = createHeadlessWindow(300, 200, clock);
	affresco::Screen screen = affresco::Screen::fromString(document.dump());
	screen.build(*window);
	const json step = {{"runInTerminal", "t1"}, {"text", "ls"}};
	const affresco::RevisionResult added =
	    screen.submitRevision(json{{"author", "agent"}, {"set", {{"/actions/sneak", json::array({step})}}}}.dump());
	CHECK(added.status == RevisionStatus::Rejected);
	CHECK(added.error.contains("permission denied"));
}
