#pragma once

#include "sgl/core/Json.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgl {

// What an author's revisions do: applied, proposed to the user for review, or refused
enum class Policy {
	Allow,
	Review,
	Deny,
};

// What version control needs to know about the documents it stores. Version control can patch and keep any JSON;
// the format that gives the JSON its meaning (implemented by format::Rules) says what is valid, where nodes are,
// and who may change what.
class DocumentRules {
public:
	virtual ~DocumentRules() = default;

	// Throws DocumentError if `document` is not a valid document
	virtual void validate(const json& document) const = 0;

	// JSON Pointer of the node with that id (what "#id" paths in a patch refer to), or nothing
	[[nodiscard]] virtual std::optional<std::string> findNode(const json& document, std::string_view id) const = 0;

	// What `document` lets `author` do
	[[nodiscard]] virtual Policy policyFor(const json& document, std::string_view author) const = 0;

	// Paths no revision may change: "#id", "/state/..." (the live state) or JSON Pointers into the document
	[[nodiscard]] virtual std::vector<std::string> lockedPaths(const json& document) const = 0;

	// Throws DocumentError if the format forbids going from `before` to `after` (e.g. changing the permissions)
	virtual void checkChange(const json& before, const json& after) const = 0;
};

} // namespace sgl
