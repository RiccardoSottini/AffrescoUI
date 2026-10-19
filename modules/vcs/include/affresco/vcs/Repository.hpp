#pragma once

/// @file
/// @brief vcs::Repository: Git-like version control for any JSON document and its live state.

#include "affresco/core/DocumentRules.hpp"
#include "affresco/core/VersionControl.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace affresco::vcs {

namespace detail {
class HistoryStore;
}

/// @brief Keeps a JSON document and its live state under version control (see VERSION_CONTROL.md).
///
/// Commits are stored by their SHA-256, which covers the document, the parents, the author and committer, the time
/// and the patch, so a commit id stands for exactly one history. "main" only grows. Proposals are branches on top of
/// main, rebuilt on main's head whenever their author pushes to them, and merged with a merge commit (like
/// `git merge --no-ff`), so the history records who approved what.
///
/// Knows nothing about what the document describes: the DocumentRules say what is valid, where "#id" nodes are, and
/// the permission rules. A rule is judged by the change's effect (the document before and after), never by the
/// paths its patch happens to use.
///
/// The history lives in memory unless openHistory() gives it a folder: then every change is written there, and the
/// next openHistory() of that folder brings it back (VERSION_CONTROL.md, "History on disk").
class Repository final : public VersionControl {
public:
	/// Most proposals one channel may have open at once, so a peer can't flood the reviewer.
	static constexpr std::size_t maxOpenProposals = 16;
	/// How many commits apart, along first parents, commits keep their whole document: a commit is rebuilt from at
	/// most this many deltas. Chosen by measurement (VERSION_CONTROL.md, "History in proportion to the changes").
	static constexpr int snapshotInterval = 64;

	/// @brief Puts a document under version control as revision 0.
	/// @param rules The document's rules; must outlive the repository.
	/// @param document The document; its "state" becomes the live state.
	/// @throws DocumentError If the document is not valid.
	Repository(const DocumentRules& rules, json document);
	/// @brief Closes the history folder, if one is open, and releases its lock.
	~Repository() override;

	/// @brief What openHistory() found.
	struct OpenedHistory {
		bool loaded = false;            ///< The history was read from the folder; otherwise a new one started there.
		std::string memoryOnly;         ///< Why changes aren't written, e.g. another program holds the lock; empty if
		                                ///< they are.
		std::filesystem::path setAside; ///< Where the old history went, if it began with another version of the
		                                ///< document (the screen file changed): a new one starts.
	};

	/// @brief Keeps the history in a folder from now on: loads the history written there, or starts one. Call it
	/// before the first change.
	///
	/// A history that began with another version of the document is moved aside and a new one starts. If another
	/// program holds the folder's lock, the history is read but never written (OpenedHistory::memoryOnly says so).
	/// @param directory The folder, e.g. "~/.affrescoui/kanban.history". Created if needed.
	/// @return What it found.
	/// @throws DocumentError If the history there is damaged (what() names the file; nothing is loaded), or its last
	/// revision can't be shown, or if this repository already changed or already has a folder.
	OpenedHistory openHistory(const std::filesystem::path& directory);

	void setObserver(VersionObserver* observer) override { observer_ = observer; }

	/* Working copy */
	[[nodiscard]] const json& document() const override { return document_; }
	[[nodiscard]] json& state() override { return state_; }
	[[nodiscard]] const json& state() const override { return state_; }
	[[nodiscard]] CommitInfo head() const override { return info(mainHead()); }

	/* Changes */
	RevisionResult submit(const json& revision) override;
	std::optional<std::string> commit(const json& revision) override;
	[[nodiscard]] CommitInfo resolve(const json& revision) const override { return info(find(revision)); }
	[[nodiscard]] json restoreRevision(const json& revision, std::string_view author) const override;
	[[nodiscard]] json revertRevision(const json& revision, std::string_view author) const override;
	[[nodiscard]] json diff(const json& from, const json& to) const override;
	[[nodiscard]] CommitInfo blame(std::string_view target) const override;

	/* Review */
	[[nodiscard]] std::vector<ProposalInfo> proposals() const override;
	[[nodiscard]] json proposalDiff(int proposal) const override;
	std::optional<std::string> accept(int proposal, const Approval& approval) override;
	bool reject(int proposal, std::string_view reason) override;
	std::optional<std::string> preview(int proposal) override;
	void endPreview() override;
	[[nodiscard]] std::optional<int> previewing() const override { return previewing_; }
	[[nodiscard]] bool mayReview(std::string_view author) const override;
	void setMode(Mode mode) override;
	/// @brief Sets the mode until the program quits, without keeping it in the history: what a host's command line
	/// asks for. The history keeps the mode a reviewer last chose (setMode()), which the next start uses.
	/// @param mode The mode.
	void setModeForThisRun(Mode mode);
	[[nodiscard]] Mode mode() const override { return mode_; }
	[[nodiscard]] std::vector<Grant> grants() const override { return grants_; }
	bool revokeGrant(std::string_view rule) override;

	/* History */
	[[nodiscard]] std::vector<CommitInfo> log() const override;
	[[nodiscard]] json exportChanges(const json& from, std::string_view name) const override;
	std::optional<std::string> importChanges(const json& changes) override;
	/// @brief Tells whether a change set was already applied to this history: importChanges() keeps the id of each
	/// one it applies (the SHA-256 of its JSON), on disk too, so a host can apply a plugin once.
	/// @param changes The change set.
	/// @return true if it was.
	[[nodiscard]] bool hasImported(const json& changes) const;

private:
	/// @brief A commit object. It keeps how its document differs from its first parent's, not the document: every
	/// snapshotInterval-th commit along first parents (and revision 0) also keeps the whole document, and any other is
	/// rebuilt from the nearest one (documentOf()), so the history grows with the changes, not with the document.
	struct Commit {
		std::string id;                               ///< SHA-256 of what it is (idOf()); not of the delta.
		std::vector<std::string> parents;             ///< main's commit first.
		std::string author, committer, message, time; ///< Who wrote it, who applied it, why, when.
		int number = 0;                               ///< Position on main, or after the base on a proposal branch.
		int proposal = 0;                             ///< The proposal a merge commit merged.
		std::string tree;                             ///< The SHA-256 of its document's JSON, which the id covers.
		json patch = json::array(); ///< As submitted (with "#id" paths): what rebases and change sets replay.
		json delta;                 ///< Its document from its first parent's (detail::delta()); null without a parent,
		                            ///< or when loaded from a history written before deltas (then it has a snapshot).
		std::shared_ptr<const json> snapshot; ///< Its whole document, if it keeps one (shared by equal documents).
		int sinceSnapshot = 0;                ///< Commits back to the nearest snapshot along first parents.
		/// What undoes its change to the live state: JSON Patch on "/state". Null if it changed none, or if it was
		/// read from disk (the live state starts again with every run, so only the document is reverted then).
		json stateUndo;
		/// The parts of the live state it changed, as it left them: `[{"path", "value"?}]`, no value for a part it
		/// removed. A revert needs them as they are, as it needs the parts of the document.
		json stateAfter;
	};

	/// @brief An open proposal: a branch from `base` to `head`.
	struct Proposal {
		int id = 0;                                 ///< Unique for the repository's lifetime.
		std::string author, message, time;          ///< Who opened it, its title, when.
		std::string base, head;                     ///< main's commit it is built on, and its newest commit.
		std::size_t commits = 0;                    ///< Commits on the branch.
		json patch = json::array();                 ///< Every commit's operations, in order: what accept replays.
		std::vector<std::string> touches, remember; ///< See ProposalInfo.
		std::string reason;                         ///< Why it waits.
	};

	/// @brief A revision applied to a copy, not yet committed.
	struct Prepared {
		json before, after;               ///< As rules see them (detail::view): the live state at "/state".
		json document, state;             ///< The new document and live state.
		json patch = json::array();       ///< Its operations as plain JSON Patch ("set" became replace or add).
		std::vector<std::string> touches; ///< What its operations touch (detail::area), each once.
	};

	/// @brief What the permission rules say about a change.
	struct Verdict {
		enum class Kind { Allow, Ask, Deny } kind; ///< Apply it, propose it or refuse it.
		std::string reason;                        ///< For Ask and Deny.
		bool askedByRule = false;                  ///< An "ask" rule matched: "don't ask again" can't cover it.
	};

	/// @brief main's newest commit.
	[[nodiscard]] const Commit& mainHead() const { return objects_.at(main_.back()); }
	/// @brief main's head's document (the preview has its own).
	[[nodiscard]] const json& mainDocument() const { return previewing_ ? committedDocument_ : document_; }
	/// @brief The live state as committed (the preview has its own).
	[[nodiscard]] const json& committedState() const { return previewing_ ? committedState_ : state_; }
	/// @brief A commit's document: main's head's as it is, any other rebuilt from the nearest snapshot.
	/// @param commit The commit.
	/// @return The document.
	/// @throws HistoryError If a delta read from disk doesn't rebuild the document its id names.
	[[nodiscard]] json documentOf(const Commit& commit) const;
	/// @brief Rebuilds a commit's document from the nearest snapshot along first parents, and checks it against the
	/// commit's tree.
	/// @param objects The commits, by id.
	/// @param commit The commit.
	/// @return The document.
	/// @throws HistoryError If the deltas don't rebuild the document the commit names (a damaged history).
	[[nodiscard]] static json rebuild(const std::unordered_map<std::string, Commit>& objects, const Commit& commit);
	/// @brief How a commit's document differs from its first parent's.
	/// @param commit A commit with a parent.
	/// @return Its delta (worked out from the documents for a commit loaded from a history without deltas).
	[[nodiscard]] json deltaOf(const Commit& commit) const;
	/// @brief Records how a commit changed the live state, so that a revert can undo it (Commit::stateUndo), when its
	/// patch reaches the live state.
	/// @param commit The commit, about to be stored.
	/// @param before The live state before it.
	/// @param after The live state after it.
	static void recordStateChange(Commit& commit, const json& before, const json& after);
	/// @brief What undoes a commit's change to the live state, if the parts it changed are still as it left them.
	/// @param target The commit.
	/// @return JSON Patch on "/state" (empty if it changed none, or none this run).
	/// @throws DocumentError If a part changed since (a conflict).
	[[nodiscard]] json stateRevert(const Commit& target) const;
	/// @brief Checks that what a revert undoes in the document is still as the commit left it.
	/// @param target The commit.
	/// @param then Its document.
	/// @param patch The revert's document operations.
	/// @throws DocumentError If a part changed since (a conflict).
	void checkDocumentRevert(const Commit& target, const json& then, const json& patch) const;
	/// @brief Describes a commit.
	/// @param commit The commit.
	/// @return Its public description.
	[[nodiscard]] static CommitInfo info(const Commit& commit);
	/// @brief Names a commit for messages.
	/// @param commit The commit.
	/// @return e.g. "revision 3 (a3f9c21)".
	[[nodiscard]] static std::string describe(const Commit& commit);

	/// @brief Finds a commit by revision number, id prefix, "main", "HEAD", "proposal/<id>", with "~n".
	/// @param revision The name.
	/// @return The commit.
	/// @throws DocumentError If there is none or the prefix is ambiguous.
	[[nodiscard]] const Commit& find(const json& revision) const;
	/// @brief Finds an open proposal.
	/// @param id Its id.
	/// @return It, or nullptr.
	[[nodiscard]] Proposal* findProposal(int id);
	/// @brief Finds an open proposal.
	/// @param id Its id.
	/// @return It, or nullptr.
	[[nodiscard]] const Proposal* findProposal(int id) const;
	/// @brief Hashes a commit and stores it with its delta, and a snapshot if one is due (storing the same content
	/// twice gives the same commit).
	/// @param commit The commit, without its id, tree, delta or snapshot.
	/// @param document Its document.
	/// @param parentDocument Its first parent's document, or nullptr for revision 0.
	/// @return The stored commit.
	const Commit& store(Commit commit, const json& document, const json* parentDocument);
	/// @brief Puts a commit on main and makes it the working copy. Ends any preview.
	/// @param commit The commit; main's head becomes its first parent.
	/// @param document The document after it.
	/// @param state The live state after it.
	/// @return The stored commit.
	/// @throws DocumentError If the observer refuses it; then nothing changed.
	const Commit& land(Commit commit, json document, json state);

	/// @brief Resolves a "#id/rest" path to the node's JSON Pointer followed by "/rest".
	/// @param view The document as it is when the operation runs.
	/// @param path A JSON Pointer or an "#id" path.
	/// @param where Its place in the revision, for errors.
	/// @return A JSON Pointer.
	/// @throws DocumentError If no node has that id.
	[[nodiscard]] std::string resolvePath(const json& view, const std::string& path, const std::string& where) const;
	/// @brief The format's hint for a path that finds nothing, for an error.
	/// @param view The document the path was read in.
	/// @param path The path as written.
	/// @return " (<hint>)", or "" if there is none.
	[[nodiscard]] std::string hintFor(const json& view, const std::string& path) const;
	/// @brief Makes an operation, its paths resolved, ready to apply and to store: "set" becomes "replace" or "add",
	/// and the nodes it adds get ids (DocumentRules::nameNodes).
	/// @param op The operation, changed in place.
	/// @param written The operation as the revision wrote it.
	/// @param view The document as it is when the operation runs.
	/// @return The operation to store in the commit: as written, with its kind and its value as applied.
	[[nodiscard]] json normalise(json& op, const json& written, const json& view) const;
	/// @brief Applies one operation to a view.
	/// @param view The document, changed in place.
	/// @param op The operation, its paths resolved.
	/// @param written The operation as the revision wrote it, for the error.
	/// @param where Its place in the revision.
	/// @throws DocumentError If it doesn't apply: names the operation, the path and a hint.
	void apply(json& view, json op, const json& written, const std::string& where) const;
	/// @brief Names the first of an operation's paths that finds nothing, with the format's hint, for an error.
	/// @param view The document the operation failed on.
	/// @param paths The operation's "from" and "path", each as [written, resolved] (nulls where it has none).
	/// @return " at <path> (<hint>)", or "" if each path finds something.
	[[nodiscard]] std::string pathsHint(const json& view, const json& paths) const;
	/// @brief Checks a revision's "parent" against main's head.
	/// @param revision The revision.
	/// @throws DocumentError If the screen has moved on (a conflict).
	void checkParent(const json& revision) const;
	/// @brief Applies a patch to a copy, one operation at a time, and checks the format's rules. Changes nothing.
	/// @param patch The JSON Patch.
	/// @param document The document to start from.
	/// @param state The live state to start from.
	/// @return The views and the new document and state.
	/// @throws DocumentError If it can't apply or the result isn't valid.
	[[nodiscard]] Prepared prepare(const json& patch, const json& document, const json& state) const;
	/// @brief Judges a change by the permission rules: deny, then ask, then allow and grants, then the mode.
	/// @param before The view before.
	/// @param after The view after.
	/// @param author Who makes it.
	/// @param reviewed false for changes that are never reviewed (the app's own, merges): only deny rules apply.
	/// @return The verdict.
	[[nodiscard]] Verdict evaluate(const json& before, const json& after, const std::string& author,
	                               bool reviewed) const;
	/// @brief Builds a proposal's branch again on main's head, with one more commit, and describes it.
	/// @param proposal The proposal.
	/// @param revision The new commit's revision.
	/// @throws DocumentError If its commits no longer apply, or a deny rule refuses them.
	void extend(Proposal& proposal, const json& revision);
	/// @brief The "don't ask again" rules a proposal offers: one per part it touches, for its author's channel, unless
	/// an "ask" rule matched or a part is broader than one node or area.
	/// @param author The proposal's author.
	/// @param touches What it touches (detail::area).
	/// @param askedByRule Whether an "ask" rule matched it.
	/// @return The rules, e.g. "agent(#board)"; empty if it can't be remembered.
	[[nodiscard]] static std::vector<std::string> rememberedRules(const std::string& author,
	                                                              const std::vector<std::string>& touches,
	                                                              bool askedByRule);
	/// @brief Pushes a revision onto its author's open proposal ("proposal" in the revision).
	/// @param revision The revision.
	/// @param author Its author.
	/// @return What happened.
	/// @throws DocumentError If there is no such proposal, it isn't the author's, or the result is refused.
	RevisionResult push(const json& revision, const std::string& author);
	/// @brief Tells the observer that the working copy changed.
	/// @throws DocumentError If the observer refuses it.
	void documentChanged();
	/// @brief Tells the observer and onReviewChanged that the proposals changed.
	void proposalsChanged();

	/* History on disk (RepositoryHistory.cpp) */

	/// @brief What the refs file holds: main's head, the open proposals, the grants, the mode and the change sets applied.
	struct Refs;
	/// @brief A commit's id: the SHA-256 of what it is (store() and the history on disk use it): its tree, parents,
	/// author, committer, time, patch, proposal and message. Not its delta or snapshot, which say how it is kept.
	/// @param commit The commit; its id is ignored.
	/// @return 64 hex digits.
	[[nodiscard]] static std::string idOf(const Commit& commit);
	/// @brief A commit as it is written to disk.
	/// @param commit The commit.
	/// @return Everything its id covers, with the document named by its id (its tree), then its delta, and whether
	/// its document is written as an object of its own (a snapshot).
	[[nodiscard]] static json objectOf(const Commit& commit);
	/// @brief Reads a document object and checks it against its id.
	/// @param store The history.
	/// @param id The document's id: the SHA-256 of its JSON.
	/// @param namedIn The file that names it, for errors.
	/// @param documents The documents read so far, by id: commits that share a document read it once.
	/// @return The document, shared by the commits that name it.
	/// @throws DocumentError If it is missing, malformed, or its content doesn't match its id.
	[[nodiscard]] static std::shared_ptr<const json>
	readDocument(detail::HistoryStore& store, const std::string& id, const std::filesystem::path& namedIn,
	             std::unordered_map<std::string, std::shared_ptr<const json>>& documents);
	/// @brief Reads a commit object, and its document if it is a snapshot, and checks them against their ids. A
	/// commit's delta isn't covered by its id: rebuild() checks the documents deltas give against their trees.
	/// @param store The history.
	/// @param id The commit's id.
	/// @param namedIn The file that names it, for errors.
	/// @param documents The documents read so far, by id (see readDocument()).
	/// @return The commit, not yet numbered.
	/// @throws DocumentError If it is missing, malformed, or its content doesn't match its id.
	[[nodiscard]] static Commit readObject(detail::HistoryStore& store, const std::string& id,
	                                       const std::filesystem::path& namedIn,
	                                       std::unordered_map<std::string, std::shared_ptr<const json>>& documents);
	/// @brief Reads every commit the heads lead to, numbers them, and counts each one's distance to a snapshot.
	/// @param store The history.
	/// @param heads main's head, then the proposals' heads.
	/// @return The commits, by id.
	/// @throws DocumentError If one is missing or damaged.
	[[nodiscard]] static std::unordered_map<std::string, Commit> readCommits(detail::HistoryStore& store,
	                                                                        std::vector<std::string> heads);
	/// @brief Counts each commit's distance to the nearest snapshot along first parents (Commit::sinceSnapshot).
	/// @param objects The commits read, by id; sinceSnapshot is -1 for those not counted yet.
	static void countFromSnapshots(std::unordered_map<std::string, Commit>& objects);
	/// @brief Reads the refs file and checks its checksum.
	/// @param store The history.
	/// @return What it holds; the proposals have no commits or patch yet (completeProposal()).
	/// @throws DocumentError If it is damaged.
	[[nodiscard]] static Refs readRefs(detail::HistoryStore& store);
	/// @brief Reads one proposal from the refs file.
	/// @param entry Its entry.
	/// @param where Its place in the file, for errors.
	/// @return The proposal, without its commits and patch.
	/// @throws DocumentError If it is malformed.
	[[nodiscard]] static Proposal readProposal(const json& entry, const std::string& where);
	/// @brief Finds a proposal's commits on its branch, and replays their patches into it.
	/// @param proposal The proposal.
	/// @param objects The commits.
	/// @param main main's commits, oldest first.
	/// @param file The refs file, for errors.
	/// @throws DocumentError If its branch doesn't lead from its head to its base on main.
	static void completeProposal(Proposal& proposal, const std::unordered_map<std::string, Commit>& objects,
	                             const std::vector<std::string>& main, const std::filesystem::path& file);
	/// @brief Works out again what a proposal read from the refs file touches, and what "don't ask again" would grant
	/// for it, from the patch its branch replays: accept() grants what it remembers, and the refs file may have been
	/// written by hand. The live state it was made on isn't kept, so the base's "state" stands in for it; if the patch
	/// doesn't apply on that, nothing is remembered (it can still be accepted).
	/// @param proposal The proposal, completed (completeProposal()).
	/// @param base Its base commit's document.
	void recheckProposal(Proposal& proposal, const json& base) const;
	/// @brief Replaces the history with the one in a folder, if it began with the same document.
	/// @param store The folder.
	/// @return false, changing nothing, if it began with another document.
	/// @throws DocumentError If it is damaged, or its head can't be shown; then nothing changed.
	bool loadHistory(detail::HistoryStore& store);
	/// @brief The refs file's content: main's head, the open proposals, the grants, the mode and the change sets applied.
	/// @return It, without its checksum.
	[[nodiscard]] json refsJson() const;
	/// @brief Writes a commit, and its document if it is a snapshot the folder doesn't have yet.
	/// @param commit The commit; its parents are on disk already.
	/// @throws DocumentError If either can't be written, or is more than a history reads back.
	void writeCommit(const Commit& commit);
	/// @brief Writes the commits the folder doesn't have yet, then the refs. A failure is logged, and the next change
	/// tries again: the change itself stays.
	void saveHistory() noexcept;

	const DocumentRules& rules_;          ///< What is valid and who may change what.
	VersionObserver* observer_ = nullptr; ///< The renderer.

	std::unordered_map<std::string, Commit> objects_; ///< Every commit, by id.
	std::vector<std::string> main_;                   ///< main's commits, oldest first: main_[n] is revision n.

	json document_;               ///< main's head, or the proposal being previewed.
	json state_ = json::object(); ///< The live state.

	std::vector<Proposal> proposals_; ///< Open proposals, oldest first.
	int nextProposal_ = 1;            ///< Next proposal id.
	std::optional<int> previewing_;   ///< Proposal shown in place of main's head.
	json committedState_;             ///< The live state, kept aside during a preview.
	json committedDocument_;          ///< main's head's document, kept aside during a preview.
	Mode mode_ = Mode::Default;       ///< The mode in force: see setMode() and setModeForThisRun().
	Mode keptMode_ = Mode::Default;   ///< The mode the history keeps: the one a reviewer last chose (setMode()).
	std::vector<Grant> grants_;       ///< "Don't ask again" rules.
	bool importing_ = false;          ///< Commits are announced once the whole change set is in.
	std::vector<std::string> imported_; ///< The change sets applied, by id (hasImported()), oldest first.
	std::unique_ptr<detail::HistoryStore> history_; ///< The folder the history is kept in, if any (openHistory()).
};

} // namespace affresco::vcs
