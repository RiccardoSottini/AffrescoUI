#pragma once

/// @file
/// @brief The namespaces a qualified word names (README.md, "Qualified names"): `str/join` is the `join` of
/// `clojure.string`, through its alias `str`. A bare namespace's functions are globals of their own names
/// (`clojure.core/inc` is `inc`); a prefixed one's are globals named with the namespace (`clojure.string/join`).
///
/// A program file's `(ns name (:require ...))` form (reference/namespaces.md) labels the file and gives it aliases of
/// its own: names stay in one flat space, so a namespace of the program's files is bare, and `board/render` is the
/// global `render`, checked against the file that declares the namespace.

#include <affresco/lisp/Value.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::lisp {

class Machine;

/// Aliases by name, to the namespace each stands for.
using AliasMap = std::map<std::string, std::string, std::less<>>;

/// @brief A namespace of the program's files: the file that declares it, and what that file defines.
struct ProgramSpace {
	std::string file;                         ///< The file, as the program calls it.
	std::set<std::string, std::less<>> names; ///< The addresses of its definitions.
};

/// @brief A namespace a prefix names.
struct NamespaceFound {
	std::string_view name;                 ///< Its full name.
	bool bare = false;                     ///< Its functions are globals of their own names.
	const ProgramSpace* program = nullptr; ///< A file of the program's namespace, or nullptr for the language's.
};

/// @brief The namespaces, and the aliases for them.
class Namespaces {
public:
	/// @brief The language's: `clojure.core` (bare), `clojure.string` and `clojure.set`; `str` and `string` stand for
	/// `clojure.string`, `set` for `clojure.set`.
	Namespaces();

	/// @brief Adds a namespace, or changes whether one is bare.
	/// @param name Its name.
	/// @param bare Whether its functions are globals of their own names.
	void add(std::string name, bool bare);
	/// @brief Adds an alias, or points one elsewhere.
	/// @param alias The alias.
	/// @param name The namespace.
	/// @return false if no namespace has that name.
	bool alias(std::string alias, std::string_view name);
	/// @brief The namespace a prefix names: through the aliases given, then these, then by its own name. A namespace of
	/// the program's files is found only through the aliases given: only the file that requires it names it.
	/// @param prefix The prefix.
	/// @param aliases A file's own aliases, or nullptr.
	/// @return It; an empty name if none.
	[[nodiscard]] NamespaceFound find(std::string_view prefix, const AliasMap* aliases) const;
	/// @brief Forgets the program's namespaces, before a program is loaded again. The files' alias tables stay where
	/// they are, emptied: code compiled earlier keeps a pointer to its file's.
	void clearProgram();
	/// @brief Whether no program's namespace nor file alias is known: nothing for clearProgram() to forget.
	/// @return true if so.
	[[nodiscard]] bool programless() const noexcept { return programSpaces_.empty() && fileAliases_.empty(); }
	/// @brief Adds a namespace of the program's files.
	/// @param name Its name.
	/// @param space Its file and what that file defines.
	void addProgramSpace(std::string name, ProgramSpace space);
	/// @brief A namespace of the program's files.
	/// @param name Its name.
	/// @return It, or nullptr.
	[[nodiscard]] const ProgramSpace* programSpace(std::string_view name) const;
	/// @brief Sets a file's own aliases (its `ns` form's).
	/// @param file The file, as the program calls it ("" for the command line's).
	/// @param aliases Its aliases.
	void setFileAliases(const std::string& file, AliasMap aliases);
	/// @brief A file's own aliases.
	/// @param file The file, or nullptr for none.
	/// @return Them, or nullptr if the file has no `ns` form. The pointer lives as long as these namespaces.
	[[nodiscard]] const AliasMap* fileAliases(const std::string* file) const;

private:
	std::map<std::string, bool, std::less<>> spaces_;                ///< Whether each is bare, by name.
	AliasMap aliases_;                                               ///< See alias().
	std::map<std::string, ProgramSpace, std::less<>> programSpaces_; ///< See addProgramSpace().
	std::map<std::string, AliasMap, std::less<>> fileAliases_;       ///< See setFileAliases().
};

/// @brief Tells whether a word is qualified: a '/' that is neither its first character nor its last (`/` is
/// division, and `a/` a word of its own).
/// @param word The word.
/// @return true if it is; the namespace is what comes before its first '/'.
[[nodiscard]] constexpr bool isQualified(std::string_view word) noexcept {
	const std::size_t slash = word.find('/');
	return slash != std::string_view::npos && slash > 0 && slash + 1 < word.size();
}

/// @brief One spec of an `ns` form's `(:require ...)`.
struct Required {
	std::string name;                ///< The namespace as written; "" when a string names the file.
	std::string path;                ///< The program's file it names, relative to the requiring file; "" if built in.
	std::string alias;               ///< Its alias, or "".
	std::vector<std::string> refers; ///< The words its `:refer` names.
	std::uint32_t line = 0;          ///< Where it is written.
	std::uint32_t column = 0;        ///< Where it is written.
};

/// @brief What a file's `ns` form says.
struct NamespaceDeclaration {
	std::string name;               ///< The file's namespace.
	std::vector<Required> required; ///< What it requires, in the order written.
};

/// The language's namespaces, which a file requires without a file of the program.
inline constexpr std::array<std::string_view, 3> languageNamespaces = {"clojure.core", "clojure.set", "clojure.string"};

/// @brief Reads an `ns` form: `(ns name "doc"? (:require spec...)...)`.
/// @param form The form, its head `ns`.
/// @param file The file it is in, for errors.
/// @return What it says.
/// @throws LispError Naming the place, for anything else.
[[nodiscard]] NamespaceDeclaration readNamespaceForm(const Value& form, const std::string& file);

/// @brief The program's file a dotted name requires, relative to the requiring file: its first segment (the app's)
/// dropped when it has two or more, the rest the path from the folder the requiring file's own name implies
/// (`flow.parts.board` is `parts/board.lisp`; from `flow.parts.cell`, `../parts/board.lisp`).
/// @param name The namespace required.
/// @param own The requiring file's namespace.
/// @return The path.
[[nodiscard]] std::string namespacePath(std::string_view name, std::string_view own);

/// @brief Gives a program's files their namespaces and their aliases, before the program's definitions are compiled:
/// each file's table goes to Namespaces::setFileAliases(). Checks what the program can check: a file required by a
/// dotted name declares that name; a `:refer` names what its namespace has as the plain word.
/// @param machine The machine.
/// @param declared The files' `ns` forms, as file and text (Program::namespaceForms()).
/// @param definedIn Each file's definitions' addresses, by file.
/// @throws LispError Naming the place, for what doesn't check.
void applyNamespaces(Machine& machine, std::span<const std::pair<std::string, std::string>> declared,
                     const std::map<std::string, std::set<std::string, std::less<>>, std::less<>>& definedIn);

/// @brief Sets the aliases of the forms a person types after an `ns` form at the command line (Interpreter::eval()):
/// it names the language's namespaces only, and loads no file.
/// @param machine The machine.
/// @param form The form.
/// @param file What the forms are read as.
/// @throws LispError Naming the place, for a file required or anything else that doesn't check.
void declareNamespace(Machine& machine, const Value& form, const std::string& file);

} // namespace affresco::lisp
