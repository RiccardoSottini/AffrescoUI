#pragma once

/// @file
/// @brief vcs::Repository: version control for any JSON document and its live state.

#include "sgl/core/DocumentRules.hpp"
#include "sgl/core/VersionControl.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgl::vcs {

/// @brief Keeps a JSON document and its live state under version control (see VERSION_CONTROL.md).
///
/// Knows nothing about what the document describes: the DocumentRules say what is valid, where "#id" nodes are, and
/// who may change what. "review" revisions are checked against the current document before they are queued, so what
/// could never apply is rejected while its author is still listening.
class Repository final : public VersionControl {
public:
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
	[[nodiscard]] HistoryEntry head() const override;

	/* Changes */
	RevisionResult submit(const json& revision) override;
	std::optional<std::string> commit(const json& revision) override;
	[[nodiscard]] json restoreRevision(int number, std::string_view author) const override;

	/* Review */
	[[nodiscard]] std::vector<ProposalInfo> proposals() const override;
	std::optional<std::string> accept(int proposal) override;
	bool reject(int proposal, std::string_view reason) override;
	std::optional<std::string> preview(int proposal) override;
	void endPreview() override;
	[[nodiscard]] std::optional<int> previewing() const override { return previewing_; }
	[[nodiscard]] bool mayReview(std::string_view author) const override;
	void setAutoAccept(bool enabled) override;
	[[nodiscard]] bool autoAccept() const override { return autoAccept_; }

	/* History */
	[[nodiscard]] std::vector<HistoryEntry> history() const override;
	[[nodiscard]] json exportChanges(int fromRevision, std::string_view name) const override;
	std::optional<std::string> importChanges(const json& changes) override;

private:
	/// @brief A committed revision.
	struct Revision {
		int number;                 ///< 0 is the document as loaded.
		std::string author;         ///< Who made it.
		std::string message;        ///< Why.
		json document;              ///< The document after it.
		json patch = json::array(); ///< As submitted (with #id paths): what a change set carries.
		std::string time;           ///< UTC, ISO 8601.
	};

	/// @brief A revision waiting for a reviewer (author policy "review").
	struct Proposal {
		int id;           ///< Unique for the repository's lifetime.
		json revision;    ///< As submitted.
		std::string time; ///< UTC, ISO 8601.
	};

	/// @brief A document and state ready to commit, already checked against the permissions.
	struct Prepared {
		json document; ///< The document after the revision.
		json state;    ///< The state after the revision.
	};

	/// @brief Resolves a "#id/rest" path to the node's JSON Pointer followed by "/rest".
	/// @param doc The document to search.
	/// @param path A JSON Pointer or an "#id" path.
	/// @param where Its place in the revision, for errors.
	/// @return A JSON Pointer.
	/// @throws DocumentError If no node has that id.
	[[nodiscard]] std::string resolvePath(const json& doc, const std::string& path, const std::string& where) const;
	/// @brief Checks the rules every revision follows, whoever wrote it: the format's checkChange() and the locks.
	/// @param newDocument The document after the revision.
	/// @param newState The state after the revision.
	/// @throws DocumentError If a rule is broken.
	void checkPermissions(const json& newDocument, const json& newState) const;
	/// @brief Computes what a revision leads to, changing nothing.
	///
	/// Checks "parent" against the head, resolves "#id" paths, sends "/state" operations to the state and the rest
	/// to the document, then checks the permissions and validates the result.
	/// @param revision The revision.
	/// @return The new document and state.
	/// @throws DocumentError If the revision can't apply.
	[[nodiscard]] Prepared prepare(const json& revision) const;
	/// @brief Tells the observer that the working copy changed.
	/// @throws DocumentError If the observer refuses it.
	void documentChanged();
	/// @brief Tells the observer and onReviewChanged that the proposals changed.
	void proposalsChanged();

	const DocumentRules& rules_;          ///< What is valid and who may change what.
	VersionObserver* observer_ = nullptr; ///< The renderer.

	json document_;                 ///< The head, or the proposal being previewed.
	json state_ = json::object();   ///< The live state.
	std::vector<Revision> history_; ///< history_.front() is the document as loaded.

	std::vector<Proposal> proposals_;         ///< Oldest first.
	int nextProposal_ = 1;                    ///< Next proposal id.
	std::optional<int> previewing_;           ///< Proposal shown in place of the committed document.
	json committedDocument_, committedState_; ///< Kept aside during a preview.
	bool autoAccept_ = false;                 ///< See setAutoAccept().
	bool importing_ = false;                  ///< Commits are announced once the whole change set is in.
};

} // namespace sgl::vcs
