#include "sgl/vcs/Repository.hpp"

#include <algorithm>
#include <ctime>
#include <utility>

namespace sgl::vcs {

using expect::fail;

namespace {

/// @brief The current time.
/// @return UTC, ISO 8601.
std::string utcNow() {
	const std::time_t now = std::time(nullptr);
	char buffer[32];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
	return buffer;
}

/// @brief Reads the value at a JSON Pointer.
/// @param value The JSON.
/// @param pointer The pointer.
/// @return A copy, or std::nullopt if absent.
std::optional<json> at(const json& value, const std::string& pointer) {
	const json::json_pointer p(pointer);
	if(!value.contains(p)) return std::nullopt;
	return value[p];
}

} // namespace

Repository::Repository(const DocumentRules& rules, json document) : rules_(rules) {
	rules_.validate(document);
	if(document.contains("state")) state_ = expect::object(document["state"], "state");
	document_ = std::move(document);
	history_.push_back({0, "file", "original", document_, json::array(), utcNow()});
}

HistoryEntry Repository::head() const {
	const Revision& r = history_.back();
	return {r.number, r.author, r.message, r.time};
}

void Repository::documentChanged() {
	if(observer_) observer_->documentChanged();
}

void Repository::proposalsChanged() {
	if(observer_) observer_->proposalsChanged();
	onReviewChanged();
}

/* Checks */

// "#id/rest" -> the JSON Pointer of the node with that id + "/rest"
std::string Repository::resolvePath(const json& doc, const std::string& path, const std::string& where) const {
	if(!path.starts_with('#')) return path;

	const std::size_t slash = path.find('/');
	const std::string id = path.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
	const auto pointer = rules_.findNode(doc, id);
	if(!pointer) fail(where, "no node with id '" + id + "'");
	return *pointer + (slash == std::string::npos ? "" : path.substr(slash));
}

// The rules every revision must follow, whoever wrote it
void Repository::checkPermissions(const json& newDocument, const json& newState) const {
	rules_.checkChange(document_, newDocument);

	for(const std::string& path : rules_.lockedPaths(document_)) {
		bool same = false;
		if(path.starts_with('#')) {
			const std::string id = path.substr(1);
			const auto before = rules_.findNode(document_, id), after = rules_.findNode(newDocument, id);
			same = before && after && document_[json::json_pointer(*before)] == newDocument[json::json_pointer(*after)];
		} else if(path == "/state" || path.starts_with("/state/")) {
			const std::string inState = path.substr(6);
			same = at(state_, inState) == at(newState, inState);
		} else {
			same = at(document_, path) == at(newDocument, path);
		}
		if(!same) fail("revision", "permission denied: '" + path + "' is locked");
	}
}

// Validates a revision and computes the document and state it leads to. Changes nothing.
Repository::Prepared Repository::prepare(const json& revision) const {
	expect::object(revision, "revision");
	const int headNumber = history_.back().number;
	if(revision.contains("parent") && expect::integer(revision["parent"], "revision.parent") != headNumber)
		fail("revision.parent", "conflict: based on revision " + revision["parent"].dump() +
		                            ", but the current one is " + std::to_string(headNumber));
	if(!revision.contains("patch") || !revision["patch"].is_array())
		fail("revision.patch", "expected a JSON Patch array");

	// "/state/..." operations change the live state; everything else changes the document
	json documentOps = json::array(), stateOps = json::array();
	for(std::size_t i = 0; i < revision["patch"].size(); ++i) {
		const std::string where = "revision.patch[" + std::to_string(i) + "]";
		json op = expect::object(revision["patch"][i], where);
		for(const char* key : {"path", "from"})
			if(op.contains(key))
				op[key] = resolvePath(document_, expect::string(op[key], where + "." + key), where + "." + key);

		const std::string path = op.value("path", "");
		if(path == "/state" || path.starts_with("/state/")) {
			op["path"] = path.substr(6);
			stateOps.push_back(std::move(op));
		} else {
			documentOps.push_back(std::move(op));
		}
	}

	Prepared result;
	try {
		result.document = document_.patch(documentOps);
		result.state = state_.patch(stateOps);
	} catch(const json::exception& error) {
		fail("revision.patch", error.what());
	}
	if(!result.state.is_object()) fail("revision.patch", "the state must stay an object");

	checkPermissions(result.document, result.state);
	rules_.validate(result.document);
	return result;
}

/* Changes */

// Either everything changes or nothing does: if the observer refuses the new version, it is rolled back
std::optional<std::string> Repository::commit(const json& revision) {
	endPreview();
	try {
		Prepared next = prepare(revision);

		json oldDocument = std::exchange(document_, std::move(next.document));
		json oldState = std::exchange(state_, std::move(next.state));
		history_.push_back({history_.back().number + 1, revision.value("author", "unknown"),
		                    toText(revision.value("message", json(""))), document_, revision["patch"], utcNow()});

		try {
			documentChanged();
		} catch(const DocumentError&) {
			history_.pop_back();
			document_ = std::move(oldDocument);
			state_ = std::move(oldState);
			documentChanged();
			throw;
		}
	} catch(const DocumentError& error) {
		return error.what();
	}
	if(!importing_) onCommit(head());
	return std::nullopt;
}

RevisionResult Repository::submit(const json& revision) {
	const int headNumber = history_.back().number;
	const std::string author = revision.is_object() ? revision.value("author", "unknown") : "unknown";
	const Policy policy = rules_.policyFor(document_, author);

	if(policy == Policy::Deny)
		return {RevisionStatus::Rejected, headNumber, 0,
		        "permission denied: '" + author + "' may not change this screen"};

	if(policy == Policy::Review && !autoAccept_) {
		try {
			(void)prepare(revision); // reject what could never apply now, while its author is still listening
		} catch(const DocumentError& error) {
			return {RevisionStatus::Rejected, headNumber, 0, error.what()};
		}
		proposals_.push_back({nextProposal_, revision, utcNow()});
		proposalsChanged();
		return {RevisionStatus::Proposed, headNumber, nextProposal_++, {}};
	}

	if(const auto error = commit(revision)) return {RevisionStatus::Rejected, headNumber, 0, *error};
	return {RevisionStatus::Applied, history_.back().number, 0, {}};
}

json Repository::restoreRevision(int number, std::string_view author) const {
	const auto it = std::ranges::find(history_, number, &Revision::number);
	if(it == history_.end()) fail("restore", "no revision " + std::to_string(number));
	json patch = json::diff(document_, it->document);
	if(patch.empty())
		fail("restore", "nothing to restore: the screen already looks like revision " + std::to_string(number));
	return {{"author", std::string(author)},
	        {"message", "Restored revision " + std::to_string(number)},
	        {"patch", std::move(patch)}};
}

/* Review */

std::vector<ProposalInfo> Repository::proposals() const {
	std::vector<ProposalInfo> result;
	for(const Proposal& p : proposals_)
		result.push_back({p.id, p.revision.value("author", "unknown"), toText(p.revision.value("message", json(""))),
		                  p.revision["patch"].size(), p.time});
	return result;
}

std::optional<std::string> Repository::accept(int proposal) {
	const auto it = std::ranges::find(proposals_, proposal, &Proposal::id);
	if(it == proposals_.end()) return "sGLGUI screen: no proposal " + std::to_string(proposal);

	const json revision = it->revision;
	proposals_.erase(it);
	proposalsChanged();

	const auto error = commit(revision);
	onDecision({proposal, error, history_.back().number});
	return error;
}

bool Repository::reject(int proposal, std::string_view reason) {
	const auto it = std::ranges::find(proposals_, proposal, &Proposal::id);
	if(it == proposals_.end()) return false;

	if(previewing_ == proposal) endPreview();
	proposals_.erase(it);
	proposalsChanged();
	onDecision({proposal, "rejected by the user" + (reason.empty() ? std::string() : ": " + std::string(reason)),
	            history_.back().number});
	return true;
}

std::optional<std::string> Repository::preview(int proposal) {
	const auto it = std::ranges::find(proposals_, proposal, &Proposal::id);
	if(it == proposals_.end()) return "sGLGUI screen: no proposal " + std::to_string(proposal);

	endPreview();
	try {
		Prepared next = prepare(it->revision);
		committedDocument_ = std::exchange(document_, std::move(next.document));
		committedState_ = std::exchange(state_, std::move(next.state));
		previewing_ = proposal;
		documentChanged();
	} catch(const DocumentError& error) {
		endPreview();
		return error.what();
	}
	onReviewChanged();
	return std::nullopt;
}

void Repository::endPreview() {
	if(!previewing_) return;
	previewing_.reset();
	document_ = std::move(committedDocument_);
	state_ = std::move(committedState_);
	documentChanged();
	onReviewChanged();
}

bool Repository::mayReview(std::string_view author) const { return rules_.mayReview(document_, author); }

void Repository::setAutoAccept(bool enabled) {
	if(autoAccept_ == enabled) return;
	autoAccept_ = enabled;
	onReviewChanged();
}

/* History */

std::vector<HistoryEntry> Repository::history() const {
	std::vector<HistoryEntry> result;
	for(const Revision& r : history_) result.push_back({r.number, r.author, r.message, r.time});
	return result;
}

json Repository::exportChanges(int fromRevision, std::string_view name) const {
	json revisions = json::array();
	for(const Revision& r : history_) {
		if(r.number <= fromRevision || r.number == 0) continue;
		revisions.push_back({{"author", r.author}, {"message", r.message}, {"time", r.time}, {"patch", r.patch}});
	}
	return {{"format", "sglgui-changes"},
	        {"version", 1},
	        {"name", name.empty() ? "changes" : std::string(name)},
	        {"from", fromRevision},
	        {"to", history_.back().number},
	        {"revisions", revisions}};
}

// Applies a change set revision by revision
std::optional<std::string> Repository::importChanges(const json& changes) {
	try {
		expect::object(changes, "changes");
		if(changes.value("format", "") != "sglgui-changes") fail("changes.format", "expected \"sglgui-changes\"");
		if(changes.value("version", 0) != 1) fail("changes.version", "unsupported version (expected 1)");
		if(!changes.contains("revisions") || !changes["revisions"].is_array())
			fail("changes.revisions", "expected a list");
	} catch(const DocumentError& error) {
		return error.what();
	}

	endPreview();
	const json oldDocument = document_, oldState = state_;
	const std::size_t oldHistory = history_.size();
	const std::string name = changes.value("name", "changes");

	importing_ = true;
	for(std::size_t i = 0; i < changes["revisions"].size(); ++i) {
		json revision = changes["revisions"][i];
		if(revision.is_object()) {
			revision.erase("parent"); // a change set applies on top of whatever is there
			revision["author"] = "plugin:" + name;
		}
		if(const auto error = commit(revision)) {
			importing_ = false;
			history_.resize(oldHistory, history_.front());
			document_ = oldDocument;
			state_ = oldState;
			documentChanged();
			return "changes.revisions[" + std::to_string(i) + "]: " + *error;
		}
	}
	importing_ = false;
	for(std::size_t i = oldHistory; i < history_.size(); ++i) {
		const Revision& r = history_[i];
		onCommit({r.number, r.author, r.message, r.time});
	}
	return std::nullopt;
}

} // namespace sgl::vcs
