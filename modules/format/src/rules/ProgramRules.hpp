#pragma once

/// @file
/// @brief What format::Rules adds for programs (RFC 0005): the "program" section's check, and what a change of
/// definitions does beyond what they build.

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace affresco::format {

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
