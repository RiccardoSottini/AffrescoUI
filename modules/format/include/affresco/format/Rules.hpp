#pragma once

/// @file
/// @brief format::Rules: the AffrescoUI Screen Format as rules for version control.

#include "affresco/core/DocumentRules.hpp"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace affresco::format {

class SectionRegistry;

/// @brief The AffrescoUI Screen Format (SCREEN_FORMAT.md) as rules for version control (VERSION_CONTROL.md).
///
/// "permissions" holds rules like Claude Code's: `{"allow": [...], "ask": [...], "deny": [...], "reviewers": [...]}`,
/// each rule "author" or "author(pattern)". Without a "permissions" section everyone may change everything.
/// Revisions may never change the permissions or add or alter control steps (accept, reject, preview, restore,
/// revert, undo, patch, call), so no author can approve itself. Nor may they refer to state paths, variables,
/// actions, styles, components, parameters, fonts, colours or screens that don't exist (SCREEN_FORMAT.md, "Errors").
///
/// Used on the main thread: the checker compiles expressions through the main thread's cache.
class Rules final : public DocumentRules {
public:
	/// @brief Rules with the built-in sections of the format (SectionRegistry).
	Rules();

	/// @brief Checks the top-level sections, each as its SectionRegistry entry says: none unknown, "version" (1) and
	/// "root" there, "actions", "components", "channels", "persist", "permissions" and the others well formed.
	/// @param document The document.
	/// @throws DocumentError If something is wrong; what() names the place.
	void validate(const json& document) const override;

	/// @brief Finds a node by "id" in "root", then in "templates".
	/// @param document The document.
	/// @param id The id.
	/// @return The node's JSON Pointer, or std::nullopt.
	[[nodiscard]] std::optional<std::string> findNode(const json& document, std::string_view id) const override;

	/// @brief Reads "permissions": its allow, ask and deny rules and its reviewers. Nodes with `"locked": true` are
	/// denied to everyone ("*(#id)").
	/// @param document The document.
	/// @return The rules.
	[[nodiscard]] Permissions permissions(const json& document) const override;

	/// @brief Forbids changing "permissions", adding or altering control steps, and referring to what the screen
	/// doesn't define (findings that were there before don't count).
	/// @param before The document now, with the live state as its "state".
	/// @param after The document after the change, with the live state after it.
	/// @throws DocumentError If the change is forbidden; what() lists the new findings, each with its place.
	void checkChange(const json& before, const json& after) const override;

	/// @brief Checks a screen without running it: what it refers to but doesn't define (SCREEN_FORMAT.md, "Errors").
	/// @param document The document.
	/// @param state The state to check against (the live state of a running screen).
	/// @return One line per finding, with its place and a hint; empty if there are none.
	[[nodiscard]] std::vector<std::string> findings(const json& document, const json& state) const;

	/// @brief Lets documents name an action the application registered (Screen::onAction) without a finding.
	/// @param name The action.
	void addNativeAction(std::string name);

private:
	std::set<std::string, std::less<>> nativeActions_; ///< From addNativeAction().
	std::shared_ptr<const SectionRegistry> sections_;  ///< The sections; shared by copies, never changed.
};

} // namespace affresco::format
