#pragma once

/// @file
/// @brief The language's functions and macros written in itself, as a table (README.md, "The library"): each entry is
/// registered as a global when the interpreter starts, and read and compiled the first time something uses it. Its
/// code sees the language's own functions, never a program's of the same name, however late it is compiled.

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace affresco::lisp {

class Machine;
struct Global;

/// @brief A function or a macro of the language, written in the language.
struct LibraryEntry {
	std::string_view name;       ///< Its name, the global it defines.
	std::string_view source;     ///< One `(defn name ...)` or `(defmacro name ...)` form.
	bool macro = false;          ///< A macro of the language: `source` is a `defmacro`.
	bool takesFunctions = false; ///< A word given to it that names a function is that function, in a binding.
	/// For a macro, the functions its expansion names by word, separated by spaces: a local or a program's definition
	/// of one of them where the macro is called is an error, since the expansion would call that instead. Allowed:
	/// `not`, `=`, `nil?`, `some?`, `map`, `filter`, `mapcat`, written by word so that the expansion lowers to a
	/// binding the expression machine runs.
	std::string_view uses = {};
};

/// @brief Registers entries: each a global of the language from now on, compiled on first use.
/// @param machine The machine; the entries' texts must outlive it (they are constants).
/// @param entries The entries.
void defineLibrary(Machine& machine, std::span<const LibraryEntry> entries);

/// @brief Why a call of a library macro is refused where the program defines a word the macro's expansion calls: the
/// expansion would call the program's.
/// @param machine The machine.
/// @param macro The macro.
/// @return The message, or an empty string when the program defines none of them.
[[nodiscard]] std::string definedUse(const Machine& machine, const Global& macro);

/// @brief The words of a macro entry's `uses`, one at a time.
/// @tparam Each What to call with each word.
/// @param uses The text.
/// @param each What to call.
template <class Each> void forEachUse(std::string_view uses, const Each& each) {
	while(!uses.empty()) {
		const std::size_t space = uses.find(' ');
		if(const std::string_view word = uses.substr(0, space); !word.empty()) each(word);
		if(space == std::string_view::npos) return;
		uses.remove_prefix(space + 1);
	}
}

} // namespace affresco::lisp
