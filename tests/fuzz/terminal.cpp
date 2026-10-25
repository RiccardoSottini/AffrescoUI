/// @file
/// @brief Fuzzes the terminal node's model (format's TerminalModel, on libvterm): a terminal shows whatever its program
/// writes, escape sequences and invalid UTF-8 included, and must keep its screen, scrollback, title and replies
/// within their bounds.
///
/// The input's first three bytes choose the terminal: its rows, its columns, and flags (bit 0 allows the title, bits
/// 1-3 the size of the pieces the rest is written in, bits 4-7 the scrollback). The rest is the program's output. Half
/// way through, the terminal is resized to its columns' and rows' sizes swapped.

#include "Fuzz.hpp"

#include "TerminalModel.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

using affresco::format::TerminalModel;
using affresco::fuzz::broken;

/// @brief Checks what the model promises, whatever was written.
/// @param model The model.
void checkModel(TerminalModel& model) {
	const affresco::format::TerminalCursor cursor = model.cursor();
	if(cursor.row < 0 || cursor.row >= model.rows() || cursor.column < 0 || cursor.column > model.columns())
		broken("the cursor is on the screen");
	if(model.scrollbackSize() > model.scrollbackLimit()) broken("the scrollback keeps at most its limit");
	if(model.title().size() > 256) broken("the title has at most 256 bytes");
	if(!model.allowTitle() && !model.title().empty()) broken("no title unless allowed");
	if(model.takeOutput().size() > affresco::format::mostPendingOutput) broken("the replies are bounded");
	const std::size_t lines = model.scrollbackSize() + static_cast<std::size_t>(model.rows());
	// The first lines and the last: the oldest of the scrollback and the screen's bottom
	for(std::size_t line = 0; line < lines; line = line == 7 && lines > 16 ? lines - 8 : line + 1) {
		if(model.lineText(line).size() > static_cast<std::size_t>(model.columns()))
			broken("a line has at most its columns' characters");
		for(int column = 0; column < model.columns(); ++column)
			if(model.cell(line, column).width > 2) broken("a cell covers at most two columns");
	}
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	std::string_view input = affresco::fuzz::text(data, size);
	if(input.size() < 3) return 0;
	const auto byte = [&input](std::size_t at) { return static_cast<unsigned char>(input[at]); };
	const int rows = 1 + byte(0) % 60, columns = 1 + byte(1) % 200;
	const unsigned flags = byte(2);
	input.remove_prefix(3);

	TerminalModel model(rows, columns, (flags >> 4u) * 3u);
	model.setAllowTitle((flags & 1u) != 0);
	const std::size_t piece = 1 + ((flags >> 1u) & 7u) * 13;
	const std::size_t half = input.size() / 2;
	bool resized = false;
	for(std::size_t at = 0; at < input.size(); at += piece) {
		if(!resized && at >= half) {
			model.resize(columns, rows);
			resized = true;
			checkModel(model);
		}
		model.write(input.substr(at, std::min(piece, input.size() - at)));
		checkModel(model);
	}
	return 0;
}
