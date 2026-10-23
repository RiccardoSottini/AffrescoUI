#pragma once

/// @file
/// @brief Budgets and data rules: the parts of "permissions" that limit how much an author changes in one revision,
/// and which paths it may read, write or never touch (VERSION_CONTROL.md, "Budgets" and "Data rules").
///
/// Like the other rules they are judged by the change's effect: nodes are counted by id before and after, paths are
/// compared before and after, however the patch was written.

#include "affresco/core/DocumentRules.hpp"
#include "affresco/vcs/Roles.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vcs::detail {

/// @brief One budget: the most nodes an author may add, remove and change in one revision (and in one proposal,
/// judged whole when it merges).
struct ChangeBudget {
	std::string author;     ///< "*", a channel, an author or "role:<name>".
	long long added = -1;   ///< Most nodes added; -1: no limit.
	long long removed = -1; ///< Most nodes removed; -1: no limit.
	long long changed = -1; ///< Most nodes changed; -1: no limit.
	bool deny = true;       ///< Over it: refused (the default) or asked ("over": "ask").
};

/// @brief One data rule: paths in state and sections an author may read, write, or never touch.
struct DataRule {
	std::string author;             ///< "*", a channel, an author or "role:<name>".
	std::vector<std::string> read;  ///< If any: the only paths it reads (its write paths too), within the scope.
	std::vector<std::string> write; ///< If any: the only paths it changes, within the scope.
	std::vector<std::string> never; ///< Paths it neither reads nor changes.
};

/// @brief The budgets and data rules of a document.
struct Policy {
	std::vector<ChangeBudget> budgets; ///< In the order written.
	std::vector<DataRule> data;        ///< In the order written.
};

/// @brief Who a data rule is judged for: an author, and the roles that may cover it.
struct Actor {
	std::string_view author; ///< The author.
	const Roles& roles;      ///< Who plays which role.
};

/// @brief How many nodes a change adds, removes and changes, counted by id.
struct NodeCounts {
	std::size_t added = 0;   ///< Ids only after.
	std::size_t removed = 0; ///< Ids only before.
	std::size_t changed = 0; ///< Ids on both sides whose own keys (all but "children") differ.
};

/// @brief What a budget or data rule says about a change.
struct PolicyVerdict {
	bool deny = true;   ///< Refuse it; else ask.
	std::string reason; ///< Names the budget or the rule.
};

/// @brief Reads the budgets and data rules from a document's "permissions" (checked by the format).
/// @param document The document.
/// @return The policy; empty without one.
[[nodiscard]] Policy policyOf(const json& document);

/// @brief Counts the nodes in "root", "screens", "templates" and "overlays" a change adds, removes and changes.
/// @param before The view before.
/// @param after The view after.
/// @return The counts.
[[nodiscard]] NodeCounts countNodes(const json& before, const json& after);

/// @brief The first budget covering an author that a change goes over.
/// @param policy The policy.
/// @param before The view before.
/// @param after The view after.
/// @param author The author.
/// @param roles Who plays which role.
/// @return Deny or ask, with a reason naming the budget; std::nullopt if within every budget.
[[nodiscard]] std::optional<PolicyVerdict> overBudget(const Policy& policy, const json& before, const json& after,
                                                      std::string_view author, const Roles& roles);

/// @brief Whether a change writes where the author's data rules say it may not.
/// @param rules Finds "#id" nodes.
/// @param policy The policy.
/// @param before The view before.
/// @param after The view after.
/// @param actor Who makes it.
/// @return Why it is denied, or std::nullopt.
[[nodiscard]] std::optional<std::string> dataDenial(const DocumentRules& rules, const Policy& policy,
                                                    const json& before, const json& after, const Actor& actor);

/// @brief Tells whether an author's reads are limited: a data rule covering it has "read" or "never" paths.
/// @param policy The policy.
/// @param author The author.
/// @param roles Who plays which role.
/// @return true if they are.
[[nodiscard]] bool readLimited(const Policy& policy, std::string_view author, const Roles& roles);

/// @brief The view as an author may read it: "never" paths removed and, with "read" paths, everything else in the
/// scope too (array elements read as null, so positions stay).
/// @param policy The policy.
/// @param view The view.
/// @param author The author.
/// @param roles Who plays which role.
/// @return The readable view.
[[nodiscard]] json readableView(const Policy& policy, const json& view, std::string_view author, const Roles& roles);

/// @brief Whether a patch names a path the author may not read (a "test" or a "copy" from it would tell its value).
/// @param rules Finds "#id" nodes.
/// @param policy The policy.
/// @param view The view the patch applies to.
/// @param patch The operations.
/// @param actor Who sends it.
/// @return Why it is denied, or std::nullopt.
[[nodiscard]] std::optional<std::string> unreadablePath(const DocumentRules& rules, const Policy& policy,
                                                        const json& view, const json& patch, const Actor& actor);

} // namespace affresco::vcs::detail
