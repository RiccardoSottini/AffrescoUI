/// @file
/// @brief What a build keeps of each definition (BuildRecord), so that the build of a later version runs only the
/// definitions that changed and those that use them (Program::rebuild()), and gives the tree a build in full gives.
#pragma once

#include "Machine.hpp"
#include "TreeParts.hpp"

#include <affresco/lisp/Interpreter.hpp>
#include <affresco/lisp/Program.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco::lisp {

/// @brief What a build kept of one definition: its entry, what its code uses, and what it gave.
struct DefinitionBuilt {
	std::string name;                    ///< Its address, its entry's name.
	std::string kind;                    ///< Its form's word: `defn`, `defstate`...
	std::string file;                    ///< Its entry's file.
	std::string text;                    ///< Its entry's text.
	std::string author;                  ///< Its entry's author.
	std::vector<std::string> uses;       ///< What its code uses by name (Program::namesUsed()).
	std::optional<CallsByName> named;    ///< What it calls with a text written out first, if anything.
	std::vector<Gift> gifts;             ///< What it gave the tree, in order.
	std::optional<Extension> extension;  ///< Itself, if it is a hook or advice.
	std::vector<UndefinedName> undefined; ///< The names it uses that nothing defined.
	std::vector<UndefinedName> truth;     ///< Its truth tests (Program::truthTests()).
};

/// @brief What a build kept of each definition, in the order the build ran them (macros, then the rest, each in the
/// order of their entries).
struct BuildRecord {
	std::vector<std::shared_ptr<const DefinitionBuilt>> definitions; ///< Each definition's.
	std::map<std::string, std::size_t, std::less<>> index;           ///< Each one's place in definitions, by its name.
	std::map<std::string, std::vector<std::size_t>, std::less<>> users; ///< By name, the definitions that use it.
	bool inPart = false;      ///< A later build may start from it: see Program::rebuild() for when one may not.
	bool extensions = false;  ///< The program has hooks or advice.
	Budget spent;             ///< What the build spent, at most: a build in part adds what it spent to its base's.
	std::size_t macroSpent = 0; ///< What macro expansion spent, at most, likewise.
	std::size_t compiled = 0;   ///< The definitions the build loaded and compiled.
	bool whole = true;          ///< It ran every definition.
	/// For a build in part: the tree's places, as JSON pointers, that the definitions it ran gave, before or after.
	std::vector<std::string> touched;
	std::uint64_t serial = 0; ///< Which build it is, for changedPlaces().
	std::uint64_t from = 0;   ///< The build it started from, or 0.
};

/// @brief The definition's record, as a build keeps it: its uses, and what it gives the tree, put into the parts too.
/// @param machine The machine, its definitions worked out.
/// @param definition The definition.
/// @param parts Gets what it gives, or nullptr.
/// @return The record; its entry's text, file and author are the caller's to fill.
/// @throws Problem If it can't be built.
[[nodiscard]] DefinitionBuilt recordDefinition(Machine& machine, const Definition& definition, BuiltParts* parts);

/// @brief Sorts what a build found by the definition it is in, in the order found.
/// @param found What it found.
/// @param index Each definition's place, by its name.
/// @param into Gets each item.
/// @param truth Into each definition's truth tests; else its undefined names.
/// @return false if it found one in no definition.
[[nodiscard]] bool sortByDefinition(const std::vector<UndefinedName>& found,
                                    const std::map<std::string, std::size_t, std::less<>>& index,
                                    std::vector<DefinitionBuilt>& into, bool truth);

/// @brief Puts what a build found in the order of the definitions it is in, each one's in the order found: a build
/// compiles a function a view calls when the view runs, before its turn, and a build in part compiles fewer, so only
/// this order is the same in both.
/// @param machine The build's machine.
/// @param found What it found.
void sortByDefinitionOrder(const Machine& machine, std::vector<UndefinedName>& found);

/// @brief The tree's places a definition gave, as JSON pointers: `/section/member`, or `/key` (`/root`).
/// @param definition The definition.
/// @param places Gets them.
void addPlacesGiven(const DefinitionBuilt& definition, std::vector<std::string>& places);

/// @brief A number no other build has: a record's serial.
/// @return It.
[[nodiscard]] std::uint64_t nextBuildSerial();

/// @brief Counts builds in this thread, for the tests: see rebuildStatistics().
/// @param whole It ran every definition.
/// @param compiled The definitions it compiled.
void countBuild(bool whole, std::size_t compiled);

} // namespace affresco::lisp
