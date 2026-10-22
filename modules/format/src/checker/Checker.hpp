#pragma once

/// @file
/// @brief The static checker: finds what a screen refers to but doesn't define, without running it.
///
/// It reads the document the way the runtime builds it (components, styles, repeat, let, with, actions run with
/// their callers' variables) and reports, each with its place:
/// - expressions reading state paths, computed values or variables that don't exist;
/// - actions, styles, components, fonts, colours and screens that are referenced but not defined;
/// - component parameters the component doesn't declare;
/// - steps it doesn't know, and expressions that don't parse.
///
/// What the state holds is learnt from its values plus what steps write (`set`, `push`, `bind`, ...), so a key that
/// an action creates can be read. A value the checker can't know (an event, a native call's result, an empty object)
/// is never reported: it would rather miss a typo than report one that isn't there.

#include "affresco/core/Json.hpp"

#include <compare>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace affresco::format {

/// @brief One thing a screen refers to but doesn't define.
struct Finding {
	std::string place;   ///< Where, as in load errors: "root.children[2].text".
	std::string problem; ///< What, without the place: "'state.cardz' doesn't exist".
	std::string hint;    ///< What may be meant: "did you mean 'state.cards'?" (may be empty).

	/// @brief The finding as one line, like a load error.
	/// @return "<place>: <problem> (<hint>)".
	[[nodiscard]] std::string text() const;

	/// @brief Orders findings by place, then problem, then hint.
	auto operator<=>(const Finding&) const = default;
};

/// @brief Checks a screen document without running it.
/// @param document The document; its own "state" is ignored in favour of @p state.
/// @param state The state to check against (the live state of a running screen).
/// @param nativeActions The actions the application registered (Screen::onAction): documents may name them.
/// @return The findings, sorted by place, each once; std::nullopt if the document is too large to check within the
/// checker's limit (a million values visited).
[[nodiscard]] std::optional<std::vector<Finding>> check(const json& document, const json& state,
                                                        const std::set<std::string, std::less<>>& nativeActions = {});

/// @brief The findings a change brings in: those in @p after that @p before doesn't have.
///
/// Places move when nodes are added or moved, so a finding that isn't at the same place before is matched by its
/// problem: it is new only when its problem occurs more often after the change than before.
/// @param before The findings before the change.
/// @param after The findings after it.
/// @return The new findings, in the order of @p after.
[[nodiscard]] std::vector<Finding> introduced(const std::vector<Finding>& before, const std::vector<Finding>& after);

} // namespace affresco::format
