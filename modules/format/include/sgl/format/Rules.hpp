#pragma once

#include "sgl/core/DocumentRules.hpp"

namespace sgl::format {

// The sGLGUI Screen Format (SCREEN_FORMAT.md) as rules for version control: which documents are valid, where
// "#id" nodes are, and what the "permissions" section allows (GOVERNANCE.md).
class Rules final : public DocumentRules {
public:
	void validate(const json& document) const override;
	[[nodiscard]] std::optional<std::string> findNode(const json& document, std::string_view id) const override;
	[[nodiscard]] Policy policyFor(const json& document, std::string_view author) const override;
	[[nodiscard]] bool mayReview(const json& document, std::string_view author) const override;
	[[nodiscard]] std::vector<std::string> lockedPaths(const json& document) const override;
	void checkChange(const json& before, const json& after) const override;
};

} // namespace sgl::format
