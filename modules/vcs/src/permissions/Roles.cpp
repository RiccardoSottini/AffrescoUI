/// @file
/// @brief vcs::Roles: the role each author plays for this run.

#include "affresco/vcs/Roles.hpp"

#include "permissions/Permissions.hpp"

#include <algorithm>

namespace affresco::vcs {

namespace {

/// @brief Tells whether a role name is well formed: 1 to maxNameBytes lowercase letters, digits and '-', starting
/// with a letter.
/// @param role The name.
/// @return true if it is.
bool isRoleName(std::string_view role) noexcept {
	return !role.empty() && role.size() <= Roles::maxNameBytes && role[0] >= 'a' && role[0] <= 'z' &&
	       std::ranges::all_of(role,
	                           [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

} // namespace

std::optional<std::string> Roles::set(std::string_view author, std::string_view role) {
	if(author.empty() || author.size() > maxNameBytes || author == "*" ||
	   author.find_first_of("() :") != std::string_view::npos)
		return std::string(R"(a role is given to an author or a channel, e.g. "agent" or "agent/claude-code")");
	const auto same = std::ranges::find_if(roles_, [&](const auto& entry) { return entry.first == author; });
	if(role.empty()) {
		if(same != roles_.end()) roles_.erase(same);
		return std::nullopt;
	}
	if(!isRoleName(role)) return std::string("a role is a name of lowercase letters, digits and '-', e.g. \"design\"");
	if(same != roles_.end()) {
		same->second = role;
		return std::nullopt;
	}
	if(roles_.size() >= maxRoles) return std::string("at most 64 authors have a role");
	roles_.emplace_back(author, role);
	return std::nullopt;
}

std::string Roles::of(std::string_view author) const {
	for(const std::string_view name : {author, std::string_view(detail::channelOf(author))}) {
		const auto found = std::ranges::find_if(roles_, [&](const auto& entry) { return entry.first == name; });
		if(found != roles_.end()) return found->second;
	}
	return {};
}

json Roles::toJson() const {
	json result = json::object();
	for(const auto& [author, role] : roles_) result[author] = role;
	return result;
}

bool ruleCovers(std::string_view ruleAuthor, std::string_view author, const Roles& roles) {
	if(ruleAuthor.starts_with("role:")) {
		const std::string role = roles.of(author);
		return !role.empty() && ruleAuthor.substr(5) == role;
	}
	return detail::authorMatches(ruleAuthor, author);
}

} // namespace affresco::vcs
