#pragma once

/// @file
/// @brief The VersionControl and VersionObserver interfaces: a live document under version control.

#include "affresco/core/Json.hpp"
#include "affresco/core/Signal.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

/// @brief What happened to a submitted revision.
enum class RevisionStatus {
	Applied,  ///< Committed on main: the document changed.
	Proposed, ///< Waiting for a reviewer, on a proposal branch.
	Rejected, ///< Nothing changed; RevisionResult::error says why.
};

/// @brief How much a reviewer is asked, named after Claude Code's permission modes.
enum class Mode {
	Default,     ///< The rules decide; a change no rule allows waits for review.
	AcceptEdits, ///< A change that would wait only because no rule allows it is applied. Ask and deny rules still hold.
	Plan,        ///< Authors who aren't reviewers can only propose: their allow rules and grants are suspended.
};

/// @brief The outcome of VersionControl::submit.
struct RevisionResult {
	RevisionStatus status; ///< Applied, proposed or rejected.
	int revision = 0;      ///< main's revision number after the call.
	std::string commit;    ///< Applied: the new commit on main. Proposed: the proposal's head commit.
	int proposal = 0;      ///< When proposed: its id.
	std::string error;     ///< When rejected: why.
};

/// @brief A commit: one version of the document, named by its SHA-256.
struct CommitInfo {
	std::string id;                   ///< 64 hex digits; any unique prefix of 4 or more names it (Git shows 7).
	int number = 0;                   ///< Its revision number: the position on main, 0 being the document as loaded.
	std::vector<std::string> parents; ///< None for the first commit, two for a merged proposal (main, then branch).
	std::string author;               ///< Who wrote the change.
	std::string committer;            ///< Who applied it: the author, or the reviewer who accepted it.
	std::string message, time;        ///< Why, and when (UTC, ISO 8601).
	int proposal = 0;                 ///< The proposal it merged, if any.
};

/// @brief An open proposal: a branch waiting for review, like a pull request.
struct ProposalInfo {
	int id = 0;                        ///< For accept(), reject() and preview().
	std::string branch;                ///< "proposal/<id>".
	std::string author, message;       ///< Who proposed it, and its title (the first commit's message).
	std::string base, head;            ///< The main commit it is based on, and its newest commit.
	std::size_t commits = 0;           ///< Commits on the branch.
	std::size_t operations = 0;        ///< JSON Patch operations in all of them.
	std::vector<std::string> touches;  ///< What it changes: nodes ("#board") or areas ("/state/cards").
	std::vector<std::string> remember; ///< The rules "don't ask again" would add; empty if it can't be remembered.
	std::vector<std::string> changes;  ///< Its operations as lines for a reviewer: "set #statusBar/color to @topBar".
	std::string reason;                ///< Why it waits for review.
	std::string time;                  ///< When it was opened (UTC, ISO 8601).
};

/// @brief A reviewer's "don't ask again": an allow rule that lasts until a reviewer revokes it. It is kept with the
/// history: in memory, or on disk for a screen opened from a file.
struct Grant {
	std::string rule;      ///< e.g. "agent(#board)".
	std::string grantedBy; ///< The reviewer.
	int proposal = 0;      ///< The proposal it was granted with.
	std::string time;      ///< UTC, ISO 8601.
};

/// @brief A reviewer accepting a proposal.
struct Approval {
	std::string reviewer;      ///< Who accepts: the merge commit's committer. Never the proposal's own author.
	std::string head;          ///< The head commit the reviewer saw (7+ characters); refused if the branch moved since.
	bool dontAskAgain = false; ///< Also grant ProposalInfo::remember, so changes like it apply without asking.
};

/// @brief A reviewer's decision on a proposal.
struct Decision {
	int proposal;                     ///< The proposal's id.
	std::optional<std::string> error; ///< std::nullopt: accepted and merged; otherwise why it was rejected.
	int revision;                     ///< main's revision number after the decision.
	std::string commit;               ///< When merged: the merge commit.
};

/// @brief Told when the working copy of a VersionControl changes. There is one observer: the renderer.
class VersionObserver {
public:
	virtual ~VersionObserver() = default;

	/// @brief The document or the state was replaced (commit, preview, restore, import).
	/// @throws DocumentError To refuse the new version: it is rolled back, and this is called again for the old one.
	virtual void documentChanged() = 0;

	/// @brief Only the proposals changed.
	virtual void proposalsChanged() = 0;
};

/// @brief Version control for a live document, like Git, with approvals like Claude Code's.
///
/// Commits are content-addressed (SHA-256) and chained by their parents. "main" is the live branch; it only grows,
/// so the history is an audit log that can't be rewritten. A change the permission rules don't allow becomes a
/// proposal: a branch its author can keep pushing to, which a reviewer previews and then merges (a merge commit,
/// committed by the reviewer) or rejects. Every change is atomic.
///
/// Revisions are `{"author", "message", "parent"?, "proposal"?, "patch": [ops]}`: patch paths may start with "#id"
/// (a node) or "/state" (the live state rather than the document); "proposal" pushes onto the author's own open
/// proposal. Revisions are named by a number on main, a commit id or a unique prefix of one, "main", "HEAD",
/// "HEAD~n" or "proposal/<id>". Implemented by vcs::Repository.
class VersionControl {
public:
	virtual ~VersionControl() = default;

	/// @brief Sets who is told about changes to the working copy.
	/// @param observer The observer, or nullptr to stop.
	virtual void setObserver(VersionObserver* observer) = 0;

	/* Working copy */

	/// @brief The document: main's head, or the proposal being previewed.
	[[nodiscard]] virtual const json& document() const = 0;
	/// @brief The live state: changed freely by actions, patched by revisions.
	[[nodiscard]] virtual json& state() = 0;
	/// @brief The live state.
	[[nodiscard]] virtual const json& state() const = 0;
	/// @brief main's head.
	[[nodiscard]] virtual CommitInfo head() const = 0;

	/* Changes */

	/// @brief Applies, proposes or rejects a revision, following the permission rules and the mode.
	/// @param revision The revision.
	/// @return What happened.
	virtual RevisionResult submit(const json& revision) = 0;

	/// @brief Commits a revision from the application itself: not reviewed, but deny rules apply. Ends any preview.
	/// @param revision The revision.
	/// @return The error, or std::nullopt if committed.
	virtual std::optional<std::string> commit(const json& revision) = 0;

	/// @brief Finds a commit, like `git rev-parse`.
	/// @param revision A number on main, a commit id or prefix, "main", "HEAD", "HEAD~n" or "proposal/<id>".
	/// @return The commit.
	/// @throws DocumentError If there is no such commit, or the prefix is ambiguous.
	[[nodiscard]] virtual CommitInfo resolve(const json& revision) const = 0;

	/// @brief Builds a revision that brings the whole document back to a commit, like `git restore --source`.
	/// @param revision The commit to go back to.
	/// @param author The new revision's author.
	/// @return The revision, for submit() or commit().
	/// @throws DocumentError If there is no such commit or the document already matches it.
	[[nodiscard]] virtual json restoreRevision(const json& revision, std::string_view author) const = 0;

	/// @brief Builds a revision that undoes one commit on main and keeps everything after it, like `git revert`.
	/// @param revision The commit to undo.
	/// @param author The new revision's author.
	/// @return The revision, for submit() or commit().
	/// @throws DocumentError If it isn't on main, or later commits changed the same parts (a conflict).
	[[nodiscard]] virtual json revertRevision(const json& revision, std::string_view author) const = 0;

	/// @brief Compares two commits' documents, like `git diff`.
	/// @param from The older commit.
	/// @param to The newer commit.
	/// @return A JSON Patch from one to the other.
	/// @throws DocumentError If either commit is unknown.
	[[nodiscard]] virtual json diff(const json& from, const json& to) const = 0;

	/* Review */

	/// @brief The open proposals, oldest first.
	[[nodiscard]] virtual std::vector<ProposalInfo> proposals() const = 0;

	/// @brief What accepting a proposal would change now ("/state/..." paths for the live state).
	/// @param proposal The proposal's id.
	/// @return A JSON Patch from main's head.
	/// @throws DocumentError If there is no such proposal or it no longer applies.
	[[nodiscard]] virtual json proposalDiff(int proposal) const = 0;

	/// @brief Merges a proposal into main; onDecision fires. It stays open if the approval itself is refused.
	/// @param proposal The proposal's id.
	/// @param approval Who accepts, what they saw, and whether to stop asking for changes like it.
	/// @return The error, or std::nullopt if merged.
	virtual std::optional<std::string> accept(int proposal, const Approval& approval) = 0;

	/// @brief Closes a proposal without merging it; its author is told why.
	/// @param proposal The proposal's id.
	/// @param reason What to do differently.
	/// @return false if there is no such proposal.
	virtual bool reject(int proposal, std::string_view reason) = 0;

	/// @brief Shows a proposal live, as accepting it would, until endPreview() or a commit.
	/// @param proposal The proposal's id.
	/// @return The error, or std::nullopt.
	virtual std::optional<std::string> preview(int proposal) = 0;
	/// @brief Goes back to main's head after preview().
	virtual void endPreview() = 0;
	/// @brief The proposal being previewed, if any.
	[[nodiscard]] virtual std::optional<int> previewing() const = 0;

	/// @brief Tells whether an author may review.
	/// @param author The author's full name.
	/// @return true for a reviewer.
	[[nodiscard]] virtual bool mayReview(std::string_view author) const = 0;

	/// @brief Sets how much reviewers are asked. Proposals already open stay until someone decides.
	/// @param mode The mode.
	virtual void setMode(Mode mode) = 0;
	/// @brief How much reviewers are asked.
	[[nodiscard]] virtual Mode mode() const = 0;

	/// @brief The "don't ask again" grants, oldest first.
	[[nodiscard]] virtual std::vector<Grant> grants() const = 0;
	/// @brief Withdraws a grant.
	/// @param rule The grant's rule.
	/// @return false if there is no such grant.
	virtual bool revokeGrant(std::string_view rule) = 0;

	/* History */

	/// @brief main's commits, oldest first, like `git log --first-parent --reverse`.
	[[nodiscard]] virtual std::vector<CommitInfo> log() const = 0;

	/// @brief Exports a change set ("plugin"), portable to other copies of the same document.
	/// @param from Commits on main after this one are included.
	/// @param name The change set's name.
	/// @return The change set.
	/// @throws DocumentError If @p from is not on main.
	[[nodiscard]] virtual json exportChanges(const json& from, std::string_view name) const = 0;

	/// @brief Applies a change set on top of the current document, all or nothing.
	/// @param changes A change set from exportChanges().
	/// @return The error, or std::nullopt if every revision applied.
	virtual std::optional<std::string> importChanges(const json& changes) = 0;

	/* Notifications, e.g. for every connected peer (the VersionObserver is for the one that renders) */

	Signal<const Decision&> onDecision; ///< A reviewer accepted or rejected a proposal.
	Signal<const CommitInfo&> onCommit; ///< A commit landed on main, by anyone.
	Signal<> onReviewChanged;           ///< Proposals, preview, mode or grants changed.
};

/// @brief The name of a mode.
/// @param mode The mode.
/// @return "default", "acceptEdits" or "plan".
inline std::string toString(Mode mode) {
	return mode == Mode::AcceptEdits ? "acceptEdits" : mode == Mode::Plan ? "plan" : "default";
}

/// @brief Reads a mode's name.
/// @param name "default", "acceptEdits" or "plan".
/// @return The mode, or std::nullopt.
inline std::optional<Mode> modeFromString(std::string_view name) {
	if(name == "default") return Mode::Default;
	if(name == "acceptEdits") return Mode::AcceptEdits;
	if(name == "plan") return Mode::Plan;
	return std::nullopt;
}

/// @brief Converts a commit to its live-protocol JSON.
/// @param c The commit.
/// @return `{"id", "short", "number", "parents", "author", "committer", "message", "time", "proposal"}`.
inline json toJson(const CommitInfo& c) {
	return {{"id", c.id},
	        {"short", c.id.substr(0, 7)},
	        {"number", c.number},
	        {"parents", c.parents},
	        {"author", c.author},
	        {"committer", c.committer},
	        {"message", c.message},
	        {"time", c.time},
	        {"proposal", c.proposal ? json(c.proposal) : json()}};
}

/// @brief Converts a proposal to its live-protocol JSON.
/// @param p The proposal.
/// @return `{"id", "branch", "author", "message", "base", "head", "short", "commits", "operations", "touches",
/// "remember", "changes", "reason", "time"}`.
inline json toJson(const ProposalInfo& p) {
	return {{"id", p.id},
	        {"branch", p.branch},
	        {"author", p.author},
	        {"message", p.message},
	        {"base", p.base},
	        {"head", p.head},
	        {"short", p.head.substr(0, 7)},
	        {"commits", p.commits},
	        {"operations", p.operations},
	        {"touches", p.touches},
	        {"remember", p.remember},
	        {"changes", p.changes},
	        {"reason", p.reason},
	        {"time", p.time}};
}

/// @brief Converts a grant to its live-protocol JSON.
/// @param g The grant.
/// @return `{"rule", "grantedBy", "proposal", "time"}`.
inline json toJson(const Grant& g) {
	return {{"rule", g.rule}, {"grantedBy", g.grantedBy}, {"proposal", g.proposal}, {"time", g.time}};
}

} // namespace affresco
