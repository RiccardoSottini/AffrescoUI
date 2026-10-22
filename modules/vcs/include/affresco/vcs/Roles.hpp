#pragma once

/// @file
/// @brief vcs::Roles: the role each author plays for this run ("design", "data", "builder"), which rules can name as
/// "role:<name>" (VERSION_CONTROL.md, "Roles").

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::vcs {

/// @brief Who plays which role. Set by the host or by a reviewer, never by the author itself; kept in memory for the
/// run. Bounded: maxRoles authors, names of at most maxNameBytes.
///
/// Not thread-safe: kept on the thread that changes the document.
class Roles {
public:
	static constexpr std::size_t maxRoles = 64;     ///< Authors with a role.
	static constexpr std::size_t maxNameBytes = 64; ///< The longest author or role name.

	/// @brief Gives an author a role, or takes it away.
	/// @param author An author ("agent/claude-code") or a channel ("agent"), whose authors all get it.
	/// @param role A name of lowercase letters, digits and '-' ("design"); empty takes the role away.
	/// @return Why it was refused, or std::nullopt.
	[[nodiscard]] std::optional<std::string> set(std::string_view author, std::string_view role);

	/// @brief The role an author plays: its own, else its channel's.
	/// @param author The author, e.g. "agent/claude-code".
	/// @return The role, or "" if none.
	[[nodiscard]] std::string of(std::string_view author) const;

	/// @brief Every role given, as `{"agent/claude-code": "design"}`.
	/// @return The roles.
	[[nodiscard]] json toJson() const;

private:
	std::vector<std::pair<std::string, std::string>> roles_; ///< Author (or channel), role; in the order given.
};

/// @brief Tells whether a rule's author covers an author: "*", the author, its channel, or "role:<name>" for the role
/// it plays.
/// @param ruleAuthor As written in the rule.
/// @param author The author.
/// @param roles Who plays which role.
/// @return true if it does.
[[nodiscard]] bool ruleCovers(std::string_view ruleAuthor, std::string_view author, const Roles& roles);

} // namespace affresco::vcs
