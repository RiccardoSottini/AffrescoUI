#pragma once

/// @file
/// @brief A program: named definitions, each kept as its exact text, and the pure build that makes them a JSON tree
/// (README.md, RFC 0005).

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::lisp {

/// @brief The names a definition's code calls with a text written out first (`(run "save")`), and those texts.
struct CallsByName {
	std::map<std::string, std::vector<std::string>> texts; ///< The texts each name is called with, sorted.
	std::set<std::string>
	    otherwise; ///< Those of the names also used another way: as a value, a computed first argument.
};

/// @brief By a definition's address, what it calls with a text written out first (Program::namedCalls()).
using NamedCalls = std::map<std::string, CallsByName>;

/// @brief One top-level form of a program.
struct Entry {
	std::string file;   ///< The file it is in, as the program calls it.
	std::string name;   ///< Its stable address: its definition's name; `defkey ctrl+k` and `add-hook after-save f` for
	                    ///< forms that name no definition.
	std::string kind;   ///< The form's head: `defn`, `defcommand`...
	std::string text;   ///< Its exact text, with the comment above it and the one that ends its line.
	std::string author; ///< Who wrote it; "" until the program is given one.
	std::size_t line = 1;   ///< The line its text starts on.
	std::size_t column = 1; ///< The column its text starts on.
};

/// @brief Splits a program's text into its top-level forms, each with its exact text. Concatenating the entries' texts
/// with the white space between them gives the file back: all a file's comments are in some entry's text; those after
/// the last form are the last entry's.
/// @param text The text.
/// @param file Its name.
/// @param author Who wrote it.
/// @return The entries, in order. No text gives no entry.
/// @throws LispError If it can't be read, or a form names no definition.
[[nodiscard]] std::vector<Entry> splitEntries(std::string_view text, const std::string& file,
                                              const std::string& author = "");

/// @brief Tells a program from a file holding one data value: a program has several top-level forms, or one that is a
/// definition (`defn`, `defscreen`...).
/// @param text The text.
/// @return true if it is a program.
/// @throws LispError If it can't be read.
[[nodiscard]] bool isProgram(std::string_view text);

/// @brief What in a text needs running, if anything, for a tool that treats files as data (`affrescoui convert`,
/// `fmt`).
/// @param text The text.
/// @return The first definition's name, or the place of the first `~`; std::nullopt for plain data, and for text that
/// can't be read, which the notation's reader reports.
[[nodiscard]] std::optional<std::string> needsRunning(std::string_view text);

/// @brief A file's built tree. A data file is read as if backquoted: words stay data, `~` runs, and the tree is exactly
/// what the notation's reader gives, but that a word the language runs (notation::isLanguageWord()) isn't a node's type
/// there: `(not x)` is an error saying to write `~(not ...)`, or `(node :type not ...)` for a node of that type.
/// A program is split and built (Program::build()). Build code is pure.
/// @param text The text.
/// @param file Its name, for errors.
/// @param limits The limits. A data file also gets as much budget as its text's size.
/// @return The tree.
/// @throws LispError If it can't be read or built, naming the place.
[[nodiscard]] json buildText(std::string_view text, const std::string& file = "", const Limits& limits = {});

/// @brief A form in a text a person or a program has in hand.
struct FormAt {
	std::string text;          ///< Its exact text.
	std::size_t line = 1;      ///< The line it starts on, from 1.
	std::size_t column = 1;    ///< The column it starts on, from 1, as a text editor counts: in characters.
	std::size_t endLine = 1;   ///< The line it ends on.
	std::size_t endColumn = 1; ///< The column just past its end.
	std::string kind;          ///< Its head, if it is a definition form; else "".
	std::string name;          ///< Its address, if it is a definition form; else "".
};

/// @brief The form that ends at or before a position, however deep: what "evaluate the last form" reads. A text that
/// goes on badly further down doesn't matter; a word, a string or a list all count.
/// @param text The text.
/// @param line The position's line, from 1.
/// @param column Its column, from 1, in characters.
/// @return The form, or std::nullopt if none ends there.
[[nodiscard]] std::optional<FormAt> formBefore(std::string_view text, std::size_t line, std::size_t column);

/// @brief The top-level form around a position, or else the last one before it, or else the next: what "evaluate this
/// definition" reads.
/// @param text The text.
/// @param line The position's line, from 1.
/// @param column Its column, from 1, in characters.
/// @return The form, or std::nullopt if there is none or the one around it isn't finished.
[[nodiscard]] std::optional<FormAt> formAround(std::string_view text, std::size_t line, std::size_t column);

/// @brief A text's top-level forms, each with its exact text: "evaluate this region".
/// @param text The text.
/// @param file Its name, for errors.
/// @return The forms. Those that aren't definition forms have an empty FormAt::kind.
/// @throws LispError If it can't be read.
[[nodiscard]] std::vector<FormAt> formsOf(std::string_view text, const std::string& file = "");

/// @brief A name a definition's code uses that nothing defines. Not an error until it runs: a function's body may use
/// what the host gives only when the app runs.
struct UndefinedName {
	std::string name;       ///< The name.
	std::string file;       ///< Where it is used.
	std::size_t line = 0;   ///< Its line.
	std::size_t column = 0; ///< Its column.
	std::string definition; ///< The definition it is in.
};

/// @brief A program: its entries, in the order given. Order doesn't matter to what it means: macros are expanded first,
/// and the sections it builds are sorted by name, so moving a definition changes nothing.
class Program {
public:
	/// @brief Adds a file's entries (splitEntries()).
	/// @param text Its text.
	/// @param file Its name.
	/// @param author Who wrote it.
	/// @throws LispError If it can't be read, or a name is taken, naming both places.
	void addFile(std::string_view text, const std::string& file, const std::string& author = "");
	/// @brief Adds one entry, as a document's `program` section holds them. Its text must be one form with that name.
	/// @param entry The entry.
	/// @throws LispError If it isn't, or the name is taken, naming both places.
	void addEntry(Entry entry);
	/// @brief The entries, in the order added. @return Them.
	[[nodiscard]] const std::vector<Entry>& entries() const noexcept { return entries_; }

	/// @brief Gives build code a host's pure primitive.
	/// @param name Its name.
	/// @param primitive It; Primitive::pure must be set and Primitive::capability empty.
	/// @throws std::invalid_argument If it isn't pure.
	void definePure(const std::string& name, Primitive primitive);

	/// @brief Loads the definitions into an interpreter: macros first, then the rest, each compiled; a `def`'s value is
	/// worked out when first read. Nothing is built, and the interpreter's own primitives, gate and author checks apply
	/// when functions run. Definitions replace what the interpreter had of those names.
	/// @param interpreter The interpreter.
	/// @throws LispError If a definition can't be compiled.
	void load(Interpreter& interpreter) const;

	/// @brief Builds the tree, in a fresh interpreter with no way out but the pure primitives (definePure()): the same
	/// source always gives the same tree. Every `def` and `defcustom` is worked out here, used or not. `format` and
	/// `version` come first, then `defscreen main`'s properties and `root`, then `screens`, `actions`, `keys` and
	/// `settings`, each sorted by name. Records what extensions() and undefinedNames() say.
	/// @param limits The limits.
	/// @return The tree.
	/// @throws LispError If building fails or a limit runs out, naming the place.
	[[nodiscard]] json build(const Limits& limits = {});

	/// @brief The `add-hook`, `remove-hook` and `advice` definitions of the last build. @return Them, in order of the
	/// entries.
	[[nodiscard]] const std::vector<Extension>& extensions() const noexcept { return extensions_; }
	/// @brief The names the last build's definitions use that nothing defines. @return Them, in order of use.
	[[nodiscard]] const std::vector<UndefinedName>& undefinedNames() const noexcept { return undefined_; }
	/// @brief The names each definition's code uses, macros' output and the functions inside it included: nothing runs
	/// but what a name reaches, as the language has no way to find a global from a value. @return Them, by its address.
	[[nodiscard]] const std::map<std::string, std::vector<std::string>>& namesUsed() const noexcept { return used_; }
	/// @brief Of the names each definition uses (namesUsed()), those it calls with a text written out first, with those
	/// texts: `(run "save")` and `(run :tidy)` give `{"run": ["save", "tidy"]}`; and which of them it also uses another
	/// way (as a value, through `apply`, with a computed first argument). A host may follow what the texts name where
	/// a name is used no other way, and count the rest as reaching anything. @return Them, by its address.
	[[nodiscard]] const NamedCalls& namedCalls() const noexcept { return named_; }

private:
	std::vector<Entry> entries_;                                ///< See entries().
	std::map<std::string, std::size_t> names_;                  ///< Where each entry is in entries_, by its name.
	std::vector<std::pair<std::string, Primitive>> primitives_; ///< See definePure().
	std::vector<Extension> extensions_;                         ///< See extensions().
	std::vector<UndefinedName> undefined_;                      ///< See undefinedNames().
	std::map<std::string, std::vector<std::string>> used_;      ///< See namesUsed().
	NamedCalls named_;                                          ///< See namedCalls().
};

} // namespace affresco::lisp
