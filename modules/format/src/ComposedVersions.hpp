#pragma once

/// @file
/// @brief format::ComposedVersions: a document's version control as the runtime sees it, with the enabled features
/// merged in (Composition.hpp).

#include "affresco/core/VersionControl.hpp"

#include <set>
#include <string>

namespace affresco::format {

/// @brief Version control whose document is the composed screen: everything else is the inner one's.
///
/// History, diffs and reviews stay on the document as written, each feature whole in its record; the runtime builds
/// what document() returns. When the document changes, the live state gains the state of each feature just enabled
/// and loses that of each one disabled or removed, then the observer is told.
class ComposedVersions final : public VersionControl, private VersionObserver {
public:
	/// @brief Wraps a version control.
	/// @param inner The version control, which must outlive this.
	explicit ComposedVersions(VersionControl& inner);

	/// @brief Stops observing the inner version control.
	~ComposedVersions() override;

	ComposedVersions(const ComposedVersions&) = delete;            ///< Not copyable.
	ComposedVersions& operator=(const ComposedVersions&) = delete; ///< Not copyable.

	/// @copydoc VersionControl::setObserver
	void setObserver(VersionObserver* observer) override { observer_ = observer; }
	/// @brief The composed screen, or the inner document when it has nothing to compose.
	[[nodiscard]] const json& document() const override;
	/// @copydoc VersionControl::state
	[[nodiscard]] json& state() override { return inner_.state(); }
	/// @copydoc VersionControl::state
	[[nodiscard]] const json& state() const override { return inner_.state(); }
	/// @copydoc VersionControl::head
	[[nodiscard]] CommitInfo head() const override { return inner_.head(); }
	/// @copydoc VersionControl::submit
	RevisionResult submit(const json& revision) override { return inner_.submit(revision); }
	/// @copydoc VersionControl::commit
	std::optional<std::string> commit(const json& revision) override { return inner_.commit(revision); }
	/// @copydoc VersionControl::resolve
	[[nodiscard]] CommitInfo resolve(const json& revision) const override { return inner_.resolve(revision); }
	/// @copydoc VersionControl::restoreRevision
	[[nodiscard]] json restoreRevision(const json& revision, std::string_view author) const override {
		return inner_.restoreRevision(revision, author);
	}
	/// @copydoc VersionControl::revertRevision
	[[nodiscard]] json revertRevision(const json& revision, std::string_view author) const override {
		return inner_.revertRevision(revision, author);
	}
	/// @copydoc VersionControl::diff
	[[nodiscard]] json diff(const json& from, const json& to) const override { return inner_.diff(from, to); }
	/// @copydoc VersionControl::blame
	[[nodiscard]] CommitInfo blame(std::string_view target) const override { return inner_.blame(target); }
	/// @copydoc VersionControl::proposals
	[[nodiscard]] std::vector<ProposalInfo> proposals() const override { return inner_.proposals(); }
	/// @copydoc VersionControl::proposalDiff
	[[nodiscard]] json proposalDiff(int proposal) const override { return inner_.proposalDiff(proposal); }
	/// @copydoc VersionControl::accept
	std::optional<std::string> accept(int proposal, const Approval& approval) override {
		return inner_.accept(proposal, approval);
	}
	/// @copydoc VersionControl::reject
	bool reject(int proposal, std::string_view reason) override { return inner_.reject(proposal, reason); }
	/// @copydoc VersionControl::preview
	std::optional<std::string> preview(int proposal) override { return inner_.preview(proposal); }
	/// @copydoc VersionControl::endPreview
	void endPreview() override { inner_.endPreview(); }
	/// @copydoc VersionControl::previewing
	[[nodiscard]] std::optional<int> previewing() const override { return inner_.previewing(); }
	/// @copydoc VersionControl::mayReview
	[[nodiscard]] bool mayReview(std::string_view author) const override { return inner_.mayReview(author); }
	/// @copydoc VersionControl::setMode
	void setMode(Mode mode) override { inner_.setMode(mode); }
	/// @copydoc VersionControl::mode
	[[nodiscard]] Mode mode() const override { return inner_.mode(); }
	/// @copydoc VersionControl::grants
	[[nodiscard]] std::vector<Grant> grants() const override { return inner_.grants(); }
	/// @copydoc VersionControl::revokeGrant
	bool revokeGrant(std::string_view rule) override { return inner_.revokeGrant(rule); }
	/// @copydoc VersionControl::log
	[[nodiscard]] std::vector<CommitInfo> log() const override { return inner_.log(); }
	/// @copydoc VersionControl::exportChanges
	[[nodiscard]] json exportChanges(const json& from, std::string_view name) const override {
		return inner_.exportChanges(from, name);
	}
	/// @copydoc VersionControl::importChanges
	std::optional<std::string> importChanges(const json& changes) override { return inner_.importChanges(changes); }

private:
	/// @brief Recomposes the screen and brings the live state in line with the features, then tells the observer.
	void documentChanged() override;
	/// @brief Tells the observer.
	void proposalsChanged() override;
	/// @brief Recomposes the screen from the inner document.
	void recompose();
	/// @brief Gives the live state each enabled feature's state it lacks, and drops that of features gone.
	void syncState();

	VersionControl& inner_;               ///< The version control wrapped.
	VersionObserver* observer_ = nullptr; ///< Who is told about changes.
	json composed_;                       ///< The composed screen, if composing is needed.
	bool composing_ = false;              ///< Whether document() is composed_.
	std::set<std::string> featureStates_; ///< The state keys features own now.
	std::vector<Connection> forwarded_;   ///< The inner signals forwarded to this one's.
};

} // namespace affresco::format
