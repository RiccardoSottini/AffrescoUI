/// @file
/// @brief vcs::Repository's log and change sets: exporting a range of commits, and checking, trying and importing
/// change sets (VERSION_CONTROL.md, "Change sets: customisations as plugins").

#include "affresco/vcs/Repository.hpp"

#include "revisions/Revisions.hpp"
#include "permissions/Permissions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

namespace affresco::vcs {

using expect::fail;

namespace {

/// @brief A change set as the current format writes it: a version 1 one's patches upgraded (DocumentRules::
/// upgradeRevision()), a current one as it is.
/// @param rules The format's rules.
/// @param changes The change set, checked.
/// @return It, current.
json currentChanges(const DocumentRules& rules, const json& changes) {
	if(changes.value("version", 0) != 1) return changes;
	json current = changes;
	current["version"] = 2;
	for(json& revision : current["revisions"])
		if(revision.is_object() && revision.contains("patch"))
			static_cast<void>(rules.upgradeRevision(revision["patch"]));
	return current;
}

} // namespace

std::vector<CommitInfo> Repository::log() const {
	std::vector<CommitInfo> result;
	result.reserve(main_.size());
	for(const std::string& id : main_) result.push_back(info(objects_.at(id)));
	return result;
}

json Repository::exportChanges(const json& from, std::string_view name) const {
	const Commit& base = find(from);
	const auto start = std::ranges::find(main_, base.id);
	if(start == main_.end()) fail("exportChanges.from", describe(base) + " is not on main");

	json revisions = json::array();
	for(auto it = start + 1; it != main_.end(); ++it) {
		const Commit& c = objects_.at(*it);
		revisions.push_back({{"author", c.author},
		                     {"committer", c.committer},
		                     {"message", c.message},
		                     {"time", c.time},
		                     {"commit", c.id},
		                     {"patch", c.patch}});
	}
	return {{"format", "affrescoui-changes"},
	        {"version", 2},
	        {"name", name.empty() ? "changes" : std::string(name)},
	        {"from", base.number},
	        {"to", mainHead().number},
	        {"base", base.id},
	        {"head", main_.back()},
	        {"revisions", revisions}};
}

// The checks importChanges() and tryChanges() share
void Repository::checkChanges(const json& changes) {
	expect::object(changes, "changes");
	if(changes.value("format", "") != "affrescoui-changes") fail("changes.format", "expected \"affrescoui-changes\"");
	if(const int version = changes.value("version", 0); version != 1 && version != 2)
		fail("changes.version", "unsupported version (expected 1 or 2)");
	if(!changes.contains("revisions") || !changes["revisions"].is_array()) fail("changes.revisions", "expected a list");
}

// Applies a change set revision by revision
std::optional<std::string> Repository::importChanges(const json& changes) {
	try {
		checkChanges(changes);
	} catch(const DocumentError& error) {
		return error.what();
	}
	const json current = currentChanges(rules_, changes);

	endPreview();
	const json oldState = state_, oldDocument = document_;
	const std::size_t oldMain = main_.size();
	const std::string name = current.value("name", "changes");

	importing_ = true;
	for(std::size_t i = 0; i < current["revisions"].size(); ++i) {
		json revision = current["revisions"][i];
		if(revision.is_object()) {
			revision.erase("parent"); // a change set applies on top of whatever is there
			revision["author"] = "plugin:" + name;
		}
		if(const auto error = commit(revision)) {
			importing_ = false;
			main_.resize(oldMain);
			document_ = oldDocument;
			state_ = oldState;
			documentChanged();
			return "changes.revisions[" + std::to_string(i) + "]: " + *error;
		}
	}
	importing_ = false;
	const bool rebased = rebaseProposals();
	if(!hasImported(changes)) imported_.push_back(detail::sha256(changes.dump()));
	saveHistory();
	if(rebased) proposalsChanged();
	for(std::size_t i = oldMain; i < main_.size(); ++i) onCommit(info(objects_.at(main_[i])));
	return std::nullopt;
}

bool Repository::hasImported(const json& changes) const {
	return std::ranges::find(imported_, detail::sha256(changes.dump())) != imported_.end();
}

// What importChanges() does, on copies: the same patches and the same deny rules, nothing committed
json Repository::tryChanges(const json& changes) const {
	checkChanges(changes);
	const json current = currentChanges(rules_, changes);
	const std::string author = "plugin:" + current.value("name", std::string("changes"));
	json document = mainDocument(), state = committedState();
	json view = detail::view(document, state);
	for(std::size_t i = 0; i < current["revisions"].size(); ++i) {
		const std::string where = "changes.revisions[" + std::to_string(i) + "]";
		try {
			const json& revision = current["revisions"][i];
			expect::object(revision, "revision");
			Prepared next = prepare(detail::patchOf(revision), document, state);
			if(const Verdict verdict = evaluate(next.before, next.after, author, false);
			   verdict.kind == Verdict::Kind::Deny)
				fail("revision", verdict.reason);
			document = std::move(next.document);
			state = std::move(next.state);
			view = std::move(next.after);
		} catch(const DocumentError& error) {
			fail(where, detail::reasonOf(error));
		}
	}
	return view;
}

void Repository::recordImported(const json& changes) {
	if(hasImported(changes)) return;
	imported_.push_back(detail::sha256(changes.dump()));
	saveHistory();
}

} // namespace affresco::vcs
