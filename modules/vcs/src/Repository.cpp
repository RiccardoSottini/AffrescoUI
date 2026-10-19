/// @file
/// @brief vcs::Repository: commits, main, proposals, review and change sets. Keeping them on disk is in
/// RepositoryHistory.cpp.

#include "affresco/vcs/Repository.hpp"

#include "Budget.hpp"
#include "Deltas.hpp"
#include "HistoryStore.hpp" // complete, for history_ (the constructor destroys it if it throws)
#include "Permissions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

#include "affresco/core/Portable.hpp"

#include <algorithm>
#include <charconv>
#include <ctime>
#include <utility>

namespace affresco::vcs {

using expect::fail;

namespace {

/// @brief The current time.
/// @return UTC, ISO 8601.
std::string utcNow() {
	const std::tm now = utcTime(std::time(nullptr));
	char buffer[32];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &now);
	return buffer;
}

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

/// @brief A revision's author.
/// @param revision The revision.
/// @return "author", or "unknown".
std::string authorOf(const json& revision) {
	const auto it = revision.find("author");
	return it != revision.end() && it->is_string() ? it->get<std::string>() : "unknown";
}

/// @brief A revision's message.
/// @param revision The revision.
/// @return "message" as text.
std::string messageOf(const json& revision) { return toText(revision.value("message", json(""))); }

/// @brief A revision's changes as JSON Patch: its "patch", then its compact edits in a fixed order: "set" (replace,
/// or add where nothing is there yet), "add", "remove", "move". `{"set": {"#statusBar/color": "@topBar"}}`.
/// @param revision The revision.
/// @return The operations ("set" ones keep op "set" until prepare() sees the document).
/// @throws DocumentError If it has neither, or they are malformed.
json patchOf(const json& revision) {
	json patch = json::array();
	if(revision.contains("patch")) {
		if(!revision["patch"].is_array()) fail("revision.patch", "expected a JSON Patch array");
		patch = revision["patch"];
	}
	auto edits = [&](const char* key) -> const json& {
		if(!revision[key].is_object()) fail(std::string("revision.") + key, "expected an object: path -> value");
		return revision[key];
	};
	if(revision.contains("set"))
		for(const auto& [path, value] : edits("set").items()) patch.push_back({{"op", "set"}, {"path", path}, {"value", value}});
	if(revision.contains("add"))
		for(const auto& [path, value] : edits("add").items()) patch.push_back({{"op", "add"}, {"path", path}, {"value", value}});
	if(revision.contains("remove")) {
		const json& paths = revision["remove"].is_string() ? json::array({revision["remove"]}) : revision["remove"];
		if(!paths.is_array()) fail("revision.remove", "expected a list of paths");
		for(const json& path : paths) patch.push_back({{"op", "remove"}, {"path", expect::string(path, "revision.remove")}});
	}
	if(revision.contains("move"))
		for(const auto& [from, to] : edits("move").items())
			patch.push_back({{"op", "move"}, {"from", from}, {"path", expect::string(to, "revision.move." + from)}});
	if(patch.empty())
		fail("revision", "expected changes: a JSON Patch (\"patch\") or compact edits (\"set\", \"add\", \"remove\", "
		                 "\"move\")");
	return patch;
}

/// @brief A short preview of a value for a reviewer: text as is, the rest as JSON, cut at 48 characters.
/// @param value The value.
/// @return The preview.
std::string preview(const json& value) {
	std::string text = value.is_string() ? value.get<std::string>() : value.dump();
	if(text.size() > 48) text = text.substr(0, 45) + "...";
	return text;
}

/// @brief One operation as a line for a reviewer: "set #statusBar/color to @topBar", "remove #badge".
/// @param op The operation.
/// @return The line.
std::string summary(const json& op) {
	const std::string kind = op.value("op", "");
	const std::string path = op.value("path", "");
	if(kind == "remove") return "remove " + path;
	if(kind == "move" || kind == "copy") return kind + " " + op.value("from", "") + " to " + path;
	if(kind == "test") return "check " + path;
	if(kind == "add") return "add at " + path + ": " + preview(op.value("value", json()));
	return "set " + path + " to " + preview(op.value("value", json()));
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

} // namespace

Repository::Repository(const DocumentRules& rules, json document) : rules_(rules) {
	rules_.validate(document);
	if(document.contains("state")) state_ = expect::object(document["state"], "state");

	Commit first;
	first.author = first.committer = "file";
	first.message = "original";
	first.time = utcNow();
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
		if(n < 0 || n >= static_cast<long long>(main_.size()))
			fail("revision", "no revision " + std::to_string(n) + " (main is at revision " +
			                     std::to_string(main_.size() - 1) + ")");
		return objects_.at(main_[static_cast<std::size_t>(n)]);
	}
	if(!revision.is_string())
		fail("revision", "expected a revision number or a commit (an id, \"HEAD~1\", \"main\", \"proposal/2\")");

	std::string name = revision.get<std::string>();
	std::size_t back = 0;
	if(const std::size_t tilde = name.find('~'); tilde != std::string::npos) {
		const auto n = tilde + 1 == name.size() ? std::optional<std::size_t>(1) : digits(name.substr(tilde + 1));
		if(!n) fail("revision", "'" + name + "': expected a number after '~'");
		back = *n;
		name.resize(tilde);
	}

	const Commit* commit = nullptr;
	if(name == "HEAD" || name == "main") {
		commit = &mainHead();
	} else if(name.starts_with("proposal/")) {
		const auto id = digits(name.substr(9));
		const Proposal* proposal = id && *id <= 1'000'000 ? findProposal(static_cast<int>(*id)) : nullptr;
		if(!proposal) fail("revision", "no open proposal '" + name + "'");
		commit = &objects_.at(proposal->head);
	} else {
		if(name.size() < 4 || name.size() > 64 ||
		   !std::ranges::all_of(name, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
			fail("revision", "unknown revision '" + name + "'");
		for(const auto& [id, object] : objects_) {
			if(!id.starts_with(name)) continue;
			if(commit) fail("revision", "'" + name + "' is ambiguous: give more of the commit id");
			commit = &object;
		}
		if(!commit) fail("revision", "no commit '" + name + "'");
	}

	for(std::size_t i = 0; i < back; ++i) {
		if(commit->parents.empty()) fail("revision", "'" + revision.get<std::string>() + "' goes back past revision 0");
		commit = &objects_.at(commit->parents.front());
	}
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

// One operation at a time, on the document as rules see it ("/state" is the live state), so "#id" paths find nodes
// that earlier operations added or moved
Repository::Prepared Repository::prepare(const json& patch, const json& document, const json& state) const {
	if(!patch.is_array()) fail("revision.patch", "expected a JSON Patch array");

	detail::RevisionBudget budget(patch); // before any work: a peer's revision may be made to be slow
	Prepared result;
	result.before = detail::view(document, state);
	json after = result.before;
	for(std::size_t i = 0; i < patch.size(); ++i) {
		json op = expect::object(patch[i], "revision.patch[" + std::to_string(i) + "]");
		const std::string written = op.contains("path") && op["path"].is_string() ? op["path"].get<std::string>() : "";
		const std::string kind = op.contains("op") && op["op"].is_string() ? op["op"].get<std::string>() : "";
		const std::string where = "revision.patch[" + std::to_string(i) + "] (" + kind + " " + written + ")";
		for(const char* key : {"path", "from"}) {
			if(!op.contains(key)) continue;
			op[key] = resolvePath(after, expect::string(op[key], where + "." + key), where + "." + key);
			if(kind != "test" && (key == std::string_view("path") || kind == "move"))
				if(std::string touched = detail::area(rules_, after, op[key]);
				   std::ranges::find(result.touches, touched) == result.touches.end())
					result.touches.push_back(std::move(touched));
		}

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

// Like Claude Code: deny beats ask beats allow; what no rule allows asks. Rules are judged by the change's effect.
Repository::Verdict Repository::evaluate(const json& before, const json& after, const std::string& author,
                                         bool reviewed) const {
	using Kind = Verdict::Kind;
	const Permissions permissions = rules_.permissions(mainDocument());

	for(const PermissionRule& rule : permissions.deny)
		if(detail::authorMatches(rule.author, author) && detail::changes(rules_, before, after, rule.pattern))
			return {Kind::Deny, "permission denied: '" + author + "' may not change " +
			                        (rule.pattern.empty() ? std::string("this screen") : rule.pattern) + " (rule \"" +
			                        rule.text + "\")"};
	if(!reviewed || !permissions.configured) return {Kind::Allow, {}};

	for(const PermissionRule& rule : permissions.ask)
		if(detail::authorMatches(rule.author, author) && detail::changes(rules_, before, after, rule.pattern))
			return {Kind::Ask, "the rule \"" + rule.text + "\" asks for review", true};

	if(mode_ == Mode::Plan && !mayReview(author))
		return {Kind::Ask, "plan mode: only reviewers change the screen directly"};

	std::vector<std::string> allowed;
	for(const PermissionRule& rule : permissions.allow)
		if(detail::authorMatches(rule.author, author)) allowed.push_back(rule.pattern);
	for(const Grant& grant : grants_)
		if(const auto rule = parsePermissionRule(grant.rule); rule && detail::authorMatches(rule->author, author))
			allowed.push_back(rule->pattern);
	if(detail::onlyInside(rules_, before, after, allowed)) return {Kind::Allow, {}};

	if(mode_ == Mode::AcceptEdits) return {Kind::Allow, {}};
	return {Kind::Ask, "no rule allows '" + author + "' to make this change"};
}

/* Changes */

// The observer refusing the new version rolls everything back
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
		saveHistory(); // before anyone hears of it
		onCommit(info(stored));
	}
	return stored;
}

std::optional<std::string> Repository::commit(const json& revision) {
	try {
		expect::object(revision, "revision");
		const std::string author = authorOf(revision);
		checkParent(revision);
		Prepared next = prepare(patchOf(revision), mainDocument(), committedState());
		if(const Verdict verdict = evaluate(next.before, next.after, author, false);
		   verdict.kind == Verdict::Kind::Deny)
			fail("revision", verdict.reason);

		Commit c;
		c.author = c.committer = author;
		c.message = messageOf(revision);
		c.time = utcNow();
		c.patch = std::move(next.patch);
		land(std::move(c), std::move(next.document), std::move(next.state));
	} catch(const DocumentError& error) {
		return error.what();
	}
	return std::nullopt;
}

RevisionResult Repository::submit(const json& revision) {
	const int number = mainHead().number;
	try {
		expect::object(revision, "revision");
		const std::string author = authorOf(revision);
		if(revision.contains("proposal")) return push(revision, author);

		checkParent(revision);
		Prepared next = prepare(patchOf(revision), mainDocument(), committedState());
		const Verdict verdict = evaluate(next.before, next.after, author, true);

		if(verdict.kind == Verdict::Kind::Deny) fail("revision", verdict.reason);

		if(verdict.kind == Verdict::Kind::Ask) {
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
			proposal.message = messageOf(revision);
			proposal.time = utcNow();
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
		c.message = messageOf(revision);
		c.time = utcNow();
		c.patch = std::move(next.patch);
		const Commit& landed = land(std::move(c), std::move(next.document), std::move(next.state));
		return {RevisionStatus::Applied, landed.number, landed.id, 0, {}};
	} catch(const DocumentError& error) {
		return {RevisionStatus::Rejected, number, {}, 0, error.what()};
	}
}

// Rebased on main's head: the commits are replayed in order (unchanged ones keep their ids), then the new one
void Repository::extend(Proposal& proposal, const json& revision) {
	std::vector<Commit> commits;
	for(std::string id = proposal.head; id != proposal.base; id = objects_.at(id).parents.front())
		commits.insert(commits.begin(), objects_.at(id));

	Commit added;
	added.author = added.committer = proposal.author;
	added.message = messageOf(revision);
	added.time = utcNow();
	added.patch = patchOf(revision);
	commits.push_back(std::move(added));
	// accept() replays them all as one patch: refused now, not once a reviewer has looked at it
	std::size_t operations = 0;
	for(const Commit& c : commits) operations += c.patch.size();
	detail::expectFewEnoughOperations(operations);

	json document = mainDocument(), state = committedState(), patch = json::array();
	std::string parent = main_.back();
	int number = mainHead().number;
	std::vector<std::string> touches;
	for(Commit& c : commits) {
		Prepared next = prepare(c.patch, document, state);
		for(std::string& area : next.touches)
			if(std::ranges::find(touches, area) == touches.end()) touches.push_back(std::move(area));
		state = std::move(next.state);
		c.patch = std::move(next.patch);
		patch.insert(patch.end(), c.patch.begin(), c.patch.end());
		c.parents = {parent};
		c.number = ++number;
		parent = store(std::move(c), next.document, &document).id;
		document = std::move(next.document);
	}

	const json before = detail::view(mainDocument(), committedState()), after = detail::view(document, state);
	const Verdict verdict = evaluate(before, after, proposal.author, true);
	if(verdict.kind == Verdict::Kind::Deny) fail("revision", verdict.reason);

	proposal.base = main_.back();
	proposal.head = parent;
	proposal.commits = commits.size();
	proposal.patch = std::move(patch);
	proposal.reason = verdict.kind == Verdict::Kind::Ask ? verdict.reason : "proposals are always reviewed";
	proposal.remember = rememberedRules(proposal.author, touches, verdict.askedByRule);
	proposal.touches = std::move(touches);
}

std::vector<std::string> Repository::rememberedRules(const std::string& author, const std::vector<std::string>& touches,
                                                     bool askedByRule) {
	std::vector<std::string> rules;
	if(!askedByRule && !touches.empty() && std::ranges::all_of(touches, detail::narrow))
		for(const std::string& area : touches) rules.push_back(detail::channelOf(author) + "(" + area + ")");
	return rules;
}

RevisionResult Repository::push(const json& revision, const std::string& author) {
	const int id = expect::integer(revision["proposal"], "revision.proposal");
	Proposal* proposal = findProposal(id);
	if(!proposal) fail("revision.proposal", "no open proposal " + std::to_string(id));
	if(proposal->author != author)
		fail("revision.proposal",
		     "proposal #" + std::to_string(id) + " is by '" + proposal->author + "': only its author may push to it");
	checkParent(revision);

	Proposal updated = *proposal;
	extend(updated, revision);
	*proposal = std::move(updated);
	saveHistory();
	if(previewing_ == id) endPreview(); // what was shown is out of date
	proposalsChanged();
	return {RevisionStatus::Proposed, mainHead().number, proposal->head, id, {}};
}

json Repository::restoreRevision(const json& revision, std::string_view author) const {
	const Commit& target = find(revision);
	json patch = json::diff(mainDocument(), documentOf(target));
	if(patch.empty()) fail("restore", "nothing to restore: the screen already looks like " + describe(target));
	return {{"author", std::string(author)}, {"message", "Restore " + describe(target)}, {"patch", std::move(patch)}};
}

// The inverse of the commit, refused if a later commit changed any part it touches (a revert never guesses)
json Repository::revertRevision(const json& revision, std::string_view author) const {
	const Commit& target = find(revision);
	if(std::ranges::find(main_, target.id) == main_.end()) fail("revert", describe(target) + " is not on main");
	if(target.parents.empty()) fail("revert", "revision 0 is the screen as loaded: there is nothing to revert");

	const json then = documentOf(target);
	json patch = json::diff(then, documentOf(objects_.at(target.parents.front())));
	checkDocumentRevert(target, then, patch);
	const json undo = stateRevert(target); // the live state too, when the commit changed it in this run
	if(patch.empty() && undo.empty())
		fail("revert", "nothing to revert: " + describe(target) + " changed neither the document nor, in this run, the "
		                                                      "live state");
	patch.insert(patch.end(), undo.begin(), undo.end());
	return {
	    {"author", std::string(author)}, {"message", "Revert \"" + target.message + "\""}, {"patch", std::move(patch)}};
}

void Repository::checkDocumentRevert(const Commit& target, const json& then, const json& patch) const {
	const json& head = mainDocument();
	auto same = [&](const std::string& pointer) {
		const json *now = detail::locate(head, pointer), *before = detail::locate(then, pointer);
		return now && before ? *now == *before : now == before;
	};
	for(const json& op : patch) {
		const json::json_pointer path(op["path"].get<std::string>());
		std::string checked = path.to_string();
		const json* container = detail::locate(then, path.parent_pointer().to_string());
		if(!path.empty() && (path.back() == "-" || (container && container->is_array())))
			checked = path.parent_pointer().to_string();

		// The part must be as the commit left it, inside the same nodes
		bool unchanged = same(checked);
		for(json::json_pointer prefix(checked); unchanged && !prefix.empty(); prefix = prefix.parent_pointer()) {
			const json* node = detail::locate(then, prefix.to_string());
			if(node && node->is_object() && node->contains("id")) unchanged = same(prefix.to_string() + "/id");
		}
		if(!unchanged)
			fail("revert", "conflict: " + (checked.empty() ? std::string("the screen") : checked) + " changed after " +
			                   describe(target) + "; restore an earlier revision instead");
	}
}

json Repository::diff(const json& from, const json& to) const {
	return json::diff(documentOf(find(from)), documentOf(find(to)));
}

/* Review */

std::vector<ProposalInfo> Repository::proposals() const {
	std::vector<ProposalInfo> result;
	for(const Proposal& p : proposals_)
	{
		std::vector<std::string> changes;
		for(const json& op : p.patch) changes.push_back(summary(op));
		result.push_back({p.id, "proposal/" + std::to_string(p.id), p.author, p.message, p.base, p.head, p.commits,
		                  p.patch.size(), p.touches, p.remember, std::move(changes), p.reason, p.time});
	}
	return result;
}

json Repository::proposalDiff(int id) const {
	const Proposal* proposal = findProposal(id);
	if(!proposal) fail("diff", "no proposal " + std::to_string(id));
	const Prepared next = prepare(proposal->patch, mainDocument(), committedState());
	return json::diff(next.before, next.after);
}

// Always a merge commit (like `git merge --no-ff`): the history keeps who approved it, and the branch's commits
std::optional<std::string> Repository::accept(int id, const Approval& approval) {
	Proposal* proposal = findProposal(id);
	if(!proposal) return "AffrescoUI screen: no proposal " + std::to_string(id);

	// Refused approvals leave the proposal open
	if(detail::channelOf(approval.reviewer) == detail::channelOf(proposal->author))
		return "AffrescoUI screen: permission denied: '" + approval.reviewer + "' may not accept its own proposal";
	// Every approval names what was reviewed: without it, a branch pushed to after the review would merge unseen
	if(approval.head.size() < 7)
		return "AffrescoUI screen: accepting proposal #" + std::to_string(id) +
		       " needs its head commit, the version you reviewed (at least 7 characters)";
	if(!proposal->head.starts_with(approval.head))
		return "AffrescoUI screen: proposal #" + std::to_string(id) + " changed since you reviewed it (it is now at " +
		       proposal->head.substr(0, 7) + "): review it again";
	if(approval.dontAskAgain && proposal->remember.empty())
		return "AffrescoUI screen: proposal #" + std::to_string(id) +
		       " can't be remembered (it changes parts that always ask, or too much of the screen): accept it "
		       "without \"don't ask again\"";

	const Proposal taken = *proposal;
	std::erase_if(proposals_, [id](const Proposal& p) { return p.id == id; });
	proposalsChanged();

	std::optional<std::string> error;
	std::string merged;
	try {
		// main may have moved since the proposal was built
		Prepared next = [&] {
			try {
				return prepare(taken.patch, mainDocument(), committedState());
			} catch(const DocumentError& failure) {
				const std::string why = detail::reasonOf(failure);
				fail("proposal #" + std::to_string(id),
				     "no longer applies on " + describe(mainHead()) + " (" + why + "): ask its author for a new one");
			}
		}();
		if(const Verdict verdict = evaluate(next.before, next.after, taken.author, false);
		   verdict.kind == Verdict::Kind::Deny)
			fail("revision", verdict.reason);

		Commit merge;
		merge.parents = {taken.head};
		merge.author = taken.author;
		merge.committer = approval.reviewer;
		merge.message = taken.message;
		merge.time = utcNow();
		merge.proposal = id;
		merge.patch = taken.patch;
		merged = land(std::move(merge), std::move(next.document), std::move(next.state)).id;

		if(approval.dontAskAgain) {
			for(const std::string& rule : taken.remember)
				if(std::ranges::find(grants_, rule, &Grant::rule) == grants_.end())
					grants_.push_back({rule, approval.reviewer, id, utcNow()});
			onReviewChanged();
		}
	} catch(const DocumentError& failure) {
		error = failure.what();
	}
	saveHistory(); // the proposal is closed, merged or not, and grants may have been added
	onDecision({id, error, mainHead().number, merged});
	return error;
}

bool Repository::reject(int id, std::string_view reason) {
	if(!findProposal(id)) return false;

	if(previewing_ == id) endPreview();
	std::erase_if(proposals_, [id](const Proposal& p) { return p.id == id; });
	saveHistory();
	proposalsChanged();
	onDecision({id,
	            "rejected by the reviewer" + (reason.empty() ? std::string() : ": " + std::string(reason)),
	            mainHead().number,
	            {}});
	return true;
}

std::optional<std::string> Repository::preview(int id) {
	const Proposal* proposal = findProposal(id);
	if(!proposal) return "AffrescoUI screen: no proposal " + std::to_string(id);

	endPreview();
	try {
		Prepared next = prepare(proposal->patch, document_, state_);
		committedState_ = std::exchange(state_, std::move(next.state));
		committedDocument_ = std::exchange(document_, std::move(next.document));
		previewing_ = id;
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

// Without rules anyone may; otherwise the reviewers, by exact name or channel ("studio" covers "studio/riccardo")
bool Repository::mayReview(std::string_view author) const {
	const Permissions permissions = rules_.permissions(mainDocument());
	if(!permissions.configured) return true;
	return std::ranges::any_of(permissions.reviewers, [&](const std::string& reviewer) {
		return reviewer == author || reviewer == detail::channelOf(author);
	});
}

void Repository::setMode(Mode mode) {
	if(keptMode_ != mode) {
		keptMode_ = mode;
		saveHistory();
	}
	setModeForThisRun(mode);
}

void Repository::setModeForThisRun(Mode mode) {
	if(mode_ == mode) return;
	mode_ = mode;
	onReviewChanged();
}

bool Repository::revokeGrant(std::string_view rule) {
	if(std::erase_if(grants_, [&](const Grant& g) { return g.rule == rule; }) == 0) return false;
	saveHistory();
	onReviewChanged();
	return true;
}

/* History */

std::vector<CommitInfo> Repository::log() const {
	std::vector<CommitInfo> result;
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
	        {"version", 1},
	        {"name", name.empty() ? "changes" : std::string(name)},
	        {"from", base.number},
	        {"to", mainHead().number},
	        {"base", base.id},
	        {"head", main_.back()},
	        {"revisions", revisions}};
}

// Applies a change set revision by revision
std::optional<std::string> Repository::importChanges(const json& changes) {
	try {
		expect::object(changes, "changes");
		if(changes.value("format", "") != "affrescoui-changes")
			fail("changes.format", "expected \"affrescoui-changes\"");
		if(changes.value("version", 0) != 1) fail("changes.version", "unsupported version (expected 1)");
		if(!changes.contains("revisions") || !changes["revisions"].is_array())
			fail("changes.revisions", "expected a list");
	} catch(const DocumentError& error) {
		return error.what();
	}

	endPreview();
	const json oldState = state_, oldDocument = document_;
	const std::size_t oldMain = main_.size();
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
			main_.resize(oldMain);
			document_ = oldDocument;
			state_ = oldState;
			documentChanged();
			return "changes.revisions[" + std::to_string(i) + "]: " + *error;
		}
	}
	importing_ = false;
	if(!hasImported(changes)) imported_.push_back(detail::sha256(changes.dump()));
	saveHistory();
	for(std::size_t i = oldMain; i < main_.size(); ++i) onCommit(info(objects_.at(main_[i])));
	return std::nullopt;
}

bool Repository::hasImported(const json& changes) const {
	return std::ranges::find(imported_, detail::sha256(changes.dump())) != imported_.end();
}

} // namespace affresco::vcs
