/// @file
/// @brief vcs::Repository: review at scale. Proposals rebase themselves onto a newer main through a three-way merge
/// (Merge.hpp), approvals add up to what the review policy asks, and "don't ask again" grants can expire
/// (VERSION_CONTROL.md, "Rebasing" and "Review policies").

#include "affresco/vcs/Repository.hpp"

#include "revisions/Budget.hpp"
#include "review/Merge.hpp"
#include "permissions/Permissions.hpp"
#include "Reasons.hpp"
#include "revisions/Revisions.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <limits>

namespace affresco::vcs {

namespace {

/// Longest a grant may last: a year.
constexpr long long maxGrantSeconds = 366LL * 24 * 3600;

/// @brief Reads a duration: a whole number and a unit, "30m", "8h" or "7d".
/// @param text The duration.
/// @return Seconds, or std::nullopt if it isn't one or is longer than maxGrantSeconds.
std::optional<long long> secondsOf(std::string_view text) {
	if(text.size() < 2) return std::nullopt;
	long long count = 0;
	const std::string_view digits = text.substr(0, text.size() - 1);
	const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), count);
	if(error != std::errc() || end != digits.data() + digits.size() || count < 1) return std::nullopt;
	const char unit = text.back();
	const long long scale = unit == 'm' ? 60 : unit == 'h' ? 3600 : unit == 'd' ? 86400 : 0;
	if(scale == 0 || count > maxGrantSeconds / scale) return std::nullopt;
	return count * scale;
}

/// @brief Tells whether a path is in the live state.
/// @param path A JSON Pointer.
/// @return true for "/state" and the paths inside it.
bool inState(const std::string& path) { return path == "/state" || path.starts_with("/state/"); }

/// @brief The paths of conflicts, for a message.
/// @param conflicts The conflicts.
/// @return e.g. "#title/text, /colors/accent".
std::string pathsOf(const json& conflicts) {
	std::string paths;
	for(std::size_t i = 0; i < conflicts.size() && i < 4; ++i)
		paths += (i ? ", " : "") + conflicts[i].value("path", std::string("/"));
	if(conflicts.size() > 4) paths += " and " + std::to_string(conflicts.size() - 4) + " more";
	return paths;
}

} // namespace

/* Rebasing */

bool Repository::rebaseProposals() {
	bool changed = false;
	for(Proposal& proposal : proposals_)
		if(proposal.base != main_.back()) changed = rebase(proposal) || changed;
	return changed;
}

// Exact when replaying the commits gives the merge: then the change is the one reviewed, and its approvals hold
bool Repository::rebase(Proposal& proposal) {
	detail::MergeResult merged =
	    detail::merge(documentOf(objects_.at(proposal.base)), mainDocument(), documentOf(objects_.at(proposal.head)));
	Proposal rebased = proposal;
	if(merged.clean()) {
		bool exact = false;
		try {
			exact = replay(rebased, branchOf(proposal)) == merged.merged;
		} catch(const DocumentError&) {
			exact = false; // its operations no longer apply as written: the merge does
		}
		try {
			if(exact) rebased.reviewedHeads.push_back(proposal.head);
			else squash(rebased = proposal, merged.merged);
		} catch(const DocumentError& failure) {
			merged.conflicts.push_back({{"path", "/"},
			                            {"reason", detail::reasonOf(failure)},
			                            {"base", nullptr},
			                            {"main", nullptr},
			                            {"proposal", nullptr}});
		}
	}
	if(!merged.clean()) {
		if(proposal.conflicts == merged.conflicts) return false;
		proposal.conflicts = std::move(merged.conflicts);
		proposal.reason = "conflicts with main at " + pathsOf(proposal.conflicts) + ": its author must push again";
		return true;
	}
	proposal = std::move(rebased);
	return true;
}

void Repository::squash(Proposal& proposal, const json& merged) {
	json patch = json::array();
	for(json& op : json::diff(mainDocument(), merged))
		if(!inState(op["path"].get<std::string>())) patch.push_back(std::move(op));
	for(const json& op : proposal.patch)
		if(inState(op.value("path", std::string()))) patch.push_back(op);
	detail::expectFewEnoughOperations(patch.size());

	Commit commit;
	commit.author = commit.committer = proposal.author;
	commit.message = "Rebase onto " + describe(mainHead());
	commit.time = detail::utcNow();
	commit.patch = std::move(patch);
	if(!proposal.run.empty()) commit.run = proposal.run; // the merge replaces the commits, not what they run
	replay(proposal, {std::move(commit)});
	proposal.approvals.clear(); // what it does now is the merge, which nobody reviewed
	proposal.reviewedHeads.clear();
}

/* Approvals and policies */

std::optional<std::string> Repository::checkApproval(const Proposal& proposal, const Approval& approval) const {
	const std::string name = "AffrescoUI screen: proposal #" + std::to_string(proposal.id);
	if(detail::channelOf(approval.reviewer) == detail::channelOf(proposal.author))
		return "AffrescoUI screen: permission denied: '" + approval.reviewer + "' may not accept its own proposal";
	// What brings content in or makes something happen is the person's to accept, judged on main as it is now
	if(detail::channelOf(approval.reviewer) != personChannel) {
		std::vector<std::string> person = proposal.person;
		try {
			const Prepared next = prepare(proposal.patch, mainDocument(), committedState());
			person = personNeeds(next.before, next.after, proposal.run);
		} catch(const DocumentError&) { // it no longer applies: as it was described, then
		}
		if(!person.empty())
			return "AffrescoUI screen: permission denied: only the person may accept proposal #" +
			       std::to_string(proposal.id) + " (it " + person.front() + (person.size() > 1 ? ", and more" : "") +
			       "): '" + approval.reviewer + "' may not";
	}
	// Every approval names what was reviewed: without it, a branch pushed to after the review would merge unseen
	if(approval.head.size() < 7)
		return "AffrescoUI screen: accepting proposal #" + std::to_string(proposal.id) +
		       " needs its head commit, the version you reviewed (at least 7 characters)";
	const auto reviewed = [&](const std::string& head) { return head.starts_with(approval.head); };
	if(!reviewed(proposal.head) && std::ranges::none_of(proposal.reviewedHeads, reviewed))
		return name + " changed since you reviewed it (it is now at " + proposal.head.substr(0, 7) +
		       "): review it again";
	if(approval.dontAskAgain && proposal.remember.empty())
		return name + " can't be remembered (it changes parts that always ask, or too much of the screen): accept "
		              "it without \"don't ask again\"";
	if(!approval.duration.empty() && !secondsOf(approval.duration))
		return "AffrescoUI screen: '" + approval.duration.substr(0, 40) +
		       "' is not a duration for grants: expected minutes, hours or days, e.g. \"30m\", \"8h\", \"7d\" (at "
		       "most a year)";
	// A conflicted proposal that still applies would overwrite main's side of the conflict
	if(!proposal.conflicts.empty()) try {
			(void)prepare(proposal.patch, mainDocument(), committedState());
			return name + " conflicts with main at " + pathsOf(proposal.conflicts) +
			       ": its author must push again (or reject it)";
		} catch(const DocumentError&) {
			return std::nullopt; // accept() closes it, saying why it no longer applies
		}
	return std::nullopt;
}

std::string Repository::approve(Proposal& proposal, const std::string& reviewer) {
	if(std::ranges::find(proposal.approvals, reviewer) == proposal.approvals.end())
		proposal.approvals.push_back(reviewer);
	try {
		// Judged on main as it is now, by the policy main has now: a rebase or an old policy can't lower the bar
		const Prepared next = prepare(proposal.patch, mainDocument(), committedState());
		auto [policy, missing] = reviewNeeds(next.before, next.after, proposal.approvals);
		proposal.policy = std::move(policy);
		return missing;
	} catch(const DocumentError&) {
		return {};
	}
}

std::pair<std::string, std::string> Repository::reviewNeeds(const json& before, const json& after,
                                                            const std::vector<std::string>& approvals) const {
	const ReviewPolicy policy = rules_.permissions(mainDocument()).review;
	std::size_t required = static_cast<std::size_t>(std::max(policy.approvals, 1));
	if(policy.behaviourApprovals > policy.approvals && !rules_.behaviourLines(before, after).empty())
		required = static_cast<std::size_t>(policy.behaviourApprovals);

	std::string asks = std::to_string(required) + (required == 1 ? " approval" : " approvals");
	const std::size_t counted = detail::countedApprovals(approvals);
	std::string missing;
	if(counted < required) {
		const std::size_t more = required - counted;
		missing = std::to_string(more) + (more == 1 ? " more approval" : " more approvals");
	}
	for(const auto& [pattern, owners] : policy.owners) {
		if(!detail::changes(rules_, before, after, pattern)) continue;
		std::string names;
		for(const std::string& owner : owners) {
			if(!names.empty()) names += ", ";
			names += owner;
		}
		const std::string owner = std::format("an owner of {} ({})", pattern, names);
		asks += ", one from " + owner;
		const bool approved = std::ranges::any_of(approvals, [&](const std::string& reviewer) {
			return std::ranges::any_of(owners,
			                           [&](const std::string& o) { return detail::authorMatches(o, reviewer); });
		});
		if(!approved) missing += (missing.empty() ? "" : "; ") + ("an approval from " + owner);
	}
	return {asks, missing};
}

/* Grants */

std::string Repository::grantsExpire(const Approval& approval) const {
	const std::optional<long long> asked = secondsOf(approval.duration);
	const std::optional<long long> longest = secondsOf(rules_.permissions(mainDocument()).review.grantsLast);
	if(!asked && !longest) return {};
	return detail::utcAfter(std::min(asked.value_or(std::numeric_limits<long long>::max()),
	                                 longest.value_or(std::numeric_limits<long long>::max())));
}

// ISO 8601 times in UTC sort as text
bool Repository::expired(const Grant& grant) { return !grant.expires.empty() && grant.expires <= detail::utcNow(); }

std::vector<Grant> Repository::grants() const {
	std::vector<Grant> active;
	std::ranges::copy_if(grants_, std::back_inserter(active), [](const Grant& grant) { return !expired(grant); });
	return active;
}

bool Repository::revokeGrant(std::string_view rule) {
	if(std::erase_if(grants_, [&](const Grant& g) { return g.rule == rule; }) == 0) return false;
	saveHistory();
	onReviewChanged();
	return true;
}

} // namespace affresco::vcs
