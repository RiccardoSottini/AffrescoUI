/// @file
/// @brief The parts of vcs::Repository that permission rules add: what a rule says about a change, who may review,
/// the modes, what an author may read, and the paths its patches name.

#include "affresco/vcs/Repository.hpp"

#include "permissions/Permissions.hpp"
#include "permissions/Policy.hpp"

#include "affresco/core/Json.hpp"

#include <algorithm>

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

// Like Claude Code: deny beats ask beats allow; what no rule allows asks. Rules are judged by the change's effect.
Repository::Verdict Repository::evaluate(const json& before, const json& after, const std::string& author,
                                         bool reviewed) const {
	using Kind = Verdict::Kind;
	const Permissions permissions = rules_.permissions(mainDocument());
	const detail::Policy policy = detail::policyOf(mainDocument());
	const auto covers = [&](const PermissionRule& rule) { return ruleCovers(rule.author, author, roles_); };

	for(const PermissionRule& rule : permissions.deny)
		if(covers(rule) && detail::changes(rules_, before, after, rule.pattern))
			return {Kind::Deny, "permission denied: '" + author + "' may not change " +
			                        (rule.pattern.empty() ? std::string("this screen") : rule.pattern) + " (rule \"" +
			                        rule.text + "\")"};
	if(const auto denied = detail::dataDenial(rules_, policy, before, after, author, roles_))
		return {Kind::Deny, "permission denied: " + *denied};
	if(!reviewed) return {Kind::Allow, {}};
	if(const auto leased = leaseDenial(before, after, author)) return {Kind::Deny, "permission denied: " + *leased};
	if(const auto over = detail::overBudget(policy, before, after, author, roles_))
		return {over->deny ? Kind::Deny : Kind::Ask, (over->deny ? "permission denied: " : "") + over->reason, true};
	if(const std::vector<std::string> lines = rules_.reviewLines(before, after); !lines.empty()) {
		std::string reason = "always reviewed:";
		for(std::size_t i = 0; i < lines.size(); ++i) reason += (i ? "; " : " ") + lines[i];
		return {Kind::Ask, reason, true};
	}
	if(!permissions.configured) return {Kind::Allow, {}};

	for(const PermissionRule& rule : permissions.ask)
		if(covers(rule) && detail::changes(rules_, before, after, rule.pattern))
			return {Kind::Ask, "the rule \"" + rule.text + "\" asks for review", true};

	if(mode_ == Mode::Plan && !mayReview(author))
		return {Kind::Ask, "plan mode: only reviewers change the screen directly"};

	std::vector<std::string> allowed;
	for(const PermissionRule& rule : permissions.allow)
		if(covers(rule)) allowed.push_back(rule.pattern);
	for(const Grant& grant : grants_)
		if(const auto rule = parsePermissionRule(grant.rule);
		   rule && !expired(grant) && detail::authorMatches(rule->author, author))
			allowed.push_back(rule->pattern);
	if(detail::onlyInside(rules_, before, after, allowed)) return {Kind::Allow, {}};

	if(mode_ == Mode::AcceptEdits) return {Kind::Allow, {}};
	return {Kind::Ask, "no rule allows '" + author + "' to make this change"};
}

// Without rules anyone may; otherwise the reviewers, by exact name or channel ("studio" covers "studio/riccardo")
bool Repository::mayReview(std::string_view author) const {
	const Permissions permissions = rules_.permissions(mainDocument());
	if(!permissions.configured) return true;
	return std::ranges::any_of(permissions.reviewers, [&](const std::string& reviewer) {
		return reviewer == author || reviewer == detail::channelOf(author);
	});
}

void Repository::setMode(Mode mode) {
	if(keptMode_ != mode) {
		keptMode_ = mode;
		saveHistory();
	}
	setModeForThisRun(mode);
}

void Repository::setModeForThisRun(Mode mode) {
	if(mode_ == mode) return;
	mode_ = mode;
	onReviewChanged();
}

} // namespace affresco::vcs
