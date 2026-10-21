#pragma once

/// @file
/// @brief The DocumentRules interface: what version control needs to know about a document format.

#include "affresco/core/Json.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief One permission rule, written like a Claude Code permission rule (`Tool(specifier)`): `author` or
/// `author(pattern)`, e.g. "studio", "agent(#board)", "*(#statusBar)", "agent(/state/cards)".
struct PermissionRule {
	std::string text;    ///< As written.
	std::string author;  ///< "*", a channel name ("agent" covers "agent/claude-code") or a full author name.
	std::string pattern; ///< Empty: the whole document. Else a JSON Pointer or "#id" path; "*" matches one segment.
};

/// @brief Parses a permission rule. A pattern covers everything inside it; a trailing "/**" says the same.
/// @param text The rule.
/// @return The rule, or std::nullopt if it is malformed.
inline std::optional<PermissionRule> parsePermissionRule(std::string_view text) {
	PermissionRule rule{std::string(text), std::string(text), {}};
	if(const std::size_t open = text.find('('); open != std::string_view::npos) {
		if(!text.ends_with(')')) return std::nullopt;
		rule.author = text.substr(0, open);
		rule.pattern = text.substr(open + 1, text.size() - open - 2);
		if(rule.pattern.ends_with("/**")) rule.pattern.resize(rule.pattern.size() - 3);
		if(rule.pattern.empty() || (rule.pattern[0] != '/' && rule.pattern[0] != '#')) return std::nullopt;
	}
	if(rule.author.empty() || rule.author.find_first_of("() ") != std::string::npos) return std::nullopt;
	return rule;
}

/// @brief What a document lets each author do: its permission rules, checked deny first, then ask, then allow.
struct Permissions {
	bool configured = false;            ///< false: no rules, so every change applies and anyone reviews.
	std::vector<PermissionRule> allow;  ///< The allow rules, in the order they were written.
	std::vector<PermissionRule> ask;    ///< The ask rules, in the order they were written.
	std::vector<PermissionRule> deny;   ///< The deny rules, in the order they were written.
	std::vector<std::string> reviewers; ///< Who may decide on proposals (author or channel names).
};

/// @brief What version control needs to know about the documents it stores.
///
/// Version control can patch and keep any JSON; the format that gives the JSON its meaning (format::Rules) says what
/// is valid, where nodes are, and who may change what.
class DocumentRules {
public:
	virtual ~DocumentRules() = default;

	/// @brief Checks that a document is valid.
	/// @param document The document.
	/// @throws DocumentError If it is not; what() names the place.
	virtual void validate(const json& document) const = 0;

	/// @brief Finds a node by id (what "#id" patch paths and rule patterns refer to).
	/// @param document The document.
	/// @param id The id, without '#'.
	/// @return The node's JSON Pointer, or std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> findNode(const json& document, std::string_view id) const = 0;

	/// @brief Reads a document's permission rules.
	/// @param document The document.
	/// @return The rules; parts that are locked for everyone are among the deny rules.
	[[nodiscard]] virtual Permissions permissions(const json& document) const = 0;

	/// @brief Checks changes the format forbids for everyone (e.g. editing the permissions).
	/// @param before The document now, with the live state as its "state".
	/// @param after The document after the change, with the live state after it.
	/// @throws DocumentError If the change is forbidden.
	virtual void checkChange(const json& before, const json& after) const = 0;

	/// @brief Checks a revision's size before any of it is applied, so a peer can't make the screen do unbounded
	/// work. The default takes any size.
	/// @param patch The revision's operations, as JSON Patch.
	/// @throws DocumentError If it is too large.
	virtual void checkRevision([[maybe_unused]] const json& patch) const {}

	/// @brief What a change does that a person must always review, whoever makes it and whatever the rules allow (e.g.
	/// a new step that writes files). Version control turns any line into a proposal. The default has none.
	/// @param before The document now, with the live state as its "state".
	/// @param after The document after the change, with the live state after it.
	/// @return One line for each such use, e.g. "uses files.write (writes files in 'workspace')".
	[[nodiscard]] virtual std::vector<std::string> reviewLines([[maybe_unused]] const json& before,
	                                                           [[maybe_unused]] const json& after) const {
		return {};
	}

	/// @brief Gives ids to the nodes a revision puts somewhere without one, before the change is applied, so that the
	/// commit, the history and every diff show them. The default gives none (a format without nodes).
	/// @param document The document as it is before the operation, with the live state as its "state".
	/// @param pointer Where the value goes (a JSON Pointer, "#id" paths resolved).
	/// @param replaces true if the value replaces what is there, false if it is added.
	/// @param value The value, changed in place.
	virtual void nameNodes([[maybe_unused]] const json& document, [[maybe_unused]] std::string_view pointer,
	                       [[maybe_unused]] bool replaces, [[maybe_unused]] json& value) const {}

	/// @brief A one-line hint for a path in a revision that finds nothing ("did you mean '#statusBar'?"), for the
	/// error that rejects it. The default has none.
	/// @param document The document the path was read in, with the live state as its "state".
	/// @param path The path as written: a JSON Pointer or a "#id" path.
	/// @return The hint, or "".
	[[nodiscard]] virtual std::string pathHint([[maybe_unused]] const json& document,
	                                           [[maybe_unused]] std::string_view path) const {
		return {};
	}
};

} // namespace affresco
