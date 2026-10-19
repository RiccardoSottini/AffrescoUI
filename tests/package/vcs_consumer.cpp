/// @file
/// @brief Uses one installed module on its own: version control (affrescoui::vcs) keeping a document of the
/// application's own format, with no window or OpenGL. Exits with 0 when every check holds.

#include <affresco/vcs/Repository.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

/// @brief The rules of a format with no ids and no permissions: any JSON object is valid, and anyone may change it.
class AnyObject final : public affresco::DocumentRules {
public:
	/// @brief Accepts any JSON object.
	/// @param document The document.
	/// @throws affresco::DocumentError If it is not an object.
	void validate(const affresco::json& document) const override {
		if(!document.is_object()) throw affresco::DocumentError("document: expected an object");
	}

	/// @brief Finds no node: the format has no ids.
	/// @return std::nullopt.
	[[nodiscard]] std::optional<std::string> findNode(const affresco::json&, std::string_view) const override {
		return std::nullopt;
	}

	/// @brief Has no rules, so every change applies.
	/// @return Permissions that are not configured.
	[[nodiscard]] affresco::Permissions permissions(const affresco::json&) const override { return {}; }

	/// @brief Forbids nothing.
	void checkChange(const affresco::json&, const affresco::json&) const override {}
};

} // namespace

/// @brief Commits a change, reverts it, and checks the document and the log.
/// @return 0 if every check holds, 1 otherwise.
int main() {
	const AnyObject rules;
	const affresco::json original = {{"title", "Groceries"}};
	affresco::vcs::Repository repository(rules, original);

	const affresco::RevisionResult renamed =
	    repository.submit({{"author", "app"}, {"message", "Rename"}, {"set", {{"/title", "Shopping"}}}});
	const bool applied =
	    renamed.status == affresco::RevisionStatus::Applied && repository.document().at("title") == "Shopping";

	const affresco::RevisionResult reverted = repository.submit(repository.revertRevision("HEAD", "app"));
	const bool restored = reverted.status == affresco::RevisionStatus::Applied && repository.document() == original;

	if(!applied || !restored || repository.log().size() != 3) {
		std::cerr << "vcs_consumer: failed: " << renamed.error << reverted.error << '\n';
		return 1;
	}
	std::cout << "vcs_consumer: committed, reverted, " << repository.log().size() << " commits\n";
	return 0;
}
