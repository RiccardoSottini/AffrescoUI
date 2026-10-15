#pragma once

/// @file
/// @brief The DocumentRules interface: what version control needs to know about a document format.

#include "sgl/core/Json.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgl {

/// @brief What an author's revisions do.
enum class Policy {
	Allow,  ///< Applied right away.
	Review, ///< Proposed to a reviewer.
	Deny,   ///< Refused.
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

	/// @brief Finds a node by id (what "#id" patch paths refer to).
	/// @param document The document.
	/// @param id The id, without '#'.
	/// @return The node's JSON Pointer, or std::nullopt.
	[[nodiscard]] virtual std::optional<std::string> findNode(const json& document, std::string_view id) const = 0;

	/// @brief Tells what a document lets an author do.
	/// @param document The document.
	/// @param author The author's full name, e.g. "agent/claude-code".
	/// @return The author's policy.
	[[nodiscard]] virtual Policy policyFor(const json& document, std::string_view author) const = 0;

	/// @brief Tells whether an author may decide on proposals and switch auto-accept.
	/// @param document The document.
	/// @param author The author's full name.
	/// @return true for a reviewer.
	[[nodiscard]] virtual bool mayReview(const json& document, std::string_view author) const = 0;

	/// @brief Lists the paths no revision may change.
	/// @param document The document.
	/// @return "#id" paths, "/state/..." paths (the live state) or JSON Pointers into the document.
	[[nodiscard]] virtual std::vector<std::string> lockedPaths(const json& document) const = 0;

	/// @brief Checks changes the format forbids for everyone (e.g. editing the permissions).
	/// @param before The document now.
	/// @param after The document after the change.
	/// @throws DocumentError If the change is forbidden.
	virtual void checkChange(const json& before, const json& after) const = 0;
};

} // namespace sgl
