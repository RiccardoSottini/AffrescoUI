/// @file
/// @brief vcs::Repository: commits and main, and checking, judging and landing a revision. Proposals are in
/// RepositoryProposals.cpp, permission rules in PolicyRepository.cpp, change sets in RepositoryChanges.cpp, and
/// keeping it all on disk in RepositoryHistory.cpp.

#include "affresco/vcs/Repository.hpp"

#include "revisions/Budget.hpp"
#include "revisions/Deltas.hpp"
#include "revisions/Revisions.hpp"
#include "history/HistoryStore.hpp" // complete, for history_ (the constructor destroys it if it throws)
#include "permissions/Permissions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <utility>

namespace affresco::vcs {

using expect::fail;

namespace {

/// @brief Refuses a revision that would make the screen nest deeper than maxDocumentNesting. A screen file is trusted
/// and may nest deeper already: then only a revision that makes it deeper still is refused.
/// @param before The document with the live state in "state", before the revision.
/// @param after The same, after it.
/// @throws DocumentError If it would.
void expectShallowEnough(const json& before, const json& after) {
	const int depth = nestingDepth(after);
	if(depth > maxDocumentNesting && depth > nestingDepth(before))
		fail("revision", "the screen would nest " + std::to_string(depth) + " levels deep (the live state one level " +
		                     "inside it), more than " + std::to_string(maxDocumentNesting));
}

/// @brief Reads a whole decimal number.
/// @param text The digits.
/// @return The number, or std::nullopt.
std::optional<std::size_t> digits(std::string_view text) {
	std::size_t value = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if(text.empty() || error != std::errc() || end != text.data() + text.size()) return std::nullopt;
	return value;
}

/// @brief Takes "~n" off the end of a commit's name: how many first parents to go back from it.
/// @param name The name, e.g. "HEAD~2"; keeps what comes before the '~' ("HEAD").
/// @return n: 1 for a '~' alone, 0 without one.
/// @throws DocumentError If what follows the '~' isn't a number.
std::size_t stepsBack(std::string& name) {
	const std::size_t tilde = name.find('~');
	if(tilde == std::string::npos) return 0;
	const auto n = tilde + 1 == name.size() ? std::optional<std::size_t>(1) : digits(name.substr(tilde + 1));
	if(!n) fail("revision", "'" + name + "': expected a number after '~'");
	name.resize(tilde);
	return *n;
}

/// @brief Tells whether a name can be a commit's id or the start of one: 4 to 64 lowercase hex digits.
/// @param name The name.
/// @return true if it can.
bool idPrefix(std::string_view name) {
	return name.size() >= 4 && name.size() <= 64 &&
	       std::ranges::all_of(name, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

} // namespace

Repository::Repository(const DocumentRules& rules, json document) : rules_(rules) {
	if(!rules_.upgrade(document).empty())
		affresco::log(LogLevel::Info, "AffrescoUI history: the screen was written for an older format version, read as the current one");
	rules_.validate(document);
	if(document.contains("state")) state_ = expect::object(document["state"], "state");

	Commit first;
	first.author = first.committer = "file";
	first.message = "original";
	first.time = detail::utcNow();
	main_.push_back(store(std::move(first), document, nullptr).id);
	document_ = std::move(document);
}

/* Objects */

CommitInfo Repository::info(const Commit& c) {
	return {c.id, c.number, c.parents, c.author, c.committer, c.message, c.time, c.proposal};
}

std::string Repository::describe(const Commit& c) {
	return "revision " + std::to_string(c.number) + " (" + c.id.substr(0, 7) + ")";
}

// The id covers everything that makes the commit what it is, the parents included, so it stands for its history
std::string Repository::idOf(const Commit& commit) {
	std::string object = "tree " + commit.tree + "\n";
	for(const std::string& parent : commit.parents) object += "parent " + parent + "\n";
	object += "author " + commit.author + "\ncommitter " + commit.committer + "\ntime " + commit.time + "\npatch " +
	          detail::sha256(commit.patch.dump()) + "\n";
	if(commit.proposal) object += "proposal " + std::to_string(commit.proposal) + "\n";
	object += "\n" + commit.message;
	return detail::sha256(object);
}

// A commit keeps the delta from its first parent; every snapshotInterval-th along first parents keeps its document too
const Repository::Commit& Repository::store(Commit commit, const json& document, const json* parentDocument) {
	commit.tree = detail::sha256(document.dump());
	commit.id = idOf(commit);
	if(const auto known = objects_.find(commit.id); known != objects_.end()) return known->second;

	commit.delta = parentDocument ? detail::delta(*parentDocument, document) : json();
	commit.sinceSnapshot = parentDocument ? objects_.at(commit.parents.front()).sinceSnapshot + 1 : 0;
	commit.snapshot.reset();
	if(!parentDocument || commit.sinceSnapshot >= snapshotInterval) {
		commit.snapshot = std::make_shared<const json>(document);
		commit.sinceSnapshot = 0;
	}
	const std::string id = commit.id;
	return objects_.emplace(id, std::move(commit)).first->second;
}

const Repository::Commit& Repository::find(const json& revision) const {
	if(revision.is_number_integer()) {
		const auto n = revision.get<long long>();
		if(n < 0 || std::cmp_greater_equal(n, main_.size()))
			fail("revision", "no revision " + std::to_string(n) + " (main is at revision " +
			                     std::to_string(main_.size() - 1) + ")");
		return objects_.at(main_[static_cast<std::size_t>(n)]);
	}
	if(!revision.is_string())
		fail("revision", R"(expected a revision number or a commit (an id, "HEAD~1", "main", "proposal/2"))");

	std::string name = revision.get<std::string>();
	const std::size_t back = stepsBack(name);
	const Commit* commit = &named(name);
	for(std::size_t i = 0; i < back; ++i) {
		if(commit->parents.empty()) fail("revision", "'" + revision.get<std::string>() + "' goes back past revision 0");
		commit = &objects_.at(commit->parents.front());
	}
	return *commit;
}

// "HEAD" or "main", "proposal/<n>", or a commit's id or the start of one
const Repository::Commit& Repository::named(const std::string& name) const {
	if(name == "HEAD" || name == "main") return mainHead();
	if(name.starts_with("proposal/")) {
		const auto id = digits(name.substr(9));
		const Proposal* proposal = id && *id <= 1'000'000 ? findProposal(static_cast<int>(*id)) : nullptr;
		if(!proposal) fail("revision", "no open proposal '" + name + "'");
		return objects_.at(proposal->head);
	}
	if(!idPrefix(name)) fail("revision", "unknown revision '" + name + "'");
	const Commit* commit = nullptr;
	for(const auto& [id, object] : objects_) {
		if(!id.starts_with(name)) continue;
		if(commit) fail("revision", "'" + name + "' is ambiguous: give more of the commit id");
		commit = &object;
	}
	if(!commit) fail("revision", "no commit '" + name + "'");
	return *commit;
}

Repository::Proposal* Repository::findProposal(int id) {
	const auto it = std::ranges::find(proposals_, id, &Proposal::id);
	return it == proposals_.end() ? nullptr : &*it;
}

const Repository::Proposal* Repository::findProposal(int id) const {
	const auto it = std::ranges::find(proposals_, id, &Proposal::id);
	return it == proposals_.end() ? nullptr : &*it;
}

void Repository::documentChanged() {
	if(observer_) observer_->documentChanged();
}

void Repository::proposalsChanged() {
	if(observer_) observer_->proposalsChanged();
	onReviewChanged();
}

/* Checks */

std::string Repository::hintFor(const json& view, const std::string& path) const {
	const std::string hint = rules_.pathHint(view, path);
	return hint.empty() ? "" : " (" + hint + ")";
}

// The first path of a failed operation that finds nothing: "from" for a move or a copy, else "path"
std::string Repository::pathsHint(const json& view, const json& paths) const {
	for(const json& path : paths) {
		const json& written = path[0];
		const json& resolved = path[1];
		if(!written.is_string() || !resolved.is_string()) continue;
		if(const std::string hint = hintFor(view, written.get<std::string>()); !hint.empty())
			return " at " + resolved.get<std::string>() + hint;
	}
	return "";
}

// "#id/rest" -> the JSON Pointer of the node with that id + "/rest"
std::string Repository::resolvePath(const json& view, const std::string& path, const std::string& where) const {
	if(!path.starts_with('#')) return path;

	const std::size_t slash = path.find('/');
	const std::string id = path.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
	const auto pointer = rules_.findNode(view, id);
	if(!pointer) fail(where, "no node with id '" + id + "'" + hintFor(view, path));
	return *pointer + (slash == std::string::npos ? "" : path.substr(slash));
}

void Repository::checkParent(const json& revision) const {
	if(!revision.contains("parent")) return;
	const Commit& parent = find(revision["parent"]);
	if(parent.id != main_.back())
		fail("revision.parent", "conflict: based on " + describe(parent) + ", but main is at " + describe(mainHead()) +
		                            ": read the screen again");
}

// "set" becomes "replace" or "add"; nodes added without an id get one, in the commit too, so history and diffs
// always show it
json Repository::normalise(json& op, const json& written, const json& view) const {
	std::string kind = op.value("op", "");
	if(kind == "set") {
		kind = detail::locate(view, op["path"].get<std::string>()) ? "replace" : "add";
		op["op"] = kind;
	}
	if((kind == "add" || kind == "replace") && op.contains("value") && op["path"].is_string())
		rules_.nameNodes(view, op["path"].get<std::string>(), kind == "replace", op["value"]);

	json stored = written;
	if(stored.value("op", "") == "set") stored["op"] = kind;
	if(op.contains("value")) stored["value"] = op["value"];
	return stored;
}

void Repository::apply(json& view, json op, const json& written, const std::string& where) const {
	// The paths as written ("#title/colr") for the hint, and resolved, to say where it failed
	const json paths = {{written.value("from", json()), op.value("from", json())},
	                    {written.value("path", json()), op.value("path", json())}};
	try {
		detail::applyOperation(view, std::move(op), where);
	} catch(const json::exception& error) {
		fail(where, detail::reasonOf(error) + pathsHint(view, paths));
	}
}

// What a "test" reads isn't touched; what a "move" takes from is
void Repository::resolvePaths(json& op, const std::string& kind, const json& view, const std::string& where,
                              std::vector<std::string>& touches) const {
	for(const char* key : {"path", "from"}) {
		if(!op.contains(key)) continue;
		op[key] = resolvePath(view, expect::string(op[key], where + "." + key), where + "." + key);
		if(kind != "test" && (key == std::string_view("path") || kind == "move"))
			if(std::string touched = detail::area(rules_, view, op[key]);
			   std::ranges::find(touches, touched) == touches.end())
				touches.push_back(std::move(touched));
	}
}

// One operation at a time, on the document as rules see it ("/state" is the live state), so "#id" paths find nodes
// that earlier operations added or moved
Repository::Prepared Repository::prepare(const json& patch, const json& document, const json& state) const {
	if(!patch.is_array()) fail("revision.patch", "expected a JSON Patch array");
	rules_.checkRevision(patch); // before any of it is applied

	detail::RevisionBudget budget(patch); // before any work: a peer's revision may be made to be slow
	Prepared result;
	result.before = detail::view(document, state);
	json after = result.before;
	for(std::size_t i = 0; i < patch.size(); ++i) {
		const std::string at = std::format("revision.patch[{}]", i);
		json op = expect::object(patch[i], at);
		const std::string written = op.contains("path") && op["path"].is_string() ? op["path"].get<std::string>() : "";
		const std::string kind = op.contains("op") && op["op"].is_string() ? op["op"].get<std::string>() : "";
		const std::string where = std::format("{} ({} {})", at, kind, written);
		resolvePaths(op, kind, after, where, result.touches);
		result.patch.push_back(normalise(op, patch[i], after));
		budget.spend(after, op, where);
		apply(after, std::move(op), patch[i], where);
	}
	if(!after.is_object() || !after.contains("state") || !after["state"].is_object())
		fail("revision.patch", "the state must stay an object");

	// The document keeps the "state" section it was loaded with; the live state is separate
	result.state = after["state"];
	result.document = after;
	if(document.contains("state")) result.document["state"] = document["state"];
	else result.document.erase("state");
	expectShallowEnough(result.before, after);
	detail::expectNarrowEnough(result.before, after);

	rules_.checkChange(result.before, after); // with the live state: the checker reads state paths
	rules_.validate(result.document);
	result.after = std::move(after);
	return result;
}

/* Changes */

// The observer refusing the new version rolls everything back
// The files on disk keep what they were written as: the upgrade is one more commit on top
void Repository::upgradeHead(json upgraded) {
	Commit commit;
	commit.author = commit.committer = "format";
	commit.message = "format version 2";
	commit.time = detail::utcNow();
	commit.patch = detail::delta(document_, upgraded);
	commit.parents.push_back(main_.back());
	commit.number = mainHead().number + 1;
	main_.push_back(store(std::move(commit), upgraded, &document_).id);
	document_ = std::move(upgraded);
}

const Repository::Commit& Repository::land(Commit commit, json document, json state) {
	endPreview();
	commit.parents.insert(commit.parents.begin(), main_.back());
	commit.number = mainHead().number + 1;
	recordStateChange(commit, state_, state);
	const Commit& stored = store(std::move(commit), document, &document_);

	main_.push_back(stored.id);
	json oldState = std::exchange(state_, std::move(state));
	json oldDocument = std::exchange(document_, std::move(document)); // moved, never copied: the commit keeps a delta
	try {
		documentChanged();
	} catch(const DocumentError&) {
		main_.pop_back();
		state_ = std::move(oldState);
		document_ = std::move(oldDocument);
		documentChanged();
		throw;
	}
	if(!importing_) {
		const bool rebased = rebaseProposals();
		saveHistory(); // before anyone hears of it
		onCommit(info(stored));
		if(rebased) proposalsChanged();
	}
	return stored;
}

std::optional<std::string> Repository::commit(const json& revision) {
	try {
		expect::object(revision, "revision");
		const std::string author = detail::authorOf(revision);
		checkParent(revision);
		Prepared next = prepare(detail::patchOf(revision), mainDocument(), committedState());
		if(const Verdict verdict = evaluate(next.before, next.after, author, false);
		   verdict.kind == Verdict::Kind::Deny)
			fail("revision", verdict.reason);

		Commit c;
		c.author = c.committer = author;
		c.message = detail::messageOf(revision);
		c.time = detail::utcNow();
		c.patch = std::move(next.patch);
		land(std::move(c), std::move(next.document), std::move(next.state));
	} catch(const DocumentError& error) {
		return error.what();
	}
	return std::nullopt;
}

RevisionResult Repository::submit(const json& revision) { return submitRevision(revision, false); }

RevisionResult Repository::propose(const json& revision) { return submitRevision(revision, true); }

RevisionResult Repository::submitRevision(const json& revision, bool alwaysReview) {
	const int number = mainHead().number;
	try {
		expect::object(revision, "revision");
		if(alwaysReview && revision.contains("proposal")) fail("revision.proposal", "it opens a proposal of its own");
		// Evidence is what the host draws and plays itself: an author naming any of it is trying to forge it
		for(const char* key : {"evidence", "behaviour"})
			if(revision.contains(key)) fail(std::string("revision.") + key, "the host gathers it itself");
		const std::string author = detail::authorOf(revision);
		checkReadable(detail::patchOf(revision), author);
		if(revision.contains("proposal")) return push(revision, author);

		checkParent(revision);
		Prepared next = prepare(detail::patchOf(revision), mainDocument(), committedState());
		const Verdict verdict = evaluate(next.before, next.after, author, true);

		if(verdict.kind == Verdict::Kind::Deny) fail("revision", verdict.reason);

		if(verdict.kind == Verdict::Kind::Ask || alwaysReview) {
			const std::string channel = detail::channelOf(author);
			if(std::ranges::count_if(proposals_, [&](const Proposal& p) {
				   return detail::channelOf(p.author) == channel;
			   }) >= static_cast<std::ptrdiff_t>(maxOpenProposals))
				return {RevisionStatus::Rejected,
				        number,
				        {},
				        0,
				        "too many open proposals from '" + channel +
				            "': wait for the reviewer, or push to one of them"};

			Proposal proposal;
			proposal.id = nextProposal_;
			proposal.author = author;
			proposal.message = detail::messageOf(revision);
			proposal.time = detail::utcNow();
			proposal.base = proposal.head = main_.back();
			extend(proposal, revision);
			++nextProposal_;
			proposals_.push_back(std::move(proposal));
			saveHistory();
			proposalsChanged();
			return {RevisionStatus::Proposed, number, proposals_.back().head, proposals_.back().id, {}};
		}

		Commit c;
		c.author = c.committer = author;
		c.message = detail::messageOf(revision);
		c.time = detail::utcNow();
		c.patch = std::move(next.patch);
		const Commit& landed = land(std::move(c), std::move(next.document), std::move(next.state));
		return {RevisionStatus::Applied, landed.number, landed.id, 0, {}};
	} catch(const DocumentError& error) {
		return {RevisionStatus::Rejected, number, {}, 0, error.what()};
	}
}

// As submit() judges a new revision, on main's head; one that can't apply is denied, with the reason
Repository::Judgement Repository::judge(const json& revision) const {
	try {
		expect::object(revision, "revision");
		const std::string author = detail::authorOf(revision);
		checkReadable(detail::patchOf(revision), author);
		const Prepared next = prepare(detail::patchOf(revision), mainDocument(), committedState());
		const Verdict verdict = evaluate(next.before, next.after, author, true);
		static constexpr std::array<const char*, 3> decisions = {"allow", "ask", "deny"};
		return {decisions.at(static_cast<std::size_t>(verdict.kind)), verdict.reason};
	} catch(const DocumentError& error) {
		return {"deny", error.what()};
	}
}

} // namespace affresco::vcs
