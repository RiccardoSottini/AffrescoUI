#pragma once

/// @file
/// @brief format::Rules: the sGLGUI Screen Format as rules for version control.

#include "sgl/core/DocumentRules.hpp"

namespace sgl::format {

/// @brief The sGLGUI Screen Format (SCREEN_FORMAT.md) as rules for version control (VERSION_CONTROL.md).
///
/// "permissions" holds rules like Claude Code's: `{"allow": [...], "ask": [...], "deny": [...], "reviewers": [...]}`,
/// each rule "author" or "author(pattern)". Without a "permissions" section everyone may change everything.
/// Revisions may never change the permissions or add or alter control steps (accept, reject, preview, restore,
/// revert, undo, patch, call), so no author can approve itself.
class Rules final : public DocumentRules {
public:
	/// @brief Checks the top-level sections, "format", "version" (1), "root", "actions" and "permissions".
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

	/// @brief Forbids changing "permissions" and adding or altering control steps.
	/// @param before The document now.
	/// @param after The document after the change.
	/// @throws DocumentError If the change is forbidden.
	void checkChange(const json& before, const json& after) const override;
};

} // namespace sgl::format
