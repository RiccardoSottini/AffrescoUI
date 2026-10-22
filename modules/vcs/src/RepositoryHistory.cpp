/// @file
/// @brief vcs::Repository's history on disk: commits written as objects named by their SHA-256, refs for main and the
/// open proposals, and loading that checks every hash (VERSION_CONTROL.md, "History on disk").

#include "affresco/vcs/Repository.hpp"

#include "HistoryStore.hpp"
#include "Permissions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <ranges>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace affresco::vcs {

using detail::HistoryError;

/// @brief What the refs file holds, read and checked.
struct Repository::Refs {
	std::string main;                  ///< main's head.
	std::vector<Proposal> proposals;   ///< The open proposals, oldest first, without their commits and patch.
	std::vector<Grant> grants;         ///< "Don't ask again" rules.
	Mode mode = Mode::Default;         ///< How much reviewers are asked.
	int nextProposal = 1;              ///< The next proposal's id.
	std::vector<std::string> imported; ///< The change sets applied, by id.
	std::vector<std::string> packs;    ///< The packs that hold objects, oldest first (version 2).
};

namespace {

/// The refs file's "format".
constexpr const char* historyFormat = "affrescoui-history";
/// The version this writes: 2 keeps most commits as deltas (VERSION_CONTROL.md). Version 1, where every commit's
/// document is an object of its own, still loads.
constexpr int historyVersion = 2;

/// @brief Runs a reader of one history file, so that every error it throws names the file.
/// @tparam Read A function with no parameters.
/// @param file The file.
/// @param read The reader.
/// @return What it returns.
/// @throws HistoryError For any malformed content.
template <typename Read> decltype(auto) reading(const std::filesystem::path& file, Read&& read) {
	try {
		return std::forward<Read>(read)();
	} catch(const HistoryError&) {
		throw;
	} catch(const DocumentError& error) {
		throw HistoryError(file, detail::reasonOf(error));
	} catch(const json::exception& error) {
		throw HistoryError(file, detail::reasonOf(error));
	}
}

/// @brief Reads a list of strings.
/// @param value The value.
/// @param where Its place, for errors.
/// @return The strings.
/// @throws DocumentError If it is not a list of strings.
std::vector<std::string> strings(const json& value, const std::string& where) {
	if(!value.is_array()) expect::fail(where, "expected a list of strings");
	std::vector<std::string> result;
	result.reserve(value.size());
	for(const json& item : value) result.push_back(expect::string(item, where));
	return result;
}

/// @brief Reads a whole commit id.
/// @param value The value.
/// @param where Its place, for errors.
/// @return The id.
/// @throws DocumentError If it is not 64 lowercase hex digits.
std::string commitId(const json& value, const std::string& where) {
	std::string id = expect::string(value, where);
	if(!detail::isObjectId(id)) expect::fail(where, "expected a commit id (64 hex digits)");
	return id;
}

/// @brief Reads the ids of the change sets a history applied.
/// @param value The list.
/// @return The ids.
/// @throws DocumentError If it is not a list of ids (64 lowercase hex digits).
std::vector<std::string> changeSetIds(const json& value) {
	std::vector<std::string> ids = strings(value, "imported");
	for(std::size_t i = 0; i < ids.size(); ++i)
		if(!detail::isObjectId(ids[i]))
			expect::fail("imported[" + std::to_string(i) + "]", "expected a change set's id (64 hex digits)");
	return ids;
}

/// @brief Reads the names of the packs a history has.
/// @param value The list (absent from version 1).
/// @return The names.
/// @throws DocumentError If it is not a list of names (64 lowercase hex digits).
std::vector<std::string> packNames(const json& value) {
	std::vector<std::string> names = strings(value, "packs");
	for(std::size_t i = 0; i < names.size(); ++i)
		if(!detail::isObjectId(names[i]))
			expect::fail("packs[" + std::to_string(i) + "]", "expected a pack's name (64 hex digits)");
	return names;
}

/// @brief Checks a rule from "don't ask again": the only ones accept() grants are "<channel>(<one node or area>)", so a
/// broader one in the refs file was written by hand, and would let a channel change anything without review.
/// @param rule The rule.
/// @param where Its place, for errors.
/// @throws DocumentError If it is any other rule.
void expectGrantRule(const std::string& rule, const std::string& where) {
	const auto parsed = parsePermissionRule(rule);
	if(!parsed || parsed->pattern.empty() || parsed->author == "*" || !detail::narrow(parsed->pattern))
		expect::fail(where,
		             "'" + rule + "' is not a rule that \"don't ask again\" grants (<channel>(<one node or area>))");
}

/// @brief Reads a grant's expiry: a UTC time, "2026-10-22T08:30:00Z" exactly, which sorts as text, so expired() can
/// compare it with the time now. Anything else ("never", "9999", a local time) would sort after every time and never
/// expire, so it is refused.
/// @param value The value.
/// @param at Its place.
/// @return The time.
/// @throws DocumentError If it isn't one.
std::string expiryOf(const json& value, const std::string& at) {
	std::string time = expect::string(value, at);
	constexpr std::string_view shape = "dddd-dd-ddTdd:dd:ddZ";
	const bool matches = time.size() == shape.size() && std::ranges::equal(time, shape, [](char c, char expected) {
		return expected == 'd' ? c >= '0' && c <= '9' : c == expected;
	});
	if(!matches) expect::fail(at, "a UTC time such as \"2026-10-22T08:30:00Z\"");
	return time;
}

/// @brief Reads the "don't ask again" grants of a refs file.
/// @param grants The list.
/// @return The grants.
/// @throws DocumentError If one is malformed, or isn't a rule that "don't ask again" grants.
std::vector<Grant> grantsOf(const json& grants) {
	if(!grants.is_array()) expect::fail("grants", "expected a list");
	std::vector<Grant> result;
	for(std::size_t i = 0; i < grants.size(); ++i) {
		const std::string where = "grants[" + std::to_string(i) + "]";
		const json& grant = expect::object(grants[i], where);
		std::string rule = expect::string(grant.at("rule"), where + ".rule");
		expectGrantRule(rule, where + ".rule");
		result.push_back({std::move(rule), expect::string(grant.at("grantedBy"), where + ".grantedBy"),
		                  expect::integer(grant.at("proposal"), where + ".proposal"),
		                  expect::string(grant.at("time"), where + ".time"),
		                  grant.contains("expires") ? expiryOf(grant["expires"], where + ".expires") : ""});
	}
	return result;
}

/// @brief Refuses to write an object that loading would refuse (parseUntrusted()'s limit): the history on disk would
/// be damaged for good. A screen file can nest deeper, since it isn't read with that limit.
/// @param value The object.
/// @param file Its file, for errors.
/// @throws HistoryError If it nests deeper than maxNesting.
void expectReadable(const json& value, const std::filesystem::path& file) {
	if(nestingDepth(value) > maxNesting)
		throw HistoryError(file, "would nest more than " + std::to_string(maxNesting) +
		                             " levels deep, more than a history reads back, so it isn't written");
}

} // namespace

Repository::~Repository() = default;

Repository::OpenedHistory Repository::openHistory(const std::filesystem::path& directory) {
	if(history_) throw HistoryError(directory, "this history already has a folder: " + history_->directory().string());
	if(main_.size() != 1 || !proposals_.empty() || !grants_.empty())
		throw HistoryError(directory, "open the history before the first change");

	OpenedHistory opened;
	auto store = std::make_unique<detail::HistoryStore>(directory);
	if(store->exists()) {
		try {
			opened.loaded = loadHistory(*store);
		} catch(const HistoryError& error) {
			throw DocumentError(std::string(error.what()) + ". A damaged history is never loaded: move " +
			                    directory.string() + " aside to start a new one");
		} catch(const DocumentError& error) {
			// Its files are whole, but the observer can't show its last revision (e.g. a font file it names is gone)
			throw HistoryError(directory,
			                   "its last revision can't be shown (" + detail::reasonOf(error) +
			                       "), so the history isn't loaded: move the folder aside to start a new one");
		}
		// It began with another version of the document: keep it aside, whole, and start again
		if(!opened.loaded && store->writable()) {
			opened.setAside = store->moveAside();
			store = std::make_unique<detail::HistoryStore>(directory);
		}
	}
	opened.memoryOnly = store->problem();
	history_ = std::move(store);
	saveHistory(); // a new history's revision 0
	return opened;
}

bool Repository::loadHistory(detail::HistoryStore& store) {
	Refs refs = readRefs(store);
	std::vector<std::string> heads{refs.main};
	for(const Proposal& proposal : refs.proposals) heads.push_back(proposal.head);
	store.readPacks(refs.packs);
	std::unordered_map<std::string, Commit> objects = readCommits(store, std::move(heads));
	store.forgetUnread();

	// main is the first parents from its head: every commit landed on the one before it
	std::vector<std::string> main;
	for(const Commit* commit = &objects.at(refs.main);; commit = &objects.at(commit->parents.front())) {
		main.push_back(commit->id);
		if(commit->parents.empty()) break;
	}
	std::ranges::reverse(main);
	// A history begun on a version 1 screen began on this one, if the screen file was migrated since
	json original = *objects.at(main.front()).snapshot;
	static_cast<void>(rules_.upgrade(original));
	if(original != mainDocument()) return false;

	for(Proposal& proposal : refs.proposals) {
		completeProposal(proposal, objects, main, store.refsFile());
		const Commit& base = objects.at(proposal.base);
		recheckProposal(proposal, reading(store.objectFile(base.id), [&] { return rebuild(objects, base); }));
	}
	// Hashes catch damage, not a history written by hand: it may not do what no revision can (widen capabilities)
	const Commit& head = objects.at(main.back());
	json upgraded;
	json document = reading(store.objectFile(head.id), [&] {
		json rebuilt = rebuild(objects, head); // checked against the head's tree
		rules_.checkChange(*objects.at(main.front()).snapshot, rebuilt);
		upgraded = rebuilt;
		static_cast<void>(rules_.upgrade(upgraded)); // a version 1 head is read as the current version
		rules_.validate(upgraded);
		return rebuilt;
	});

	// All read and checked: put it in place, and back if the observer can't show it
	std::swap(objects_, objects);
	std::swap(main_, main);
	std::swap(proposals_, refs.proposals);
	std::swap(grants_, refs.grants);
	std::swap(mode_, refs.mode);
	std::swap(nextProposal_, refs.nextProposal);
	std::swap(imported_, refs.imported);
	std::swap(document_, document);
	json state = document_.contains("state") ? document_["state"] : json::object();
	std::swap(state_, state);
	if(upgraded != document_) upgradeHead(std::move(upgraded));
	try {
		documentChanged();
	} catch(const DocumentError&) {
		std::swap(objects_, objects);
		std::swap(main_, main);
		std::swap(proposals_, refs.proposals);
		std::swap(grants_, refs.grants);
		std::swap(mode_, refs.mode);
		std::swap(nextProposal_, refs.nextProposal);
		std::swap(imported_, refs.imported);
		std::swap(state_, state);
		std::swap(document_, document);
		documentChanged();
		throw;
	}
	keptMode_ = mode_;
	return true;
}

/* Reading */

Repository::Refs Repository::readRefs(detail::HistoryStore& store) {
	const std::filesystem::path file = store.refsFile();
	return reading(file, [&] {
		json refs = parseUntrusted(store.readRefs());
		expect::object(refs, "refs");
		const std::string checksum = expect::string(refs.at("checksum"), "checksum");
		refs.erase("checksum");
		if(detail::sha256(refs.dump()) != checksum)
			throw HistoryError(file, "its content doesn't match its checksum: the file is damaged");
		if(const int version = refs.value("version", 0);
		   refs.value("format", std::string()) != historyFormat || version < 1 || version > historyVersion)
			throw HistoryError(file, "not an AffrescoUI history (format \"affrescoui-history\", version 1 or 2)");

		Refs result;
		result.main = commitId(refs.at("main"), "main");
		result.nextProposal = expect::integer(refs.at("nextProposal"), "nextProposal");
		result.imported = changeSetIds(refs.at("imported"));
		if(refs.contains("packs")) result.packs = packNames(refs["packs"]);
		const auto mode = modeFromString(expect::string(refs.at("mode"), "mode"));
		if(!mode) expect::fail("mode", R"(expected "default", "acceptEdits" or "plan")");
		result.mode = *mode;

		result.proposals = readProposals(refs.at("proposals"), result.nextProposal);
		result.grants = grantsOf(refs.at("grants"));
		return result;
	});
}

std::vector<Repository::Proposal> Repository::readProposals(const json& proposals, int nextProposal) {
	if(!proposals.is_array()) expect::fail("proposals", "expected a list");
	std::vector<Proposal> result;
	for(std::size_t i = 0; i < proposals.size(); ++i) {
		const std::string where = "proposals[" + std::to_string(i) + "]";
		Proposal proposal = readProposal(proposals[i], where);
		if(proposal.id < 1 || proposal.id >= nextProposal ||
		   std::ranges::find(result, proposal.id, &Proposal::id) != result.end())
			expect::fail(where + ".id", "expected a proposal id below nextProposal, used once");
		result.push_back(std::move(proposal));
	}
	return result;
}

Repository::Proposal Repository::readProposal(const json& entry, const std::string& where) {
	expect::object(entry, where);
	Proposal proposal;
	proposal.id = expect::integer(entry.at("id"), where + ".id");
	proposal.author = expect::string(entry.at("author"), where + ".author");
	proposal.message = expect::string(entry.at("message"), where + ".message");
	proposal.time = expect::string(entry.at("time"), where + ".time");
	proposal.base = commitId(entry.at("base"), where + ".base");
	proposal.head = commitId(entry.at("head"), where + ".head");
	proposal.touches = strings(entry.at("touches"), where + ".touches");
	proposal.remember = strings(entry.at("remember"), where + ".remember");
	for(const std::string& rule : proposal.remember) expectGrantRule(rule, where + ".remember");
	proposal.reason = expect::string(entry.at("reason"), where + ".reason");
	// Review at scale: optional, so histories written before it still load (and older hosts ignore them)
	if(entry.contains("approvals")) proposal.approvals = strings(entry["approvals"], where + ".approvals");
	if(entry.contains("reviewedHeads")) {
		for(const std::string& head : strings(entry["reviewedHeads"], where + ".reviewedHeads"))
			proposal.reviewedHeads.push_back(commitId(head, where + ".reviewedHeads"));
	}
	if(entry.contains("conflicts")) {
		if(!entry["conflicts"].is_array()) expect::fail(where + ".conflicts", "expected a list");
		proposal.conflicts = entry["conflicts"];
	}
	return proposal;
}

std::shared_ptr<const json>
Repository::readDocument(detail::HistoryStore& store, const std::string& id, const std::filesystem::path& namedIn,
                         std::unordered_map<std::string, std::shared_ptr<const json>>& documents) {
	if(const auto known = documents.find(id); known != documents.end()) return known->second;
	if(!detail::isObjectId(id)) throw HistoryError(namedIn, "'" + id.substr(0, 80) + "' is not a document id");
	const std::filesystem::path file = store.objectFile(id);
	json document = reading(file, [&] { return parseUntrusted(store.readObject(id)); });
	// Its id is the SHA-256 of its JSON, the same hash a commit's id covers
	if(detail::sha256(document.dump()) != id)
		throw HistoryError(file, "its content doesn't match its SHA-256: the file is damaged");
	return documents.emplace(id, std::make_shared<const json>(std::move(document))).first->second;
}

Repository::Commit Repository::readObject(detail::HistoryStore& store, const std::string& id,
                                          const std::filesystem::path& namedIn,
                                          std::unordered_map<std::string, std::shared_ptr<const json>>& documents) {
	if(!detail::isObjectId(id)) throw HistoryError(namedIn, "'" + id.substr(0, 80) + "' is not a commit id");
	const std::filesystem::path file = store.objectFile(id);
	Commit commit;
	bool snapshot = true;
	reading(file, [&] {
		json object = parseUntrusted(store.readObject(id));
		expect::object(object, "commit");
		commit.parents = strings(object.at("parents"), "parents");
		commit.author = expect::string(object.at("author"), "author");
		commit.committer = expect::string(object.at("committer"), "committer");
		commit.time = expect::string(object.at("time"), "time");
		commit.proposal = expect::integer(object.at("proposal"), "proposal");
		commit.message = expect::string(object.at("message"), "message");
		commit.patch = std::move(object.at("patch"));
		commit.tree = commitId(object.at("tree"), "tree");
		// Version 2: a delta, and the document only for a snapshot. Version 1: always the document
		if(const auto delta = object.find("delta"); delta != object.end() && !commit.parents.empty()) {
			if(!delta->is_array()) expect::fail("delta", "expected a list of operations");
			commit.delta = std::move(*delta);
			snapshot = object.value("snapshot", false);
		}
	});
	// The id covers all of it but the delta, the document's id included: a single changed byte gives another id
	if(idOf(commit) != id) throw HistoryError(file, "its content doesn't match its SHA-256: the file is damaged");
	if(snapshot) commit.snapshot = readDocument(store, commit.tree, file, documents);
	commit.id = id;
	commit.number = -1;        // from its parents, once they are all read
	commit.sinceSnapshot = -1; // likewise
	return commit;
}

std::unordered_map<std::string, Repository::Commit> Repository::readCommits(detail::HistoryStore& store,
                                                                            std::vector<std::string> heads) {
	std::unordered_map<std::string, Commit> objects;
	std::unordered_map<std::string, std::shared_ptr<const json>> documents;
	std::vector<std::pair<std::string, std::filesystem::path>> pending; // a commit, and the file that names it
	pending.reserve(heads.size());
	for(std::string& head : heads) pending.emplace_back(std::move(head), store.refsFile());
	while(!pending.empty()) {
		auto [id, namedIn] = std::move(pending.back());
		pending.pop_back();
		if(objects.contains(id)) continue;
		Commit commit = readObject(store, id, namedIn, documents);
		for(const std::string& parent : commit.parents)
			if(!objects.contains(parent)) pending.emplace_back(parent, store.objectFile(id));
		objects.emplace(id, std::move(commit));
	}

	// A commit's number is one more than its first parent's; revision 0 has none. The ids chain them, so no loops.
	for(auto& [id, commit] : objects) {
		std::vector<Commit*> unnumbered;
		Commit* known = &commit;
		while(known->number < 0 && !known->parents.empty()) {
			unnumbered.push_back(known);
			known = &objects.at(known->parents.front());
		}
		known->number = std::max(known->number, 0);
		for(int number = known->number; Commit* c : unnumbered | std::views::reverse) c->number = ++number;
	}
	countFromSnapshots(objects);
	return objects;
}

// How far each is from a snapshot along first parents, which says where the next one goes (revision 0 is always one)
void Repository::countFromSnapshots(std::unordered_map<std::string, Commit>& objects) {
	for(auto& [id, commit] : objects) {
		std::vector<Commit*> counted;
		Commit* known = &commit;
		for(; known->sinceSnapshot < 0 && !known->snapshot; known = &objects.at(known->parents.front()))
			counted.push_back(known);
		if(known->snapshot) known->sinceSnapshot = 0;
		for(int distance = known->sinceSnapshot; Commit* c : counted | std::views::reverse)
			c->sinceSnapshot = ++distance;
	}
}

void Repository::completeProposal(Proposal& proposal, const std::unordered_map<std::string, Commit>& objects,
                                  const std::vector<std::string>& main, const std::filesystem::path& file) {
	const std::string name = "proposal " + std::to_string(proposal.id);
	if(std::ranges::find(main, proposal.base) == main.end())
		throw HistoryError(file, name + ": its base " + proposal.base.substr(0, 7) + " is not on main");

	std::vector<const Commit*> branch;
	for(const Commit* commit = &objects.at(proposal.head); commit->id != proposal.base;
	    commit = &objects.at(commit->parents.front())) {
		if(commit->parents.empty()) throw HistoryError(file, name + ": its branch doesn't start at its base");
		branch.push_back(commit);
	}
	if(branch.empty()) throw HistoryError(file, name + ": its branch has no commits");

	// What accept replays: every commit's operations, oldest first (as extend() built it)
	proposal.commits = branch.size();
	proposal.patch = json::array();
	for(const Commit* commit : branch | std::views::reverse)
		proposal.patch.insert(proposal.patch.end(), commit->patch.begin(), commit->patch.end());
}

void Repository::recheckProposal(Proposal& proposal, const json& base) const {
	const json state = base.contains("state") ? base["state"] : json::object();
	try {
		const Prepared next = prepare(proposal.patch, base, state);
		const Verdict verdict = evaluate(next.before, next.after, proposal.author, true);
		proposal.remember = verdict.kind == Verdict::Kind::Deny
		                        ? std::vector<std::string>()
		                        : rememberedRules(proposal.author, next.touches, verdict.askedByRule);
		proposal.touches = next.touches;
		proposal.behaviour = rules_.behaviourLines(next.before, next.after);
		proposal.policy = reviewNeeds(next.before, next.after, {}).first;
	} catch(const DocumentError&) {
		proposal.remember.clear(); // accept() replays it on main's head, and says if it no longer applies
	}
}

/* Writing */

json Repository::objectOf(const Commit& commit) {
	json object = {{"parents", commit.parents}, {"author", commit.author},     {"committer", commit.committer},
	               {"time", commit.time},       {"proposal", commit.proposal}, {"message", commit.message},
	               {"patch", commit.patch},     {"tree", commit.tree}};
	if(!commit.delta.is_null()) { // a commit loaded from version 1 has none, and its document is written
		object["delta"] = commit.delta;
		object["snapshot"] = commit.snapshot != nullptr;
	}
	return object;
}

void Repository::writeCommit(const Commit& commit) {
	// Like Git's trees, a document is written once; and only for snapshots: other commits are rebuilt from deltas
	if(commit.snapshot && !history_->hasObject(commit.tree)) {
		expectReadable(*commit.snapshot, history_->objectFile(commit.tree));
		history_->writeObject(commit.tree, commit.snapshot->dump());
	}
	const json object = objectOf(commit);
	expectReadable(object, history_->objectFile(commit.id));
	history_->writeObject(commit.id, object.dump());
}

json Repository::refsJson() const {
	json proposals = json::array();
	for(const Proposal& p : proposals_)
		proposals.push_back({{"id", p.id},
		                     {"author", p.author},
		                     {"message", p.message},
		                     {"time", p.time},
		                     {"base", p.base},
		                     {"head", p.head},
		                     {"touches", p.touches},
		                     {"remember", p.remember},
		                     {"reason", p.reason},
		                     {"approvals", p.approvals},
		                     {"reviewedHeads", p.reviewedHeads},
		                     {"conflicts", p.conflicts}});
	json grants = json::array();
	for(const Grant& grant : grants_) grants.push_back(toJson(grant));
	return {{"format", historyFormat},
	        {"version", historyVersion},
	        {"main", main_.back()},
	        {"nextProposal", nextProposal_},
	        {"mode", toString(keptMode_)},
	        {"proposals", proposals},
	        {"grants", grants},
	        {"imported", imported_},
	        {"packs", history_->packs()}};
}

// Parents before children, so every commit on disk has its whole history there too
void Repository::writeCommits() {
	std::vector<std::pair<std::string, bool>> pending{{main_.back(), false}}; // a commit; are its parents written?
	for(const Proposal& proposal : proposals_) pending.emplace_back(proposal.head, false);
	std::unordered_set<std::string> seen;
	while(!pending.empty()) {
		auto [id, parentsWritten] = std::move(pending.back());
		pending.pop_back();
		if(parentsWritten) {
			writeCommit(objects_.at(id));
			continue;
		}
		if(history_->hasObject(id) || !seen.insert(id).second) continue;
		pending.emplace_back(id, true);
		for(const std::string& parent : objects_.at(id).parents) pending.emplace_back(parent, false);
	}
}

void Repository::saveHistory() noexcept {
	if(!history_ || !history_->writable()) return;
	try {
		writeCommits();

		// Enough loose objects: into a pack, which the refs name before the loose files go
		const bool packed = history_->packDue();
		if(packed) history_->pack();

		// Last: the refs only ever name commits and packs that are already on disk
		json refs = refsJson();
		refs["checksum"] = detail::sha256(refs.dump());
		history_->writeRefs(refs.dump(2) + "\n");
		if(packed) history_->removeLoose();
	} catch(const std::exception& error) {
		try {
			affresco::log(LogLevel::Error, std::string(error.what()) +
			                                   " (the change stays in memory; the next change tries to save it again)");
		} catch(...) { // no memory for the message: say it without the details
			affresco::log(LogLevel::Error, "AffrescoUI history: a change could not be saved; it stays in memory");
		}
	}
}

} // namespace affresco::vcs
