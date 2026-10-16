#pragma once

/// @file
/// @brief The DocumentRules interface: what version control needs to know about a document format.

#include "sgl/core/Json.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgl {

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
	bool configured = false;                      ///< false: no rules, so every change applies and anyone reviews.
	std::vector<PermissionRule> allow, ask, deny; ///< The rules, in the order they were written.
	std::vector<std::string> reviewers;           ///< Who may decide on proposals (author or channel names).
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
	/// @param before The document now.
	/// @param after The document after the change.
	/// @throws DocumentError If the change is forbidden.
	virtual void checkChange(const json& before, const json& after) const = 0;
};

} // namespace sgl
