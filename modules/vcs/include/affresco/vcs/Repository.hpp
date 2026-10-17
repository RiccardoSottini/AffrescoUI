#pragma once

/// @file
/// @brief vcs::Repository: Git-like version control for any JSON document and its live state.

#include "affresco/core/DocumentRules.hpp"
#include "affresco/core/VersionControl.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace affresco::vcs {

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
class Repository final : public VersionControl {
public:
	/// Most proposals one channel may have open at once, so a peer can't flood the reviewer.
	static constexpr std::size_t maxOpenProposals = 16;

	/// @brief Puts a document under version control as revision 0.
	/// @param rules The document's rules; must outlive the repository.
	/// @param document The document; its "state" becomes the live state.
	/// @throws DocumentError If the document is not valid.
	Repository(const DocumentRules& rules, json document);

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
	[[nodiscard]] Mode mode() const override { return mode_; }
	[[nodiscard]] std::vector<Grant> grants() const override { return grants_; }
	bool revokeGrant(std::string_view rule) override;

	/* History */
	[[nodiscard]] std::vector<CommitInfo> log() const override;
	[[nodiscard]] json exportChanges(const json& from, std::string_view name) const override;
	std::optional<std::string> importChanges(const json& changes) override;

private:
	/// @brief A commit object.
	struct Commit {
		std::string id;                               ///< SHA-256 of everything below except the number.
		std::vector<std::string> parents;             ///< main's commit first.
		std::string author, committer, message, time; ///< Who wrote it, who applied it, why, when.
		int number = 0;                               ///< Position on main, or after the base on a proposal branch.
		int proposal = 0;                             ///< The proposal a merge commit merged.
		json document;                                ///< The whole document (the "tree").
		json patch = json::array(); ///< As submitted (with "#id" paths): what rebases and change sets replay.
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
	/// @brief The live state as committed (the preview has its own).
	[[nodiscard]] const json& committedState() const { return previewing_ ? committedState_ : state_; }
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
	/// @brief Hashes a commit and stores it (storing the same content twice gives the same commit).
	/// @param commit The commit, without its id.
	/// @return The stored commit.
	const Commit& store(Commit commit);
	/// @brief Puts a commit on main and makes it the working copy. Ends any preview.
	/// @param commit The commit; main's head becomes its first parent.
	/// @param state The live state after it.
	/// @return The stored commit.
	/// @throws DocumentError If the observer refuses it; then nothing changed.
	const Commit& land(Commit commit, json state);

	/// @brief Resolves a "#id/rest" path to the node's JSON Pointer followed by "/rest".
	/// @param view The document as it is when the operation runs.
	/// @param path A JSON Pointer or an "#id" path.
	/// @param where Its place in the revision, for errors.
	/// @return A JSON Pointer.
	/// @throws DocumentError If no node has that id.
	[[nodiscard]] std::string resolvePath(const json& view, const std::string& path, const std::string& where) const;
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
	Mode mode_ = Mode::Default;       ///< See setMode().
	std::vector<Grant> grants_;       ///< "Don't ask again" rules.
	bool importing_ = false;          ///< Commits are announced once the whole change set is in.
};

} // namespace affresco::vcs
