/// @file
/// @brief vcs::Runs, the rules: who may run which command (the rules of `permissions.run`, the mode, the reviewers and
/// the grants, in the order VERSION_CONTROL.md, "Running commands", gives), and a run's state changes judged by the
/// runner's data rules.

#include "affresco/vcs/Runs.hpp"

#include "permissions/Permissions.hpp"
#include "permissions/Policy.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace affresco::vcs {

namespace {

/// @brief The system's clock.
/// @return Milliseconds since 1970 (UTC).
std::int64_t systemMilliseconds() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
	    .count();
}

/// @brief Tells whether a run rule covers a runner and a command.
/// @param rule The rule: its author "*", a channel or a full name (a `role:` author covers nobody); its pattern
/// empty, the command's name, or a prefix ending in '*'.
/// @param author The runner.
/// @param command The command's name.
/// @return true if it does.
bool covers(const PermissionRule& rule, std::string_view author, std::string_view command) {
	if(rule.author.starts_with("role:") || !detail::authorMatches(rule.author, author)) return false;
	const std::string_view pattern = rule.pattern;
	if(pattern.empty() || pattern == command) return true;
	return pattern.ends_with('*') && command.starts_with(pattern.substr(0, pattern.size() - 1));
}

/// @brief The first rule of a list that covers a runner and a command.
/// @param rules The rules, in the order written.
/// @param author The runner.
/// @param command The command's name.
/// @return It, or nullptr.
const PermissionRule* firstCovering(const std::vector<PermissionRule>& rules, std::string_view author,
                                    std::string_view command) {
	const auto found =
	    std::ranges::find_if(rules, [&](const PermissionRule& rule) { return covers(rule, author, command); });
	return found == rules.end() ? nullptr : &*found;
}

/// @brief Why a command no peer may run is denied.
/// @param command The command's name.
/// @param reach What it reaches.
/// @return e.g. "'export' reaches the control step \"call\", which only the person may run".
std::string controlReason(std::string_view command, const CommandReach& reach) {
	const std::string& step = reach.controlStep;
	// "accept" names a step; "a computed run" already says what it is
	const std::string what = step.empty()         ? std::string("a control step")
	                         : step.contains(' ') ? step
	                                              : "the control step \"" + step + "\"";
	return "'" + std::string(command) + "' reaches " + what + ", which only the person may run";
}

/// @brief Why a command waits for the person's yes.
/// @param command The command's name.
/// @param uses What it reaches that writes or runs.
/// @return e.g. "'export' needs the person's yes each time: invoke files.write (writes files in 'workspace')".
std::string personReason(std::string_view command, const std::vector<std::string>& uses) {
	std::string reason = "'" + std::string(command) + "' needs the person's yes each time: ";
	for(std::size_t i = 0; i < uses.size(); ++i) reason += (i ? "; " : "") + uses[i];
	return reason;
}

} // namespace

Runs::Runs(VersionControl& versions, const DocumentRules& rules, Clock clock)
    : versions_(versions), rules_(rules), clock_(clock ? std::move(clock) : Clock(systemMilliseconds)) {}

const Permissions& Runs::permissions() const {
	// DocumentRules::permissions walks the document: read it again only when the document changed
	std::string head = versions_.head().id;
	if(!permissions_ || head != permissionsHead_) {
		permissions_ = rules_.permissions(versions_.document());
		permissionsHead_ = std::move(head);
	}
	return *permissions_;
}

RunJudgement Runs::judge(std::string_view author, std::string_view command, const CommandReach& reach) const {
	const Permissions& rules = permissions();
	const std::string name(command);
	if(const PermissionRule* rule = firstCovering(rules.run.deny, author, command))
		return {RunVerdict::Deny, "denied by the run rule '" + rule->text + "'"};
	if(reach.control) return {RunVerdict::Deny, controlReason(command, reach)};
	const bool reviewer = versions_.mayReview(author);
	if(versions_.mode() == Mode::Plan && !reviewer)
		return {RunVerdict::Deny, "plan mode: commands don't run; say what you would run"};
	if(!reach.personUses.empty()) return {RunVerdict::Person, personReason(command, reach.personUses)};
	if(reviewer) return {RunVerdict::Allow, {}};
	if(const PermissionRule* rule = firstCovering(rules.run.ask, author, command))
		return {RunVerdict::Ask, "asked by the run rule '" + rule->text + "'"};
	if(firstCovering(rules.run.allow, author, command) || granted(author, command)) return {RunVerdict::Allow, {}};
	if(!rules.configured) return {RunVerdict::Allow, {}};
	const std::string channel = detail::channelOf(author);
	return {RunVerdict::Ask, "no run rule allows '" + name + "' for '" + channel + "'; a rule \"" + channel + "(" +
	                             name + ")\" in permissions.run.allow would"};
}

std::optional<std::string> Runs::stateDenial(std::string_view author, const json& stateBefore,
                                             const json& stateAfter) const {
	const json& document = versions_.document();
	const detail::Policy policy = detail::policyOf(document);
	if(policy.data.empty()) return std::nullopt;
	const json before = detail::view(document, stateBefore);
	const json after = detail::view(document, stateAfter);
	return detail::dataDenial(rules_, policy, before, after, detail::Actor{author, roles_});
}

} // namespace affresco::vcs
