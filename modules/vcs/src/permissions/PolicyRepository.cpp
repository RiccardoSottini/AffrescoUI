/// @file
/// @brief The parts of vcs::Repository that permission rules add: what a rule says about a change, who may review,
/// the modes, what an author may read, and the paths its patches name.

#include "affresco/vcs/Repository.hpp"

#include "permissions/Permissions.hpp"
#include "permissions/Policy.hpp"

#include "affresco/core/Json.hpp"

#include <algorithm>

namespace affresco::vcs {

namespace {

/// @brief A change as permission rules judge it: by its effect, for its author.
struct JudgedChange {
	const DocumentRules& rules; ///< Finds "#id" nodes.
	const json& before;         ///< The view before.
	const json& after;          ///< The view after.
	const std::string& author;  ///< Who makes it.
	const Roles& roles;         ///< Who plays which role.
};

/// @brief The first rule of a list that covers a change's author and something the change alters.
/// @param rules The rules, in order.
/// @param change The change.
/// @return The rule, or nullptr.
const PermissionRule* firstCovering(const std::vector<PermissionRule>& rules, const JudgedChange& change) {
	const auto it = std::ranges::find_if(rules, [&](const PermissionRule& rule) {
		return ruleCovers(rule.author, change.author, change.roles) &&
		       detail::changes(change.rules, change.before, change.after, rule.pattern);
	});
	return it == rules.end() ? nullptr : &*it;
}

/// @brief Why a change is refused whatever the mode and whoever reviews: a deny rule, or the author's data rules.
/// @param change The change.
/// @param deny The deny rules.
/// @param policy The data rules.
/// @return The reason, "permission denied: ...", or std::nullopt.
std::optional<std::string> refusal(const JudgedChange& change, const std::vector<PermissionRule>& deny,
                                   const detail::Policy& policy) {
	if(const PermissionRule* rule = firstCovering(deny, change))
		return "permission denied: '" + change.author + "' may not change " +
		       (rule->pattern.empty() ? std::string("this screen") : rule->pattern) + " (rule \"" + rule->text + "\")";
	if(const auto denied =
	       detail::dataDenial(change.rules, policy, change.before, change.after, {change.author, change.roles}))
		return "permission denied: " + *denied;
	return std::nullopt;
}

/// @brief What a change asks of review whatever the permission rules allow: a budget it goes over refuses it or asks
/// for review, and what the format always reviews (DocumentRules::reviewLines()) asks for it.
/// @param change The change.
/// @param policy The budgets.
/// @return Deny or ask, with the reason ("permission denied: ..." for deny), or std::nullopt.
std::optional<detail::PolicyVerdict> reviewDemand(const JudgedChange& change, const detail::Policy& policy) {
	if(auto over = detail::overBudget(policy, change.before, change.after, change.author, change.roles)) {
		if(over->deny) over->reason = "permission denied: " + over->reason;
		return over;
	}
	const std::vector<std::string> lines = change.rules.reviewLines(change.before, change.after);
	if(lines.empty()) return std::nullopt;
	std::string reason = "always reviewed:";
	for(std::size_t i = 0; i < lines.size(); ++i) reason += (i ? "; " : " ") + lines[i];
	return detail::PolicyVerdict{false, std::move(reason)};
}

/// @brief The patterns a change's author may change without asking: its allow rules', and its grants' that are
/// still in force.
/// @param change The change.
/// @param allow The allow rules.
/// @param grants The "don't ask again" grants.
/// @param expired Tells whether a grant has expired.
/// @return The patterns.
std::vector<std::string> allowedPatterns(const JudgedChange& change, const std::vector<PermissionRule>& allow,
                                         const std::vector<Grant>& grants, bool (*expired)(const Grant&)) {
	std::vector<std::string> allowed;
	for(const PermissionRule& rule : allow)
		if(ruleCovers(rule.author, change.author, change.roles)) allowed.push_back(rule.pattern);
	for(const Grant& grant : grants)
		if(const auto rule = parsePermissionRule(grant.rule);
		   rule && !expired(grant) && detail::authorMatches(rule->author, change.author))
			allowed.push_back(rule->pattern);
	return allowed;
}

} // namespace

void Repository::checkReadable(const json& patch, const std::string& author) const {
	const detail::Policy policy = detail::policyOf(mainDocument());
	if(!detail::readLimited(policy, author, roles_)) return;
	const json view = detail::view(mainDocument(), committedState());
	if(const auto unreadable = detail::unreadablePath(rules_, policy, view, patch, {author, roles_}))
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
	const JudgedChange change{rules_, before, after, author, roles_};

	if(auto refused = refusal(change, permissions.deny, policy)) return {Kind::Deny, std::move(*refused)};
	if(!reviewed) return {Kind::Allow, {}};
	if(const auto leased = leaseDenial(before, after, author)) return {Kind::Deny, "permission denied: " + *leased};
	if(auto demand = reviewDemand(change, policy))
		return {demand->deny ? Kind::Deny : Kind::Ask, std::move(demand->reason), true};
	if(!permissions.configured) return {Kind::Allow, {}};

	if(const PermissionRule* rule = firstCovering(permissions.ask, change))
		return {Kind::Ask, "the rule \"" + rule->text + "\" asks for review", true};
	if(mode_ == Mode::Plan && !mayReview(author))
		return {Kind::Ask, "plan mode: only reviewers change the screen directly"};
	if(detail::onlyInside(rules_, before, after, allowedPatterns(change, permissions.allow, grants_, &expired)) ||
	   mode_ == Mode::AcceptEdits)
		return {Kind::Allow, {}};
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
