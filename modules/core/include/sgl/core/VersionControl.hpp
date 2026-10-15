#pragma once

/// @file
/// @brief The VersionControl and VersionObserver interfaces: a live document under version control.

#include "sgl/core/Json.hpp"
#include "sgl/core/Signal.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgl {

/// @brief What happened to a submitted revision.
enum class RevisionStatus {
	Applied,  ///< Committed: the document changed.
	Proposed, ///< Waiting for a reviewer (the author's policy is "review").
	Rejected, ///< Nothing changed; RevisionResult::error says why.
};

/// @brief The outcome of VersionControl::submit.
struct RevisionResult {
	RevisionStatus status; ///< Applied, proposed or rejected.
	int revision = 0;      ///< The current revision after the call.
	int proposal = 0;      ///< When proposed: its id.
	std::string error;     ///< When rejected: why.
};

/// @brief A proposal waiting for review.
struct ProposalInfo {
	int id;                      ///< For accept(), reject() and preview().
	std::string author, message; ///< Who sent it and why.
	std::size_t operations;      ///< JSON Patch operations in it.
	std::string time;            ///< UTC, ISO 8601.
};

/// @brief A committed revision.
struct HistoryEntry {
	int number;                        ///< 0 is the document as loaded.
	std::string author, message, time; ///< Who, why, when (UTC, ISO 8601).
};

/// @brief A reviewer's decision on a proposal.
struct Decision {
	int proposal;                     ///< The proposal's id.
	std::optional<std::string> error; ///< std::nullopt: accepted and applied; otherwise why it was rejected.
	int revision;                     ///< The current revision after the decision.
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

/// @brief Version control for a live document.
///
/// Keeps a history of revisions (JSON Patch), applies permissions per author, queues proposals for review, and
/// supports preview, restore and portable change sets. Every change is atomic. Revisions are
/// `{"author", "message", "parent"?, "patch": [ops]}`; patch paths may start with "#id" (a node) or "/state" (the
/// live state rather than the document). Implemented by vcs::Repository.
class VersionControl {
public:
	virtual ~VersionControl() = default;

	/// @brief Sets who is told about changes to the working copy.
	/// @param observer The observer, or nullptr to stop.
	virtual void setObserver(VersionObserver* observer) = 0;

	/* Working copy */

	/// @brief The document: the head, or the proposal being previewed.
	[[nodiscard]] virtual const json& document() const = 0;
	/// @brief The live state: changed freely by actions, patched by revisions.
	[[nodiscard]] virtual json& state() = 0;
	/// @brief The live state.
	[[nodiscard]] virtual const json& state() const = 0;
	/// @brief The current revision; 0 is the document as loaded.
	[[nodiscard]] virtual HistoryEntry head() const = 0;

	/* Changes */

	/// @brief Applies, proposes or rejects a revision, following the document's permissions.
	/// @param revision The revision.
	/// @return What happened.
	virtual RevisionResult submit(const json& revision) = 0;

	/// @brief Commits a revision from the application itself: not reviewed, but locks and rules apply. Ends any preview.
	/// @param revision The revision.
	/// @return The error, or std::nullopt if committed.
	virtual std::optional<std::string> commit(const json& revision) = 0;

	/// @brief Builds a revision that brings the document back to an earlier revision.
	/// @param number The revision to go back to.
	/// @param author The new revision's author.
	/// @return The revision, for submit() or commit().
	/// @throws DocumentError If @p number is unknown or the document already matches it.
	[[nodiscard]] virtual json restoreRevision(int number, std::string_view author) const = 0;

	/* Review */

	/// @brief The proposals waiting for a decision, oldest first.
	[[nodiscard]] virtual std::vector<ProposalInfo> proposals() const = 0;

	/// @brief Commits a proposal; it leaves the queue either way, and onDecision fires.
	/// @param proposal The proposal's id.
	/// @return The error, or std::nullopt if applied.
	virtual std::optional<std::string> accept(int proposal) = 0;

	/// @brief Rejects a proposal.
	/// @param proposal The proposal's id.
	/// @param reason Told to the author.
	/// @return false if there is no such proposal.
	virtual bool reject(int proposal, std::string_view reason) = 0;

	/// @brief Shows a proposal live, without committing it, until endPreview() or a decision.
	/// @param proposal The proposal's id.
	/// @return The error, or std::nullopt.
	virtual std::optional<std::string> preview(int proposal) = 0;

	/// @brief Goes back to the committed document after preview().
	virtual void endPreview() = 0;
	/// @brief The proposal being previewed, if any.
	[[nodiscard]] virtual std::optional<int> previewing() const = 0;

	/// @brief Tells whether an author may review, from the document's rules.
	/// @param author The author's full name.
	/// @return true for a reviewer.
	[[nodiscard]] virtual bool mayReview(std::string_view author) const = 0;

	/// @brief Turns auto-accept on or off: revisions that would wait for review are applied right away.
	///
	/// Rules, locks and protected parts still apply. Proposals already waiting stay until someone decides.
	/// @param enabled true to turn it on.
	virtual void setAutoAccept(bool enabled) = 0;
	/// @brief Whether auto-accept is on.
	[[nodiscard]] virtual bool autoAccept() const = 0;

	/* History */

	/// @brief Every committed revision, oldest first.
	[[nodiscard]] virtual std::vector<HistoryEntry> history() const = 0;

	/// @brief Exports a change set ("plugin"), portable to other copies of the same document.
	/// @param fromRevision Revisions after this one are included.
	/// @param name The change set's name.
	/// @return The change set.
	[[nodiscard]] virtual json exportChanges(int fromRevision, std::string_view name) const = 0;

	/// @brief Applies a change set on top of the current document, all or nothing.
	/// @param changes A change set from exportChanges().
	/// @return The error, or std::nullopt if every revision applied.
	virtual std::optional<std::string> importChanges(const json& changes) = 0;

	/* Notifications, e.g. for every connected peer (the VersionObserver is for the one that renders) */

	Signal<const Decision&> onDecision;   ///< A reviewer accepted or rejected a proposal.
	Signal<const HistoryEntry&> onCommit; ///< A revision was committed, by anyone.
	Signal<> onReviewChanged;             ///< Proposals, preview or auto-accept changed.
};

/// @brief Converts a proposal to its live-protocol JSON.
/// @param p The proposal.
/// @return `{"id", "author", "message", "operations", "time"}`.
inline json toJson(const ProposalInfo& p) {
	return {{"id", p.id}, {"author", p.author}, {"message", p.message}, {"operations", p.operations}, {"time", p.time}};
}

/// @brief Converts a history entry to its live-protocol JSON.
/// @param h The entry.
/// @return `{"number", "author", "message", "time"}`.
inline json toJson(const HistoryEntry& h) {
	return {{"number", h.number}, {"author", h.author}, {"message", h.message}, {"time", h.time}};
}

} // namespace sgl
