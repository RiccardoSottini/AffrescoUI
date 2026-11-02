#pragma once

/// @file
/// @brief What format::Rules adds for programs (RFC 0005): the "program" section's check, and what a change of
/// definitions does beyond what they build.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief What a document's program can do, as a revision's reviewer needs it: which names are its definitions, and
/// which reach a control or a review step. Nothing runs but what a name reaches (lisp::Program::namesUsed()), so this
/// is sound, and a function that reaches one counts as an action holding one.
struct ProgramReach {
	std::set<std::string> defined; ///< Its definitions.
	std::set<std::string> control; ///< Those that reach a control or a review step.
};

/// @brief What a change's hooks and advice need: the review changing what they join needs. Adding or taking away
/// advice, or a hook where a hook point meets a control step, is always reviewed.
/// @param before The view before it.
/// @param after The view after it.
/// @param lines Gets what it says.
void addExtensionReviewLines(const json& before, const json& after, std::vector<std::string>& lines);

/// @brief What a document's program can do.
/// @param document The document.
/// @return It; nothing for a document without a program, or one that can't be built, which the build reports.
[[nodiscard]] ProgramReach programReach(const json& document);

/// @brief The definitions of a document's program that use a step that writes records, each with its text: as a new or
/// changed write step in a handler is, a new or changed one here is always reviewed.
/// @param document The document.
/// @param writes The steps that write.
/// @param found Gets each, as `{step: true, "definition": name, "text": text}`.
void collectProgramWrites(const json& document, std::span<const std::string_view> writes,
                          std::multiset<std::string>& found);

/// @brief "program": `file -> {definition -> its text}`, within the limits (lisp/Document.hpp). What the text means is
/// the build's to say.
/// @param value The value.
/// @param name "program".
/// @throws DocumentError If it is malformed.
void checkProgramSection(const json& value, const std::string& name);

/// @brief Refuses a change that takes away a definition another still uses, which the build alone wouldn't notice: a
/// function's body runs only when it is called.
/// @param before The view before it.
/// @param after The view after it.
/// @throws DocumentError If it does.
void checkProgramChange(const json& before, const json& after);

/// @brief Adds a line for each definition a change touches that does more than what it builds.
/// @param before The view before it.
/// @param after The view after it.
/// @param lines Gets them.
void addProgramLines(const json& before, const json& after, std::vector<std::string>& lines);

/// @brief How many such definitions a change touches.
/// @param before The view before it.
/// @param after The view after it.
/// @return The count.
[[nodiscard]] std::size_t programLogic(const json& before, const json& after);

} // namespace affresco::format
