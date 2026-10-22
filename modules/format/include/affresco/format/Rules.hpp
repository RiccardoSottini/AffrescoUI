#pragma once

/// @file
/// @brief format::Rules: the AffrescoUI Screen Format as rules for version control.

#include "affresco/core/DocumentRules.hpp"
#include "affresco/core/Services.hpp"

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
/// revert, undo, patch, call, send, connect, respond), so no author can approve itself. Nor may they refer to state paths, variables,
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

	/// @brief Finds a node by "id" in "root", then in "screens", "templates" and the components (findNodeWithId()).
	/// @param document The document.
	/// @param id The id.
	/// @return The node's JSON Pointer, or std::nullopt.
	[[nodiscard]] std::optional<std::string> findNode(const json& document, std::string_view id) const override;

	/// @brief Reads "permissions": its allow, ask and deny rules and its reviewers. Nodes with `"locked": true` are
	/// denied to everyone ("*(#id)").
	/// @param document The document.
	/// @return The rules.
	[[nodiscard]] Permissions permissions(const json& document) const override;

	/// @brief Forbids changing "permissions" (or another capability section, such as "services"), adding or altering
	/// control steps, adding uses of services the document doesn't declare or methods kept from revisions (RFC 0002),
	/// and referring to what the screen doesn't define (findings that were there before don't count).
	/// @param before The document now, with the live state as its "state".
	/// @param after The document after the change, with the live state after it.
	/// @throws DocumentError If the change is forbidden; what() lists the new findings, each with its place.
	void checkChange(const json& before, const json& after) const override;

	/// @brief Migrates a version 1 document to version 2 (format::migrate()).
	/// @param document The document, changed in place.
	/// @return One line for each rewrite.
	[[nodiscard]] std::vector<std::string> upgrade(json& document) const override;

	/// @brief Migrates a version 1 revision's patch (format::migratePatch()).
	/// @param patch The patch, changed in place.
	/// @return One line for each rewrite.
	[[nodiscard]] std::vector<std::string> upgradeRevision(json& patch) const override;

	/// @brief Refuses a revision of more than 100,000 operations, or of more than 16 MiB written as JSON
	/// (SECURITY.md, "Limits").
	/// @param patch The revision's operations, as JSON Patch.
	/// @throws DocumentError If it is larger.
	void checkRevision(const json& patch) const override;

	/// @brief Gives readable ids (NodeNamer) to the nodes a revision adds or puts in place without one: a node, a
	/// list of "children", or a section of screens, templates or components. A node that replaces one keeps that
	/// one's id.
	/// @param document The document before the operation.
	/// @param pointer Where the value goes.
	/// @param replaces true if the value replaces what is there.
	/// @param value The value, changed in place.
	void nameNodes(const json& document, std::string_view pointer, bool replaces, json& value) const override;

	/// @brief Says what a path that finds nothing may have meant: the nearest id for "#id", else, at the first part of
	/// a JSON Pointer that isn't there, the nearest key, the size of a list or the kind of a value.
	/// @param document The document the path was read in.
	/// @param path The path as written.
	/// @return The hint, or "" if the whole path is there.
	[[nodiscard]] std::string pathHint(const json& document, std::string_view path) const override;

	/// @brief Checks a screen without running it: what it refers to but doesn't define (SCREEN_FORMAT.md, "Errors").
	/// @param document The document.
	/// @param state The state to check against (the live state of a running screen).
	/// @return One line per finding, with its place and a hint; empty if there are none.
	[[nodiscard]] std::vector<std::string> findings(const json& document, const json& state) const;

	/// @brief Lets documents name an action the application registered (Screen::onAction) without a finding.
	/// @param name The action.
	void addNativeAction(std::string name);

	/// @brief Sets the screen's services, so a revision is checked against what their methods are and do.
	/// @param services The services (outlive the rules, or are replaced first), or nullptr: then a revision may add
	/// uses of declared services only, each one reviewed.
	void setServices(const services::Services* services) noexcept { services_ = services; }

	/// @brief One line for each use of a service method that writes or runs that the change adds (or of one the
	/// services can't describe): version control makes such a change a proposal, whoever makes it.
	/// @param before The document now.
	/// @param after The document after the change.
	/// @return The lines, e.g. "uses files.write (writes: ...)".
	[[nodiscard]] std::vector<std::string> reviewLines(const json& before, const json& after) const override;

	/// @brief One line for each part of the screen's behaviour the change adds, removes or changes: actions, shortcuts,
	/// watches, onLoad, computed values, and each node's handlers, steps and bindings (BehaviourLines.cpp).
	/// @param before The document now.
	/// @param after The document after the change.
	/// @return The lines, e.g. "changes the action 'save'", "adds onClick to #addCard".
	[[nodiscard]] std::vector<std::string> behaviourLines(const json& before, const json& after) const override;

private:
	std::set<std::string, std::less<>> nativeActions_; ///< From addNativeAction().
	std::shared_ptr<const SectionRegistry> sections_;  ///< The sections; shared by copies, never changed.
	const services::Services* services_ = nullptr;     ///< See setServices().
};

} // namespace affresco::format
