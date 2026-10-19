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
#include <unordered_set>
#include <utility>

namespace affresco::vcs {

using detail::HistoryError;

/// @brief What the refs file holds, read and checked.
struct Repository::Refs {
	std::string main;                ///< main's head.
	std::vector<Proposal> proposals; ///< The open proposals, oldest first, without their commits and patch.
	std::vector<Grant> grants;       ///< "Don't ask again" rules.
	Mode mode = Mode::Default;       ///< How much reviewers are asked.
	int nextProposal = 1;            ///< The next proposal's id.
};

namespace {

/// The refs file's "format".
constexpr const char* historyFormat = "affrescoui-history";

/// @brief Runs a reader of one history file, so that every error it throws names the file.
/// @tparam Read A function with no parameters.
/// @param file The file.
/// @param read The reader.
/// @return What it returns.
/// @throws HistoryError For any malformed content.
template <typename Read> decltype(auto) reading(const std::filesystem::path& file, Read&& read) {
	try {
		return read();
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
	if(!detail::isCommitId(id)) expect::fail(where, "expected a commit id (64 hex digits)");
	return id;
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
	std::unordered_map<std::string, Commit> objects = readCommits(store, std::move(heads));

	// main is the first parents from its head: every commit landed on the one before it
	std::vector<std::string> main;
	for(const Commit* commit = &objects.at(refs.main);; commit = &objects.at(commit->parents.front())) {
		main.push_back(commit->id);
		if(commit->parents.empty()) break;
	}
	std::ranges::reverse(main);
	if(objects.at(main.front()).document != mainHead().document) return false;

	for(Proposal& proposal : refs.proposals) completeProposal(proposal, objects, main, store.refsFile());
	// Hashes catch damage, not a history written by hand: it may not do what no revision can (widen capabilities)
	const Commit& head = objects.at(main.back());
	reading(store.objectFile(head.id), [&] {
		rules_.checkChange(objects.at(main.front()).document, head.document);
		rules_.validate(head.document);
	});

	// All read and checked: put it in place, and back if the observer can't show it
	std::swap(objects_, objects);
	std::swap(main_, main);
	std::swap(proposals_, refs.proposals);
	std::swap(grants_, refs.grants);
	std::swap(mode_, refs.mode);
	std::swap(nextProposal_, refs.nextProposal);
	document_ = mainHead().document;
	json state = document_.contains("state") ? document_["state"] : json::object();
	std::swap(state_, state);
	try {
		documentChanged();
	} catch(const DocumentError&) {
		std::swap(objects_, objects);
		std::swap(main_, main);
		std::swap(proposals_, refs.proposals);
		std::swap(grants_, refs.grants);
		std::swap(mode_, refs.mode);
		std::swap(nextProposal_, refs.nextProposal);
		std::swap(state_, state);
		document_ = mainHead().document;
		documentChanged();
		throw;
	}
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
		if(refs.value("format", std::string()) != historyFormat || refs.value("version", 0) != 1)
			throw HistoryError(file, "not an AffrescoUI history (format \"affrescoui-history\", version 1)");

		Refs result;
		result.main = commitId(refs.at("main"), "main");
		result.nextProposal = expect::integer(refs.at("nextProposal"), "nextProposal");
		const auto mode = modeFromString(expect::string(refs.at("mode"), "mode"));
		if(!mode) expect::fail("mode", "expected \"default\", \"acceptEdits\" or \"plan\"");
		result.mode = *mode;

		const json& proposals = refs.at("proposals");
		if(!proposals.is_array()) expect::fail("proposals", "expected a list");
		for(std::size_t i = 0; i < proposals.size(); ++i) {
			const std::string where = "proposals[" + std::to_string(i) + "]";
			Proposal proposal = readProposal(proposals[i], where);
			if(proposal.id < 1 || proposal.id >= result.nextProposal ||
			   std::ranges::find(result.proposals, proposal.id, &Proposal::id) != result.proposals.end())
				expect::fail(where + ".id", "expected a proposal id below nextProposal, used once");
			result.proposals.push_back(std::move(proposal));
		}

		const json& grants = refs.at("grants");
		if(!grants.is_array()) expect::fail("grants", "expected a list");
		for(std::size_t i = 0; i < grants.size(); ++i) {
			const std::string where = "grants[" + std::to_string(i) + "]";
			const json& grant = expect::object(grants[i], where);
			std::string rule = expect::string(grant.at("rule"), where + ".rule");
			expectGrantRule(rule, where + ".rule");
			result.grants.push_back({std::move(rule), expect::string(grant.at("grantedBy"), where + ".grantedBy"),
			                         expect::integer(grant.at("proposal"), where + ".proposal"),
			                         expect::string(grant.at("time"), where + ".time")});
		}
		return result;
	});
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
	return proposal;
}

Repository::Commit Repository::readObject(detail::HistoryStore& store, const std::string& id,
                                          const std::filesystem::path& namedIn) {
	if(!detail::isCommitId(id)) throw HistoryError(namedIn, "'" + id.substr(0, 80) + "' is not a commit id");
	const std::filesystem::path file = store.objectFile(id);
	return reading(file, [&] {
		const json object = parseUntrusted(store.readObject(id));
		expect::object(object, "commit");
		Commit commit;
		commit.parents = strings(object.at("parents"), "parents");
		commit.author = expect::string(object.at("author"), "author");
		commit.committer = expect::string(object.at("committer"), "committer");
		commit.time = expect::string(object.at("time"), "time");
		commit.proposal = expect::integer(object.at("proposal"), "proposal");
		commit.message = expect::string(object.at("message"), "message");
		commit.patch = object.at("patch");
		commit.document = object.at("document");
		commit.number = -1; // from its parents, once they are all read
		// The id covers all of it: a single changed byte gives another id
		if(idOf(commit) != id) throw HistoryError(file, "its content doesn't match its SHA-256: the file is damaged");
		commit.id = id;
		return commit;
	});
}

std::unordered_map<std::string, Repository::Commit> Repository::readCommits(detail::HistoryStore& store,
                                                                            std::vector<std::string> heads) {
	std::unordered_map<std::string, Commit> objects;
	std::vector<std::pair<std::string, std::filesystem::path>> pending; // a commit, and the file that names it
	for(std::string& head : heads) pending.emplace_back(std::move(head), store.refsFile());
	while(!pending.empty()) {
		auto [id, namedIn] = std::move(pending.back());
		pending.pop_back();
		if(objects.contains(id)) continue;
		Commit commit = readObject(store, id, namedIn);
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
		if(known->number < 0) known->number = 0;
		for(int number = known->number; Commit* c : unnumbered | std::views::reverse) c->number = ++number;
	}
	return objects;
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

/* Writing */

json Repository::objectOf(const Commit& commit) {
	return {{"parents", commit.parents}, {"author", commit.author},     {"committer", commit.committer},
	        {"time", commit.time},       {"proposal", commit.proposal}, {"message", commit.message},
	        {"patch", commit.patch},     {"document", commit.document}};
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
		                     {"reason", p.reason}});
	json grants = json::array();
	for(const Grant& grant : grants_) grants.push_back(toJson(grant));
	return {
	    {"format", historyFormat}, {"version", 1},           {"main", main_.back()}, {"nextProposal", nextProposal_},
	    {"mode", toString(mode_)}, {"proposals", proposals}, {"grants", grants}};
}

void Repository::saveHistory() noexcept {
	if(!history_ || !history_->writable()) return;
	try {
		// Parents before children, so every commit on disk has its whole history there too
		std::vector<std::pair<std::string, bool>> pending{{main_.back(), false}}; // a commit; are its parents written?
		for(const Proposal& proposal : proposals_) pending.emplace_back(proposal.head, false);
		std::unordered_set<std::string> seen;
		while(!pending.empty()) {
			auto [id, parentsWritten] = std::move(pending.back());
			pending.pop_back();
			if(parentsWritten) {
				history_->writeObject(id, objectOf(objects_.at(id)).dump());
				continue;
			}
			if(history_->hasObject(id) || !seen.insert(id).second) continue;
			pending.emplace_back(id, true);
			for(const std::string& parent : objects_.at(id).parents) pending.emplace_back(parent, false);
		}

		// Last: the refs only ever name commits that are already on disk
		json refs = refsJson();
		refs["checksum"] = detail::sha256(refs.dump());
		history_->writeRefs(refs.dump(2) + "\n");
	} catch(const std::exception& error) {
		affresco::log(LogLevel::Error, std::string(error.what()) +
		                                   " (the change stays in memory; the next change tries to save it again)");
	}
}

} // namespace affresco::vcs
