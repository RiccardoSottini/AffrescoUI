#pragma once

#include "sgl/core/DocumentRules.hpp"
#include "sgl/core/VersionControl.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgl::vcs {

// Keeps a JSON document and its live state under version control. Knows nothing about what the document describes:
// `rules` (which must outlive the repository) say what is valid, where "#id" nodes are, and who may change what.
// See GOVERNANCE.md.
class Repository final : public VersionControl {
public:
	// `document` becomes revision 0, its "state" the live state. Throws DocumentError if it isn't valid.
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
	struct Revision {
		int number;
		std::string author;
		std::string message;
		json document;
		json patch = json::array(); // as submitted (with #id paths): what a change set carries
		std::string time;           // UTC, ISO 8601
	};

	// A revision waiting for the user's decision (author policy "review")
	struct Proposal {
		int id;
		json revision;
		std::string time;
	};

	// A document and state ready to commit, already checked against the permissions
	struct Prepared {
		json document;
		json state;
	};

	[[nodiscard]] std::string resolvePath(const json& doc, const std::string& path, const std::string& where) const;
	void checkPermissions(const json& newDocument, const json& newState) const;
	[[nodiscard]] Prepared prepare(const json& revision) const;
	void documentChanged();
	void proposalsChanged();

	const DocumentRules& rules_;
	VersionObserver* observer_ = nullptr;

	json document_;
	json state_ = json::object();
	std::vector<Revision> history_; // history_.front() is the document as loaded

	std::vector<Proposal> proposals_;
	int nextProposal_ = 1;
	std::optional<int> previewing_; // proposal shown in place of the committed document
	json committedDocument_, committedState_;
	bool autoAccept_ = false;
	bool importing_ = false; // commits are announced once the whole change set is in
};

} // namespace sgl::vcs
