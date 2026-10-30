/// @file
/// @brief Fuzzes the Lisp notation (modules/notation): its reader on any text, and its writer on any JSON value.
///
/// Any text either reads or throws a NotationError, and what reads nests at most maxNesting levels. What reads is
/// written as text that reads back as the same value, key order included, and is canonical: writing it again gives
/// the same text. Text that is JSON is also written in the Lisp notation and read back, the same.

#include "Fuzz.hpp"

#include <affresco/notation/Notation.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

using affresco::json;
using affresco::notation::Notation;

namespace {

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
	} catch(const affresco::notation::NotationError&) { // NOLINT(bugprone-empty-catch): refusing it is the rule
	}
	const json value = affresco::fuzz::untrusted(text);
	if(!value.is_discarded()) roundTrips(value);
	return 0;
}
