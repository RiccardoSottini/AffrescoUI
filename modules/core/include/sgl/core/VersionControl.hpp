#pragma once

#include "sgl/core/Json.hpp"
#include "sgl/core/Signal.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgl {

enum class RevisionStatus {
	Applied,  // committed: the document changed
	Proposed, // waiting for the user's decision (the author's policy is "review")
	Rejected, // nothing changed; `error` says why
};

struct RevisionResult {
	RevisionStatus status;
	int revision = 0; // the current revision after the call
	int proposal = 0; // when proposed: its id
	std::string error;
};

struct ProposalInfo {
	int id;
	std::string author, message;
	std::size_t operations;
	std::string time;
};

struct HistoryEntry {
	int number;
	std::string author, message, time;
};

// The user's decision on a proposal
struct Decision {
	int proposal;
	std::optional<std::string> error; // nothing: accepted and applied
	int revision;                     // the current revision after the decision
};

// Told when the working copy changes
class VersionObserver {
public:
	virtual ~VersionObserver() = default;

	// The document or the state was replaced (commit, preview, restore, import). Throw DocumentError to refuse the
	// new version: it is rolled back, and this is called again for the version that is back in place.
	virtual void documentChanged() = 0;

	// Only the proposals changed
	virtual void proposalsChanged() = 0;
};

// Version control for a live document: a history of revisions (JSON Patch), permissions per author, proposals
// the user reviews, preview, restore and portable change sets. Implemented by vcs::Repository; used by the screen
// runtime (state, control steps) and by channel sessions (revisions from outside).
//
// Revisions are JSON: {"author", "message", "parent"?, "patch": [JSON Patch ops]}. Patch paths may start with
// "#id" (a node) or "/state" (the live state rather than the document).
class VersionControl {
public:
	virtual ~VersionControl() = default;

	virtual void setObserver(VersionObserver* observer) = 0; // one at a time; nullptr to stop

	/* Working copy */
	[[nodiscard]] virtual const json& document() const = 0; // the head, or the proposal being previewed
	[[nodiscard]] virtual json& state() = 0;                // live: changed freely by actions, patched by revisions
	[[nodiscard]] virtual const json& state() const = 0;
	[[nodiscard]] virtual HistoryEntry head() const = 0; // the current revision; 0 = the document as loaded

	/* Changes */
	// From someone who must follow the document's permissions: applied, proposed for review, or rejected
	virtual RevisionResult submit(const json& revision) = 0;
	// From the application itself: not reviewed, but locks and the other rules still apply. Returns the error, if any.
	virtual std::optional<std::string> commit(const json& revision) = 0;
	// A revision that brings the document back to how it was at `number` (throws DocumentError)
	[[nodiscard]] virtual json restoreRevision(int number, std::string_view author) const = 0;

	/* Review */
	[[nodiscard]] virtual std::vector<ProposalInfo> proposals() const = 0;
	virtual std::optional<std::string> accept(int proposal) = 0;
	virtual bool reject(int proposal, std::string_view reason) = 0;
	virtual std::optional<std::string> preview(int proposal) = 0; // shows it live, without committing it
	virtual void endPreview() = 0;
	[[nodiscard]] virtual std::optional<int> previewing() const = 0;

	/* History */
	[[nodiscard]] virtual std::vector<HistoryEntry> history() const = 0;
	// A change set ("plugin"): the revisions after `fromRevision`, portable to other copies of the same document
	[[nodiscard]] virtual json exportChanges(int fromRevision, std::string_view name) const = 0;
	virtual std::optional<std::string> importChanges(const json& changes) = 0; // all or nothing

	Signal<const Decision&> onDecision;
};

inline json toJson(const ProposalInfo& p) {
	return {{"id", p.id}, {"author", p.author}, {"message", p.message}, {"operations", p.operations}, {"time", p.time}};
}

inline json toJson(const HistoryEntry& h) {
	return {{"number", h.number}, {"author", h.author}, {"message", h.message}, {"time", h.time}};
}

} // namespace sgl
