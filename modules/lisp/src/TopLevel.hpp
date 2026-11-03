#pragma once

/// @file
/// @brief What running a top-level form takes, for the interpreter and programs.

#include "Machine.hpp"
#include "Reader.hpp"

#include <optional>
#include <string>

namespace affresco::lisp {

/// @brief Runs something, so that a Problem from outside any run (compiling, a walk over a value) leaves as a
/// LispError.
/// @tparam Function What to run.
/// @param machine The machine.
/// @param function What to run.
/// @return What it returns.
template <class Function> auto leavingAsLispError(Machine& machine, const Function& function) {
	try {
		return function();
	} catch(const Problem& problem) {
		const auto [place, definition] = machine.currentPlace();
		throw LispError(place.file ? *place.file : "", place.line, place.column, definition, problem.message,
		                problem.exhausted);
	}
}

/// @brief Reads the next form: reading is bounded by the text's size, so it spends nothing.
/// @param heap The heap.
/// @param reader The reader.
/// @param budget What the heap spends otherwise.
/// @return The form, or std::nullopt at the end.
std::optional<ReadForm> readUncharged(Heap& heap, Reader& reader, Budget& budget);

/// @brief Runs a top-level form: a definition takes effect (and may replace another), `do` runs its forms in turn, a
/// macro call runs what it expands to, and anything else is run for its value.
/// @param machine The machine.
/// @param form The form.
/// @param author Who wrote it.
/// @param where The closest place known: what a macro gives may have none.
/// @return The value: a definition's is its name, as a symbol.
Value evalTopLevel(Machine& machine, const Value& form, const std::string& author, Place where);

/// @brief Defines what a program's top-level form stands for: a definition, or a macro call or a `do` that gives some.
/// Nothing runs but the macros.
/// @param machine The machine.
/// @param form The form.
/// @param author Who wrote it.
/// @param authors Its author and those of the macros it came through.
/// @param where The closest place known: what a macro gives may have none.
/// @throws LispError If it is neither, or a name is taken.
void defineTopLevel(Machine& machine, const Value& form, const std::string& author, const AuthorSet* authors,
                    Place where);

/// @brief isProgram() by tokens alone, without making the forms: a data file's one form is read once, by the notation's
/// reader, not twice more to find it is one.
/// @param text The text.
/// @return Whether it is a program, or std::nullopt if the text doesn't lex (isProgram() says what is wrong).
[[nodiscard]] std::optional<bool> isProgramByTokens(std::string_view text);

/// @brief The place of the first `~`, or a quote, in a text, by tokens (needsRunning() for a text that isn't a program).
/// @param text The text.
/// @return Its place, as needsRunning() says it, or std::nullopt.
[[nodiscard]] std::optional<std::string> firstUnquote(std::string_view text);

} // namespace affresco::lisp
