/// @file
/// @brief Fuzzes the text model (render's TextModel): an editor's text comes from files, peers and revisions, and its
/// edits, undo and redo must keep the piece tree, the line index and the history consistent.
///
/// The input is a list of commands, each a byte and its arguments: 0 inserts the bytes that follow (a line byte, a
/// column byte, a length byte, then the bytes, which may be invalid UTF-8), 1 erases between two positions, 2 undoes,
/// 3 redoes, 4 inserts outside an undo step (which forgets the history). Bit 3 of an insert merges it with the step
/// before. After every command the text must be valid UTF-8 whose lines and size match the line index, and at the end
/// undoing every step and redoing them all must give the same text.

#include "Fuzz.hpp"

#include <affresco/render/TextModel.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

/// @brief Reads the input a byte at a time; 0 once it runs out.
struct Reader {
	std::string_view input; ///< What is left.

	/// @brief The next byte.
	unsigned byte() noexcept {
		if(input.empty()) return 0;
		const auto value = static_cast<unsigned char>(input.front());
		input.remove_prefix(1);
		return value;
	}
	/// @brief The next bytes, fewer if the input ends first.
	/// @param count How many.
	std::string_view bytes(std::size_t count) noexcept {
		const std::string_view out = input.substr(0, count);
		input.remove_prefix(out.size());
		return out;
	}
	/// @brief A position in a model: a line byte (modulo its lines) and a column byte (which may be past the end).
	/// @param model The model.
	affresco::TextPosition position(const affresco::TextModel& model) noexcept {
		const std::size_t line = byte() % model.lineCount();
		return {line, byte()};
	}
};

/// @brief Whether text is valid UTF-8 (no overlong forms, surrogates or code points past U+10FFFF).
/// @param text The text.
bool validUtf8(std::string_view text) noexcept {
	for(std::size_t at = 0; at < text.size();) {
		const auto lead = static_cast<unsigned char>(text[at]);
		const std::size_t length = lead < 0x80 ? 1 : lead >= 0xC2 && lead < 0xE0 ? 2 : lead >= 0xE0 && lead < 0xF0 ? 3
		                                            : lead >= 0xF0 && lead < 0xF5 ? 4 : 0;
		if(length == 0 || at + length > text.size()) return false;
		char32_t codepoint = length == 1 ? lead : lead & (0x7Fu >> length);
		for(std::size_t i = 1; i < length; ++i) {
			const auto next = static_cast<unsigned char>(text[at + i]);
			if((next & 0xC0u) != 0x80u) return false;
			codepoint = (codepoint << 6) | (next & 0x3Fu);
		}
		const char32_t least = length == 2 ? 0x80 : length == 3 ? 0x800 : length == 4 ? 0x10000 : 0;
		if(codepoint < least || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint < 0xE000)) return false;
		at += length;
	}
	return true;
}

/// @brief Checks that the text, its size, its lines and their lengths agree, and that a position survives a round
/// trip through its offset.
/// @param model The model.
/// @param probe A position to check.
void checkModel(const affresco::TextModel& model, affresco::TextPosition probe) {
	const std::string text = model.text();
	if(text.size() != model.size()) affresco::fuzz::broken("the text is size() bytes");
	if(!validUtf8(text)) affresco::fuzz::broken("the text is valid UTF-8");
	if(static_cast<std::size_t>(std::ranges::count(text, '\n')) + 1 != model.lineCount())
		affresco::fuzz::broken("the lines are the line feeds plus one");
	std::size_t codepoints = model.lineCount() - 1;
	for(std::size_t line = 0; line < model.lineCount(); ++line) codepoints += model.lineLength(line);
	if(codepoints != static_cast<std::size_t>(std::ranges::count_if(text, [](char c) { return (c & 0xC0) != 0x80; })))
		affresco::fuzz::broken("the lines' lengths add up to the text's code points");
	const affresco::TextPosition expected{probe.line, std::min(probe.column, model.lineLength(probe.line))};
	if(model.positionOf(model.offsetOf(probe)) != expected) affresco::fuzz::broken("positions survive their offsets");
}

/// @brief Runs one command.
/// @param model The model.
/// @param reader The input.
/// @return A position the command used, to check.
affresco::TextPosition run(affresco::TextModel& model, Reader& reader) {
	const unsigned command = reader.byte();
	affresco::TextPosition caret{0, 0}, anchor{0, 0};
	switch(command % 5) {
	case 0:
	case 4: {
		const affresco::TextPosition at = reader.position(model);
		const std::string_view text = reader.bytes(reader.byte() % 32);
		if(command % 5 == 0) model.beginStep((command & 8u) != 0, at, at);
		model.insert(at, text);
		model.endStep();
		return at;
	}
	case 1: {
		const affresco::TextPosition from = reader.position(model);
		const affresco::TextPosition to = std::max(from, reader.position(model));
		model.beginStep(false, from, to);
		model.erase(from, to);
		model.endStep();
		return from;
	}
	default: model.undo(command % 5 == 3, caret, anchor); return caret;
	}
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	Reader reader{affresco::fuzz::text(data, size)};
	affresco::TextModel model;
	while(!reader.input.empty()) {
		affresco::TextPosition probe = run(model, reader);
		probe.line = std::min(probe.line, model.lineCount() - 1);
		checkModel(model, probe);
	}

	// Undo every step, then redo them all: the same text
	const std::string text = model.text();
	affresco::TextPosition caret{0, 0}, anchor{0, 0};
	std::size_t undone = 0;
	while(model.undo(false, caret, anchor)) ++undone;
	for(std::size_t step = 0; step < undone; ++step)
		if(!model.undo(true, caret, anchor)) affresco::fuzz::broken("every step undone can be redone");
	if(!model.equals(text)) affresco::fuzz::broken("undoing and redoing every step gives the same text");
	return 0;
}
