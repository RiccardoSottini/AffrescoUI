/// @file
/// @brief Fuzzes the Lisp notation (modules/notation): its reader on any text, and its writer on any JSON value.
///
/// Any text either reads or throws a NotationError whose message stays short, and what reads nests at most maxNesting
/// levels. What reads is written as text that reads back as the same value, key order included, and is canonical:
/// writing it again gives the same text. Text that is JSON is also written in the Lisp notation and read back, the
/// same. Formatting a text keeps its tree and every comment, and formatting again changes nothing.

#include "Fuzz.hpp"

#include <affresco/notation/Notation.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

using affresco::json;
using affresco::notation::Notation;

namespace {

/// Longest error message the reader gives, without its place: what it quotes of the text is cut short.
constexpr std::size_t maxMessage = 256;

/// @brief Writes a value in the Lisp notation and checks that it reads back the same, and is written the same again.
/// @param value The value, at most maxNesting deep.
void roundTrips(const json& value) {
	const std::string text = affresco::notation::write(value, Notation::Lisp);
	json back;
	try {
		back = affresco::notation::read(text, Notation::Lisp);
	} catch(const affresco::notation::NotationError&) {
		affresco::fuzz::broken("what the writer writes reads back");
	}
	if(back.dump() != value.dump()) affresco::fuzz::broken("a value reads back from its text the same, key order too");
	if(affresco::notation::write(back, Notation::Lisp) != text) affresco::fuzz::broken("the writer is canonical");
}

/// @brief Formats a text that reads: the tree is the same, a comment stays in the text, and twice is once.
/// @param text The text.
/// @param value What it reads as.
void formats(std::string_view text, const json& value) {
	const std::string once = affresco::notation::format(text);
	if(affresco::notation::read(once, Notation::Lisp).dump() != value.dump())
		affresco::fuzz::broken("formatting keeps the tree");
	if(affresco::notation::format(once) != once) affresco::fuzz::broken("formatting twice is formatting once");
	if(text.contains(';') && !text.contains('"') && !text.contains("#_") && !once.contains(';'))
		affresco::fuzz::broken("formatting keeps comments");
}

} // namespace

/// @brief libFuzzer's entry point: reads the input as Lisp, and as JSON, and writes what reads.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string_view text = affresco::fuzz::text(data, size);
	try {
		const json value = affresco::notation::read(text, Notation::Lisp);
		if(affresco::nestingDepth(value) > affresco::maxNesting)
			affresco::fuzz::broken("the reader keeps to maxNesting");
		roundTrips(value);
		formats(text, value);
	} catch(const affresco::notation::NotationError& error) { // refusing it is the rule, in a few words
		if(error.message().size() > maxMessage) affresco::fuzz::broken("an error quotes only the start of the text");
	}
	const json value = affresco::fuzz::untrusted(text);
	if(!value.is_discarded()) roundTrips(value);
	return 0;
}
