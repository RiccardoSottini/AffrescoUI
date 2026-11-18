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

const format::UpdateStatistics& Screen::lastUpdate() const noexcept { return impl_->runtime.lastUpdate(); }

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

// NOLINTNEXTLINE(bugprone-exception-escape): only running out of memory escapes, which ends the program anyway
std::string Screen::undoPlan(int revision, std::string_view optionsJson) const {
	UndoPlan plan;
	std::string error;
	const std::optional<json> options =
	    optionsJson.empty() ? std::optional<json>(json::object()) : parseJson(optionsJson, "undo", &error);
	try {
		if(options && options->is_object())
			plan = impl_->repository.undoPlan(revision, "user", undoOptionsFrom(*options, "undo"));
		else if(options) plan.blocked = "undo: expected an object of choices";
		else plan.blocked = error;
	} catch(const DocumentError& failure) {
		plan.blocked = failure.what();
	}
	return toJson(plan).dump();
}

bool Screen::revert(int revision, std::string_view optionsJson, std::string* error) {
	const std::optional<json> options =
	    optionsJson.empty() ? std::optional<json>(json::object()) : parseJson(optionsJson, "undo", error);
	if(!options) return false;
	if(!options->is_object()) return succeeded("undo: expected an object of choices", error);
	try {
		const UndoOptions choices = undoOptionsFrom(*options, "undo");
		return succeeded(impl_->repository.commit(impl_->repository.undoRevision(revision, "user", choices)), error);
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

format::Package Screen::exportPackage(const format::PackageOptions& options) const {
	if(!impl_->app) throw DocumentError("AffrescoUI package: the screen isn't an app's (open its folder or app.json)");
	const format::AppManifest& app = *impl_->app;
	const std::filesystem::path entry(app.entry);
	const std::string notation = entry.extension() == ".lisp" ? "lisp" : "json";
	json files;
	if(options.kind == format::PackageKind::App) {
		// the app's folder: the entry's, less the folders the manifest names it in
		std::filesystem::path folder = impl_->screenFolder;
		for(auto part = entry.parent_path(); !part.empty() && part != part.parent_path(); part = part.parent_path())
			folder = folder.parent_path();
		std::error_code error;
		const std::filesystem::path manifest =
		    std::filesystem::exists(folder / "app.json", error) ? folder / "app.json" : folder / "app.lisp";
		files = format::appFiles(manifest);
	}
	const vcs::Repository& repository = impl_->repository;
	return format::buildPackage(
	    repository.versionParts(), options, app, notation,
	    [&repository](std::string_view name) { return repository.assetBytes(name); }, files);
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

std::vector<AssetInfo> Screen::assets() const { return impl_->repository.assets(); }

std::shared_ptr<const std::string> Screen::assetBytes(std::string_view name) const {
	return impl_->repository.assetBytes(name);
}

Attached Screen::attach(std::string_view name, std::string bytes) {
	const std::string_view type = assetTypeOf(name);
	if(type.empty())
		return {{},
		        "'" + std::string(name.substr(0, 100)) +
		            "' isn't a kind of file an asset may be (PNG, JPEG, SVG, "
		            "WAV, MP3, FLAC, JSON or text)"};
	return impl_->repository.attach(name, type, std::move(bytes), json::object());
}

} // namespace affresco
