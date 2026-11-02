#pragma once

/// @file
/// @brief A program: named definitions, each kept as its exact text, and the pure build that makes them a JSON tree
/// (README.md, RFC 0005).

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::lisp {

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

/// @brief A definition that doesn't build anything itself, recorded for a later stage: `add-hook` or `advice`.
struct Extension {
	std::string kind;                 ///< `add-hook` or `advice`.
	std::string name;                 ///< Its address, as the entry's.
	std::vector<std::string> targets; ///< Its arguments, as written, in order: `after-save`, `notify-saved`.
	std::string doc;                  ///< Its docstring, or "".
	std::string author;               ///< Its author.
};

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
	/// source always gives the same tree. `format` and `version` come first, then `defscreen main`'s properties and
	/// `root`, then `screens`, `actions`, `keys` and `settings`, each sorted by name. Records what extensions() and
	/// undefinedNames() say.
	/// @param limits The limits.
	/// @return The tree.
	/// @throws LispError If building fails or a limit runs out, naming the place.
	[[nodiscard]] json build(const Limits& limits = {});

	/// @brief The `add-hook` and `advice` definitions of the last build. @return Them, in order of the entries.
	[[nodiscard]] const std::vector<Extension>& extensions() const noexcept { return extensions_; }
	/// @brief The names the last build's definitions use that nothing defines. @return Them, in order of use.
	[[nodiscard]] const std::vector<UndefinedName>& undefinedNames() const noexcept { return undefined_; }

private:
	std::vector<Entry> entries_;                                ///< See entries().
	std::map<std::string, std::size_t> names_;                  ///< Where each entry is in entries_, by its name.
	std::vector<std::pair<std::string, Primitive>> primitives_; ///< See definePure().
	std::vector<Extension> extensions_;                         ///< See extensions().
	std::vector<UndefinedName> undefined_;                      ///< See undefinedNames().
};

} // namespace affresco::lisp
