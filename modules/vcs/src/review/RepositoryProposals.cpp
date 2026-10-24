/// @file
/// @brief vcs::Repository's proposals, a branch each: opening and extending them, listing them for review,
/// previewing, accepting and rejecting them (VERSION_CONTROL.md, "Review: proposals are branches").

#include "affresco/vcs/Repository.hpp"

#include "revisions/Budget.hpp"
#include "revisions/Revisions.hpp"
#include "permissions/Permissions.hpp"
#include "Reasons.hpp"

#include <algorithm>

namespace affresco::vcs {

using expect::fail;

namespace {

/// @brief A short preview of a value for a reviewer: text as is, the rest as JSON, cut at 48 characters.
/// @param value The value.
/// @return The preview.
std::string preview(const json& value) {
	std::string text = value.is_string() ? value.get<std::string>() : value.dump();
	if(text.size() > 48) text = text.substr(0, 45) + "...";
	return text;
}

/// @brief One operation as a line for a reviewer: "set #statusBar/color to @topBar", "remove #badge".
/// @param op The operation.
/// @return The line.
std::string summary(const json& op) {
	const std::string kind = op.value("op", "");
	const std::string path = op.value("path", "");
	if(kind == "remove") return "remove " + path;
	if(kind == "move" || kind == "copy") return kind + " " + op.value("from", "") + " to " + path;
	if(kind == "test") return "check " + path;
	if(kind == "add") return "add at " + path + ": " + preview(op.value("value", json()));
	return "set " + path + " to " + preview(op.value("value", json()));
}

} // namespace

std::vector<Repository::Commit> Repository::branchOf(const Proposal& proposal) const {
	std::vector<Commit> commits;
	for(std::string id = proposal.head; id != proposal.base; id = objects_.at(id).parents.front())
		commits.insert(commits.begin(), objects_.at(id));
	return commits;
}

// The commits are replayed in order on main's head (unchanged ones keep their ids)
json Repository::replay(Proposal& proposal, std::vector<Commit> commits) {
	json document = mainDocument(), state = committedState(), patch = json::array();
	std::string parent = main_.back();
	int number = mainHead().number;
	std::vector<std::string> touches;
	json run = json::array();
	for(Commit& c : commits) {
		if(c.run.is_array()) run.insert(run.end(), c.run.begin(), c.run.end());
		Prepared next = prepare(c.patch, document, state);
		for(std::string& area : next.touches)
			if(std::ranges::find(touches, area) == touches.end()) touches.push_back(std::move(area));
		state = std::move(next.state);
		c.patch = std::move(next.patch);
		patch.insert(patch.end(), c.patch.begin(), c.patch.end());
		c.parents = {parent};
		c.number = ++number;
		parent = store(std::move(c), next.document, &document).id;
		document = std::move(next.document);
	}

	const json before = detail::view(mainDocument(), committedState()), after = detail::view(document, state);
	if(run.size() > 1000) fail("revision.run", "a proposal runs at most 1,000 steps");
	if(!run.empty()) rules_.checkRun(after, run); // on the document it will run on
	const Verdict verdict = evaluate(before, after, proposal.author, true, run);
	if(verdict.kind == Verdict::Kind::Deny) fail("revision", verdict.reason);

	proposal.base = main_.back();
	proposal.head = parent;
	proposal.commits = commits.size();
	proposal.patch = std::move(patch);
	proposal.reason = verdict.kind == Verdict::Kind::Ask ? verdict.reason : "proposals are always reviewed";
	proposal.remember = rememberedRules(proposal.author, touches, verdict.askedByRule);
	proposal.touches = std::move(touches);
	proposal.conflicts = json::array();
	proposal.behaviour = rules_.behaviourLines(before, after);
	proposal.policy = reviewNeeds(before, after, {}).first;
	proposal.person = personNeeds(before, after, run);
	proposal.assets = assetChanges(before, after);
	proposal.run = std::move(run);
	return document;
}

// Rebased on main's head, with the new commit last; what was approved before no longer is
void Repository::extend(Proposal& proposal, const json& revision, json changes) {
	std::vector<Commit> commits = branchOf(proposal);
	Commit added;
	added.author = added.committer = proposal.author;
	added.message = detail::messageOf(revision);
	added.time = detail::utcNow();
	added.patch = std::move(changes);
	added.run = runOf(revision);
	commits.push_back(std::move(added));
	// accept() replays them all as one patch: refused now, not once a reviewer has looked at it
	std::size_t operations = 0;
	for(const Commit& c : commits) operations += c.patch.size();
	detail::expectFewEnoughOperations(operations);

	replay(proposal, std::move(commits));
	proposal.approvals.clear();
	proposal.reviewedHeads.clear();
}

std::vector<std::string> Repository::rememberedRules(const std::string& author, const std::vector<std::string>& touches,
                                                     bool askedByRule) {
	std::vector<std::string> rules;
	if(!askedByRule && !touches.empty() && std::ranges::all_of(touches, detail::narrow))
		for(const std::string& area : touches) rules.push_back(detail::channelOf(author) + "(" + area + ")");
	return rules;
}

RevisionResult Repository::push(const json& revision, const std::string& author, const json& changes) {
	const int id = expect::integer(revision["proposal"], "revision.proposal");
	Proposal* proposal = findProposal(id);
	if(!proposal) fail("revision.proposal", "no open proposal " + std::to_string(id));
	if(proposal->author != author)
		fail("revision.proposal",
		     "proposal #" + std::to_string(id) + " is by '" + proposal->author + "': only its author may push to it");
	checkParent(revision);

	Proposal updated = *proposal;
	extend(updated, revision, changes);
	*proposal = std::move(updated);
	saveHistory();
	if(previewing_ == id) endPreview(); // what was shown is out of date
	proposalsChanged();
	return {RevisionStatus::Proposed, mainHead().number, proposal->head, id, {}};
}

std::vector<ProposalInfo> Repository::proposals() const {
	std::vector<ProposalInfo> result;
	for(const Proposal& p : proposals_) {
		std::vector<std::string> changes;
		for(const json& op : p.patch) changes.push_back(summary(op));
		ProposalInfo& info = result.emplace_back(
		    ProposalInfo{p.id, "proposal/" + std::to_string(p.id), p.author, p.message, p.base, p.head, p.commits,
			             p.patch.size(), p.touches, p.remember, std::move(changes), p.reason, p.time});
		info.conflicts = p.conflicts;
		info.approvals = p.approvals;
		info.policy = p.policy;
		info.behaviour = p.behaviour;
		info.assets = p.assets;
		info.run = p.run;
		info.person = p.person;
	}
	return result;
}

json Repository::proposalDiff(int proposal) const {
	const Proposal* open = findProposal(proposal);
	if(!open) fail("diff", "no proposal " + std::to_string(proposal));
	const Prepared next = prepare(open->patch, mainDocument(), committedState());
	return json::diff(next.before, next.after);
}

// Always a merge commit (like `git merge --no-ff`): the history keeps who approved it, and the branch's commits
std::optional<std::string> Repository::accept(int proposal, const Approval& approval) {
	Proposal* open = findProposal(proposal);
	if(!open) return "AffrescoUI screen: no proposal " + std::to_string(proposal);

	// Refused approvals leave the proposal open
	if(std::optional<std::string> refused = checkApproval(*open, approval)) return refused;
	if(const std::string missing = approve(*open, approval.reviewer); !missing.empty()) {
		open->reason =
		    "approved by " + std::to_string(detail::countedApprovals(open->approvals)) + ", waiting for " + missing;
		saveHistory();
		proposalsChanged();
		return std::nullopt;
	}

	const Proposal taken = *open;
	std::erase_if(proposals_, [proposal](const Proposal& p) { return p.id == proposal; });
	proposalsChanged();

	std::optional<std::string> error;
	std::string merged;
	try {
		// main may have moved since the proposal was built
		Prepared next = [&] {
			try {
				return prepare(taken.patch, mainDocument(), committedState());
			} catch(const DocumentError& failure) {
				const std::string why = detail::reasonOf(failure);
				fail("proposal #" + std::to_string(proposal),
				     "no longer applies on " + describe(mainHead()) + " (" + why + "): ask its author for a new one");
			}
		}();
		if(const Verdict verdict = evaluate(next.before, next.after, taken.author, false);
		   verdict.kind == Verdict::Kind::Deny)
			fail("revision", verdict.reason);
		if(!taken.run.empty()) rules_.checkRun(next.after, taken.run); // main may have moved: on what it runs on now

		Commit merge;
		merge.parents = {taken.head};
		merge.author = taken.author;
		merge.committer = approval.reviewer;
		merge.message = taken.message;
		merge.time = detail::utcNow();
		merge.proposal = proposal;
		merge.patch = taken.patch;
		if(!taken.run.empty()) merge.run = taken.run; // run once, now, by the host (onCommit)
		merged = land(std::move(merge), std::move(next.document), std::move(next.state)).id;

		if(approval.dontAskAgain) {
			for(const std::string& rule : taken.remember)
				if(std::ranges::find(grants_, rule, &Grant::rule) == grants_.end())
					grants_.push_back({rule, approval.reviewer, proposal, detail::utcNow(), grantsExpire(approval)});
			onReviewChanged();
		}
	} catch(const DocumentError& failure) {
		error = failure.what();
	}
	saveHistory(); // the proposal is closed, merged or not, and grants may have been added
	onDecision({proposal, error, mainHead().number, merged});
	return error;
}

bool Repository::reject(int proposal, std::string_view reason) {
	if(!findProposal(proposal)) return false;

	if(previewing_ == proposal) endPreview();
	std::erase_if(proposals_, [proposal](const Proposal& p) { return p.id == proposal; });
	saveHistory();
	proposalsChanged();
	onDecision({proposal,
	            "rejected by the reviewer" + (reason.empty() ? std::string() : ": " + std::string(reason)),
	            mainHead().number,
	            {}});
	return true;
}

std::optional<std::string> Repository::preview(int proposal) {
	const Proposal* open = findProposal(proposal);
	if(!open) return "AffrescoUI screen: no proposal " + std::to_string(proposal);

	endPreview();
	try {
		Prepared next = prepare(open->patch, document_, state_);
		committedState_ = std::exchange(state_, std::move(next.state));
		committedDocument_ = std::exchange(document_, std::move(next.document));
		previewing_ = proposal;
		documentChanged();
	} catch(const DocumentError& error) {
		endPreview();
		return error.what();
	}
	onReviewChanged();
	return std::nullopt;
}

void Repository::endPreview() {
	if(!previewing_) return;
	previewing_.reset();
	document_ = std::move(committedDocument_);
	state_ = std::move(committedState_);
	documentChanged();
	onReviewChanged();
}

} // namespace affresco::vcs
