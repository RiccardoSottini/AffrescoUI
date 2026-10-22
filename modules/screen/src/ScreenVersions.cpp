/// @file
/// @brief affresco::Screen's version control: revisions, proposals, history, change sets and modes, forwarded to its
/// repository.

#include "ScreenImpl.hpp"

#include <deque>

namespace affresco {

namespace {

/// @brief Parses JSON text.
/// @param text The text.
/// @param what What it is, for the error.
/// @param error Receives the error, if any (may be nullptr).
/// @return The value, or std::nullopt.
std::optional<json> parseJson(std::string_view text, const char* what, std::string* error) {
	try {
		return parseUntrusted(text);
	} catch(const json::parse_error& parseError) {
		if(error) *error = std::string("AffrescoUI screen: invalid ") + what + " JSON: " + parseError.what();
	} catch(const DocumentError& deep) {
		if(error) *error = deep.what();
	}
	return std::nullopt;
}

/// @brief Turns an optional error into a bool.
/// @param failure The error, if any.
/// @param error Receives it (may be nullptr).
/// @return true if there was no error.
bool succeeded(const std::optional<std::string>& failure, std::string* error) {
	if(failure && error) *error = *failure;
	return !failure;
}

} // namespace

/* Version control */

bool Screen::applyRevision(std::string_view revisionJson, std::string* error) {
	const auto revision = parseJson(revisionJson, "revision", error);
	return revision && succeeded(impl_->repository.commit(*revision), error);
}

RevisionResult Screen::submitRevision(std::string_view revisionJson) {
	std::string error;
	const auto parsed = parseJson(revisionJson, "revision", &error);
	if(!parsed) return {RevisionStatus::Rejected, revision(), {}, 0, error};
	return impl_->repository.submit(*parsed);
}

std::vector<ProposalInfo> Screen::proposals() const { return impl_->repository.proposals(); }

bool Screen::acceptProposal(int id, std::string_view head, std::string* error) {
	return succeeded(impl_->repository.accept(id, {"user", std::string(head), false}), error);
}

bool Screen::rejectProposal(int id, std::string_view reason) { return impl_->repository.reject(id, reason); }

bool Screen::previewProposal(int id, std::string* error) { return succeeded(impl_->repository.preview(id), error); }

void Screen::endPreview() { impl_->repository.endPreview(); }

std::optional<int> Screen::previewedProposal() const noexcept { return impl_->repository.previewing(); }

void Screen::setScenarioCheck(std::function<std::string(const std::string& documentJson)> check) {
	if(!check) return impl_->images.setScenarioCheck({});
	impl_->images.setScenarioCheck([check = std::move(check)](const std::string& document) {
		const json results = json::parse(check(document), nullptr, false);
		return results.is_array() ? results : json::array();
	});
}

bool Screen::setInspector(bool on, bool statistics, std::string* error) {
	impl_->images.inspector().setShowStatistics(statistics);
	const std::optional<std::string> failed = impl_->images.inspect(on);
	if(failed && error) *error = *failed;
	return !failed;
}

std::vector<std::string> Screen::inspectorTrace() const {
	const std::deque<std::string>& trace = impl_->images.inspector().trace();
	return {trace.begin(), trace.end()};
}

std::vector<CommitInfo> Screen::log() const { return impl_->repository.log(); }

bool Screen::restore(int revision, std::string* error) {
	try {
		return succeeded(impl_->repository.commit(impl_->repository.restoreRevision(revision, "user")), error);
	} catch(const DocumentError& failure) {
		return succeeded(failure.what(), error);
	}
}

RevisionResult Screen::submitRestore(int revision, std::string_view author) {
	try {
		return impl_->repository.submit(impl_->repository.restoreRevision(revision, author));
	} catch(const DocumentError& failure) {
		return {RevisionStatus::Rejected, this->revision(), {}, 0, failure.what()};
	}
}

bool Screen::revert(int revision, std::string* error) {
	try {
		return succeeded(impl_->repository.commit(impl_->repository.revertRevision(revision, "user")), error);
	} catch(const DocumentError& failure) {
		return succeeded(failure.what(), error);
	}
}

bool Screen::undo(std::string* error) {
	if(revision() == 0) return succeeded("AffrescoUI screen: nothing to undo", error);
	return revert(revision(), error);
}

// NOLINTNEXTLINE(bugprone-exception-escape): head() only throws if memory runs out, which ends the program anyway
int Screen::revision() const noexcept { return impl_->repository.head().number; }

std::string Screen::exportChanges(int fromRevision, std::string_view name) const {
	return impl_->repository.exportChanges(fromRevision, name).dump(2);
}

bool Screen::importChanges(std::string_view changesJson, std::string* error) {
	const auto changes = parseJson(changesJson, "changes", error);
	return changes && succeeded(impl_->repository.importChanges(*changes), error);
}

std::string Screen::installChanges(std::string_view changesJson, std::string* error) {
	const auto changes = parseJson(changesJson, "changes", error);
	if(!changes) return {};
	try {
		return std::string(vcs::toString(impl_->repairs.install(*changes)));
	} catch(const DocumentError& failure) {
		if(error) *error = failure.what();
		return {};
	}
}

void Screen::setProofCheck(
    std::function<std::string(const std::string& documentJson, const std::string& proofJson)> check) {
	if(!check) return impl_->repairs.setProofCheck({});
	impl_->repairs.setProofCheck(
	    [check = std::move(check)](const json& document, const json& proof) -> std::optional<std::string> {
		    std::string failed = check(document.dump(), proof.dump());
		    if(failed.empty()) return std::nullopt;
		    return failed;
	    });
}

std::string Screen::repairs() const { return impl_->repairs.status().dump(); }

bool Screen::hasImported(std::string_view changesJson) const {
	const auto changes = parseJson(changesJson, "changes", nullptr);
	return changes && impl_->repository.hasImported(*changes);
}

std::string Screen::documentJson() const { return impl_->repository.document().dump(); }

std::string Screen::stateJson() const { return impl_->repository.state().dump(); }

VersionControl& Screen::versions() noexcept { return impl_->repository; }

void Screen::setMode(Mode mode) { impl_->repository.setMode(mode); }

void Screen::setModeForThisRun(Mode mode) { impl_->repository.setModeForThisRun(mode); }

} // namespace affresco
