/// @file
/// @brief Forge's terminal model (libvterm): text, colours, the cursor, the bounded scrollback, a resize, the keys'
/// bytes, wide characters, and escape sequences kept inside it (the title only when allowed, no clipboard writes).

#include "features/terminal/TerminalModel.hpp"

#include <doctest/doctest.h>

#include <string>

using forge::TerminalCell;
using forge::TerminalModel;

TEST_CASE("forge terminal: text, colours and the cursor") {
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

TEST_CASE("forge terminal: the scrollback is bounded, and lines come back when it grows taller") {
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
	CHECK(model.columns() == forge::mostTerminalCells);
	model.write("\x1b[3J");
	CHECK(model.scrollbackSize() == 0);
}

TEST_CASE("forge terminal: keys become the bytes a program reads") {
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

TEST_CASE("forge terminal: the title only when allowed, and clipboard writes are dropped") {
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
}

TEST_CASE("forge terminal: a wide character covers two columns") {
	TerminalModel model(2, 10);
	model.write("\xE4\xB8\xADx");
	CHECK(model.cell(0, 0).character == U'中');
	CHECK(model.cell(0, 0).width == 2);
	CHECK(model.cell(0, 1).width == 0);
	CHECK(model.cell(0, 2).character == U'x');
	CHECK(model.lineText(0) == U"中x");
}
