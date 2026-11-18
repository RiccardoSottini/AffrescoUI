#pragma once

/// @file
/// @brief A program: named definitions, each kept as its exact text, and the pure build that makes them a JSON tree
/// (README.md, RFC 0005).

#include <affresco/core/Json.hpp>
#include <affresco/lisp/Interpreter.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
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

class Machine;          // the interpreter's state (Machine.hpp)
struct DefinitionBuilt; // what a build keeps of one definition (Rebuild.hpp)
struct NodeNote;        // what a recording build notes as it makes a node (Machine.hpp)
struct BuildRecord;     // what a build keeps of each definition, for the build of a later version (Rebuild.hpp)

/// @brief Where a recording build (Program::recordOrigins()) made a node with an id, from a form of the source: the
/// places of the node's form and of its id's text, as the program's entries were read. They differ when a call hands
/// the id to the view that makes the node (`(tool-button "save")`); a computed id has no place of its own.
struct NodePlace {
	std::string id;             ///< The node's id.
	std::string file;           ///< The file its form is in, as the program calls it.
	std::uint32_t line = 0;     ///< The line its form starts on, from 1, as the entries' start lines count it.
	std::uint32_t column = 0;   ///< The column of its '(', from 1, in characters.
	std::string idFile;         ///< The file its id's text is in.
	std::uint32_t idLine = 0;   ///< The line its id's text is on.
	std::uint32_t idColumn = 0; ///< The column its id's text starts at.
};

/// The address under which a file's `(ns ...)` form is kept with its entries (reference/namespaces.md): no definition
/// has that name, and no revision may add, change or remove it (checkNamespacesKept()).
inline constexpr std::string_view namespaceEntry = "ns";

/// @brief A program: its entries, in the order given. What a definition means doesn't depend on where it is (macros are
/// expanded first, and a definition may use one below it), but the members of a section it builds come in the order of
/// their definitions, as a JSON file's come in the order written: changing a definition never moves another.
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
	/// @brief Adds one entry as addEntry() does, its text read only by load() and build(), which refuse it then as
	/// addEntry() would: for a document's section that is loaded or built next (programOf()), read once, not twice.
	/// @param entry The entry.
	/// @throws LispError If the name is taken, naming both places.
	void addEntryUnread(Entry entry);
	/// @brief The entries, in the order added. @return Them.
	[[nodiscard]] const std::vector<Entry>& entries() const noexcept { return entries_; }
	/// @brief Gives a file its `(ns ...)` form, as the loader read it with the file (readDocument()): its namespace,
	/// and the aliases its definitions are compiled with. load() and build() read it.
	/// @param file The file, as the program calls it.
	/// @param text The form's text, which reads from line 1, column 1.
	/// @throws LispError If the file has one already.
	void declareNamespace(const std::string& file, std::string text);
	/// @brief The files' `(ns ...)` forms, as file and text, in the order given. @return Them.
	[[nodiscard]] const std::vector<std::pair<std::string, std::string>>& namespaceForms() const noexcept {
		return namespaces_;
	}

	/// @brief Gives build code a host's pure primitive.
	/// @param name Its name.
	/// @param primitive It; Primitive::pure must be set and Primitive::capability empty.
	/// @throws std::invalid_argument If it isn't pure.
	void definePure(const std::string& name, Primitive primitive);

	/// @brief Loads the definitions into an interpreter: macros first, then the rest, each compiled; a `def`'s value is
	/// worked out when first read. Nothing is built, and the interpreter's own primitives, gate and author checks apply
	/// when functions run. Definitions replace what the interpreter had of those names.
	/// @param interpreter The interpreter.
	/// @param everything Compile every definition now; else only what runs, functions and macros: a view, a component
	/// and what a definition builds are compiled when called, as a host that runs the program and never builds it
	/// needs (the build compiled all of them, and refused what doesn't compile).
	/// @throws LispError If a definition can't be compiled.
	void load(Interpreter& interpreter, bool everything = true) const;

	/// @brief Builds the tree, in a fresh interpreter with no way out but the pure primitives (definePure()): the same
	/// source always gives the same tree. Every `def` and `defcustom` is worked out here, used or not. `format` and
	/// `version` come first, then `defscreen main`'s properties and `root`, then `screens`, `actions`, `keys` and
	/// `settings`, then the others, each section's members in the order of their definitions (a `defdefaults` member
	/// where its definition is, its value replaced there by a definition that gives it). Records what extensions() and
	/// undefinedNames() say.
	/// @param limits The limits.
	/// @return The tree.
	/// @throws LispError If building fails or a limit runs out, naming the place.
	[[nodiscard]] json build(const Limits& limits = {});

	/// @brief Builds the tree as build() does, from the record of a build of an earlier version of the program: only
	/// the definitions that changed, and those that use them, are run; what the others gave is taken from the record.
	/// It gives what build() gives, and refuses what build() refuses with build()'s message, as it builds in full where
	/// it can't be sure: a changed or removed macro, hook or advice, a definition named as one of the language's (a
	/// macro of the language may call it), a form that isn't a definition (a macro's call at
	/// the top level, `do`), a file's `ns` form added, removed, moved or changed (with the same forms, names resolve as
	/// they did), a recording build, a definition that would use one it didn't load, and any error, which build() then
	/// reports. The budget is the one build() has, less what the record's build spent.
	/// @param previous The earlier build's record (record()).
	/// @param limits The limits.
	/// @return The tree.
	/// @throws LispError If building fails or a limit runs out, naming the place, as build() would.
	[[nodiscard]] json rebuild(const BuildRecord& previous, const Limits& limits = {});

	/// @brief Changes an interpreter that has loaded an earlier version of the program (load(), without everything)
	/// into one that has loaded this one: the definitions that changed are defined again, with their entries' authors,
	/// those that went are taken away, and those that use either are defined again, so that the values they work out
	/// are worked out again. Where it can't be sure it gives what load() gives, it changes nothing and returns false:
	/// a changed or removed macro, hook, advice or form that isn't a definition, a definition whose kind changed or
	/// whose name is one of the language's, a file's `ns` form added, removed, moved or changed. The interpreter must
	/// be pure while it runs (setPure()).
	/// @param interpreter The interpreter.
	/// @param before The earlier version, as it was loaded.
	/// @param built The record of this version's build (record()): who uses what.
	/// @param limits What the change may spend.
	/// @param bytesLeft If given, gets what is left of the budget's bytes once it is changed: the heap keeps what it
	/// allocated, so the caller can bound what changes in place keep together.
	/// @return true if it changed it; false if nothing was done.
	/// @throws LispError If a definition fails: the interpreter is then half changed, for the caller to throw away.
	[[nodiscard]] bool redefine(Interpreter& interpreter, const Program& before, const BuildRecord& built,
	                            const Limits& limits = {}, std::size_t* bytesLeft = nullptr) const;
	/// @brief What the last build kept of each definition, for the build of a later version (rebuild()).
	/// @return It; nullptr before a build.
	[[nodiscard]] std::shared_ptr<const BuildRecord> record() const noexcept { return record_; }

	/// @brief Makes the next builds note where they make each node with an id (nodePlaces()), or stop.
	/// Off by default: a build that records nothing costs one test per node more than before.
	/// @param on Whether to record.
	void recordOrigins(bool on) noexcept { recording_ = on; }
	/// @brief Where the last build made each node with an id, if it recorded (recordOrigins()): the first place of each
	/// id, in the order made. A node with no id, or built from a map or by a function rather than a form, has none.
	/// @return Them; empty if it didn't record.
	[[nodiscard]] const std::vector<NodePlace>& nodePlaces() const noexcept { return places_; }

	/// @brief The `add-hook`, `remove-hook` and `advice` definitions of the last build. @return Them, in order of the
	/// entries.
	[[nodiscard]] const std::vector<Extension>& extensions() const noexcept { return extensions_; }
	/// @brief The names the last build's definitions use that nothing defines. @return Them, in order of use.
	[[nodiscard]] const std::vector<UndefinedName>& undefinedNames() const noexcept { return undefined_; }
	/// @brief Where the last build's functions test a path of the running app with the language's truth: `(if
	/// state.query ...)`, `(when (not computed.list) ...)`, an operand of `and` or `or`. There "", 0 and [] are true,
	/// as only nil and false are false, while in a binding they are false (an expression's truth); `name` holds the
	/// path. And where they add a text written out with `+`, which joins texts in an expression and is an error in a
	/// function (`str` joins them): `name` is then `+`. @return Them, in order of compiling.
	[[nodiscard]] const std::vector<UndefinedName>& truthTests() const noexcept { return truth_; }
	/// @brief The names each definition's code uses, macros' output and the functions inside it included: nothing runs
	/// but what a name reaches, as the language has no way to find a global from a value. @return Them, by its address.
	[[nodiscard]] const std::map<std::string, std::vector<std::string>>& namesUsed() const noexcept { return used_; }
	/// @brief Of the names each definition uses (namesUsed()), those it calls with a text written out first, with those
	/// texts: `(run "save")` and `(run :tidy)` give `{"run": ["save", "tidy"]}`; and which of them it also uses another
	/// way (as a value, through `apply`, with a computed first argument). A host may follow what the texts name where
	/// a name is used no other way, and count the rest as reaching anything. @return Them, by its address.
	[[nodiscard]] const NamedCalls& namedCalls() const noexcept { return named_; }
	/// @brief The members the last build took from a `defdefaults`, as no other definition gave them, and the state's
	/// keys a `defstate` starts: a document's own value of one wins over it (buildDocument()), as a `defvar` gives a
	/// variable a value only if it has none. @return Them, as section and member.
	[[nodiscard]] const std::set<std::pair<std::string, std::string>>& defaulted() const noexcept { return defaulted_; }

private:
	/// @brief Keeps the first note of each id a recording build made, as nodePlaces() gives them, while the build's
	/// interpreter still owns the files' names.
	/// @param notes The notes.
	void keepNodePlaces(const std::vector<NodeNote>& notes);
	/// @brief Keeps a build in full's truth tests and the names used that nothing defined, each in its definitions'
	/// order.
	/// @param machine The build's machine.
	void keepTestsAndUndefined(const Machine& machine);
	/// @brief Builds the tree from a record, running only what changed (rebuild()).
	/// @param previous The record.
	/// @param limits The limits.
	/// @return The tree, or std::nullopt where it can't be sure, or the build failed: build() then says why.
	[[nodiscard]] std::optional<json> buildInPart(const BuildRecord& previous, const Limits& limits);
	/// @brief Takes what a build in part's definitions found, as a build in full finds it: hooks and advice, names used
	/// and undefined, the state's defaults; the node places, which only a recording build notes, are forgotten.
	/// @param record The build's record.
	void keepWhatWasDefined(const BuildRecord& record);
	/// @brief Keeps what a build in full found (record()), and whether a later build may start from it.
	/// @param machine The build's machine.
	/// @param record What it kept of each definition, in its order.
	/// @param limits Its limits.
	void keepRecord(Machine& machine, std::vector<DefinitionBuilt> record, const Limits& limits);

	std::vector<Entry> entries_;                                  ///< See entries().
	std::map<std::string, std::size_t> names_;                    ///< Where each entry is in entries_, by its name.
	std::vector<std::pair<std::string, std::string>> namespaces_; ///< See namespaceForms().
	/// For a program that holds only some of a program's entries (what a build in part loads): each file's definitions
	/// in the whole program, so that its namespaces hold what the whole program's do; empty otherwise, and load() takes
	/// them from the entries.
	std::map<std::string, std::set<std::string, std::less<>>, std::less<>> definedInWhole_;
	std::vector<std::pair<std::string, Primitive>> primitives_;   ///< See definePure().
	std::vector<Extension> extensions_;                           ///< See extensions().
	std::vector<UndefinedName> undefined_;                        ///< See undefinedNames().
	std::vector<UndefinedName> truth_;                            ///< See truthTests().
	std::map<std::string, std::vector<std::string>> used_;        ///< See namesUsed().
	NamedCalls named_;                                            ///< See namedCalls().
	std::set<std::pair<std::string, std::string>> defaulted_;     ///< See defaulted().
	std::vector<NodePlace> places_;                               ///< See nodePlaces().
	bool recording_ = false;                                      ///< See recordOrigins().
	std::shared_ptr<BuildRecord> record_;                         ///< See record().
};

/// @brief What this thread's builds did (Program::build(), Program::rebuild()): for the tests, which take the
/// difference of two readings.
struct RebuildStatistics {
	std::size_t whole = 0;        ///< Builds that ran every definition.
	std::size_t inPart = 0;       ///< Builds in part.
	std::size_t compiled = 0;     ///< The definitions they loaded and compiled, all of them together.
	std::size_t redefined = 0;    ///< Interpreters changed in place (Program::redefine()).
	std::size_t definedAgain = 0; ///< The definitions those changes defined again, all of them together.
};

/// @brief What this thread's builds did so far. @return It.
[[nodiscard]] const RebuildStatistics& rebuildStatistics() noexcept;

} // namespace affresco::lisp
