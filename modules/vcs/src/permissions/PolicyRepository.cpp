/// @file
/// @brief The parts of vcs::Repository that data rules add: what an author may read, and the paths its patches name.

#include "affresco/vcs/Repository.hpp"

#include "permissions/Permissions.hpp"
#include "permissions/Policy.hpp"

#include "affresco/core/Json.hpp"

namespace affresco::vcs {

void Repository::checkReadable(const json& patch, const std::string& author) const {
	const detail::Policy policy = detail::policyOf(mainDocument());
	if(!detail::readLimited(policy, author, roles_)) return;
	const json view = detail::view(mainDocument(), committedState());
	if(const auto unreadable = detail::unreadablePath(rules_, policy, view, patch, author, roles_))
		throw DocumentError("AffrescoUI screen: revision: permission denied: " + *unreadable);
}

std::optional<json> Repository::readable(std::string_view author) const {
	const detail::Policy policy = detail::policyOf(mainDocument());
	if(!detail::readLimited(policy, author, roles_)) return std::nullopt;
	return detail::readableView(policy, detail::view(document_, state_), author, roles_);
}

} // namespace affresco::vcs
