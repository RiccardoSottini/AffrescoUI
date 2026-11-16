/// @file
/// @brief vcs::Repository's undo of an earlier commit that keeps the later changes (VERSION_CONTROL.md, "Undo that
/// keeps what came later"): a three-way merge by node id of the commit's document (the base), main's head and the
/// commit's first parent's. What no later commit changed is undone; the rest is a conflict, which the person chooses
/// for: drop the later change with the commit, or keep it. The plan is worked out before anything changes; the
/// revision that does it carries an undo mark, which commit() and submit() check against the undo worked out again.

#include "affresco/vcs/Repository.hpp"

#include "history/HistoryStore.hpp"
#include "permissions/Permissions.hpp"
#include "review/Merge.hpp"
#include "revisions/Deltas.hpp"
#include "revisions/Trail.hpp"

#include <algorithm>
#include <map>
#include <ranges>
#include <set>
#include <utility>

namespace affresco::vcs {

using expect::fail;

namespace {

/// Largest value, as JSON, that a plan shows as it is; a larger one shows as `{"cut": true, "bytes": n}`.
constexpr std::size_t maxShownBytes = 16U * 1024U;

/// @brief A value as a plan shows it: as it is, or cut when it is large.
/// @param value The value (null where there is none).
/// @return It, or `{"cut": true, "bytes": n}`.
json shown(const json& value) {
	if(value.is_null()) return value;
	const std::size_t bytes = value.dump().size();
	if(bytes <= maxShownBytes) return value;
	return {{"cut", true}, {"bytes", bytes}};
}

/// @brief Revision numbers for a message: "revision 6", "revision 6 and 7", "revision 5, 6 and 7".
/// @param numbers The numbers, oldest first; not empty.
/// @return The words.
std::string revisionsText(const std::vector<int>& numbers) {
	std::string text = "revision ";
	for(std::size_t i = 0; i < numbers.size(); ++i) {
		if(i > 0) text += i + 1U == numbers.size() ? " and " : ", ";
		text += std::to_string(numbers[i]);
	}
	return text;
}

/// @brief Reads a choice: a list of paths, or "all".
/// @param value The choice.
/// @return true if it is one.
bool isChoice(const json& value) {
	if(value.is_null() || value == "all") return true;
	return value.is_array() && std::ranges::all_of(value, [](const json& path) { return path.is_string(); });
}

/// @brief Tells whether a choice names a path.
/// @param choice A list of paths, "all" or null.
/// @param path The path.
/// @return true if the list holds it (not for "all").
bool names(const json& choice, const std::string& path) {
	return choice.is_array() && std::ranges::find(choice, json(path)) != choice.end();
}

/// @brief Finds a model the commit changed that a later commit changed too: its records follow its fields and its
/// migrations, so it is never undone part of the way.
/// @param then The commit's document.
/// @param now main's head's.
/// @param before The commit's first parent's.
/// @return The model's name, or "" if there is none.
std::string changedModel(const json& then, const json& now, const json& before) {
	const auto modelOf = [](const json& document, const std::string& name) {
		const auto models = document.find("models");
		if(models == document.end() || !models->is_object() || !models->contains(name)) return json();
		return (*models)[name];
	};
	std::set<std::string> names;
	for(const json* side : {&then, &before})
		if(const auto models = side->find("models"); models != side->end() && models->is_object())
			for(const auto& [name, model] : models->items()) names.insert(name);
	for(const std::string& name : names) {
		const json was = modelOf(then, name), is = modelOf(now, name), had = modelOf(before, name);
		if(was == had || is.is_null()) continue; // the commit left it alone, or a later change removed it
		if(!detail::merge(was, is, had).clean()) return name;
	}
	return {};
}

} // namespace

/// What working out an undo gives: the plan, and what the revision needs.
struct Repository::UndoWork {
	const Commit* target = nullptr;  ///< The commit to undo.
	UndoPlan plan;                   ///< The plan, as the person sees it.
	json conflicts = json::array();  ///< Every conflict: `{"path", "reason", "then", "now", "before", "commits", "choice"}`
	json dropped = json::array();    ///< Paths of the conflicts dropped.
	json document;                   ///< The document after the undo, before "then".
	json patch = json::array();      ///< The undo's operations: the document's, the live state's, then "then".
	std::size_t then = 0;            ///< How many operations of the patch are "then".
};

UndoPlan Repository::undoPlan(const json& revision, std::string_view author, const UndoOptions& options) const {
	return workOutUndo(revision, author, options, true).plan;
}

json Repository::undoRevision(const json& revision, std::string_view author, const UndoOptions& options) const {
	// Choices made on another head are for changes nobody saw since
	if(const std::string& head = mainHead().id;
	   !options.head.empty() && (options.head.size() < 7U || !head.starts_with(options.head)))
		throw DocumentError("revert: main moved since you looked (it is now at " + head.substr(0, 7) + "): look again");
	UndoWork work = workOutUndo(revision, author, options, false);
	UndoPlan& plan = work.plan;
	const Commit& target = *work.target;
	if(plan.outcome == "blocked") throw DocumentError(plan.blocked);
	if(plan.outcome == "choose") {
		std::string listed;
		std::size_t open = 0;
		for(const json& conflict : work.conflicts) {
			if(!conflict["choice"].is_null()) continue;
			if(++open > 4U) continue;
			const std::string path = conflict["path"].get<std::string>();
			const std::vector<int> numbers = laterNumbers(work, path);
			listed += (open > 1U ? ", " : "") + path + (numbers.empty() ? "" : " (" + revisionsText(numbers) + ")");
		}
		if(open > 4U) listed += " and " + std::to_string(open - 4U) + " more";
		throw DocumentError("revert: conflict: " + std::to_string(open) + " later change" + (open == 1U ? "" : "s") +
		                    (open == 1U ? " was" : " were") + " built on " + describe(target) + ": " + listed +
		                    R"(: read the plan (undoPlan), then revert with "drop" or "keep" for each)");
	}
	std::string message = options.message;
	if(message.empty()) {
		message = "Revert \"" + target.message + "\"";
		if(!work.dropped.empty()) {
			std::set<int> numbers;
			for(const json& path : work.dropped)
				for(int number : laterNumbers(work, path.get<std::string>())) numbers.insert(number);
			message += numbers.empty() ? " (with what later changes built on it)"
			                           : " (with what " + revisionsText({numbers.begin(), numbers.end()}) +
			                                 " built on it)";
		}
	}
	return {{"author", std::string(author)},
	        {"message", std::move(message)},
	        {"parent", plan.head},
	        {"patch", std::move(work.patch)},
	        {"undo", {{"of", target.id}, {"drop", std::move(work.dropped)}, {"then", work.then}}}};
}

json Repository::revertRevision(const json& revision, std::string_view author) const {
	return undoRevision(revision, author, {});
}

Repository::UndoWork Repository::workOutUndo(const json& revision, std::string_view author, const UndoOptions& options,
                                             bool judged) const {
	UndoWork work;
	work.target = &find(revision); // an unknown commit throws
	const Commit& target = *work.target;
	UndoPlan& plan = work.plan;
	plan.of = info(target);
	plan.head = mainHead().id;
	const auto blocked = [&](const std::string& why) {
		plan.outcome = "blocked";
		plan.blocked = "revert: " + why;
		plan.changes.clear();
		plan.impact = nullptr;
		return std::move(work);
	};

	if(std::ranges::find(main_, target.id) == main_.end()) return blocked(describe(target) + " is not on main");
	if(target.parents.empty()) return blocked("revision 0 is the screen as loaded: there is nothing to revert");
	if(!isChoice(options.drop)) return blocked(R"(drop: expected a list of paths or "all")");
	if(!isChoice(options.keep)) return blocked(R"(keep: expected a list of paths or "all")");
	if(!options.then.is_null() && !options.then.is_array()) return blocked("then: expected a JSON Patch array");
	if(options.drop == "all" && options.keep == "all") return blocked(R"("drop" and "keep" can't both be "all")");
	if(options.drop.is_array())
		for(const json& path : options.drop)
			if(names(options.keep, path.get<std::string>()))
				return blocked("'" + path.get<std::string>() + "' is both dropped and kept");

	// The base is the commit's document: main changed it since (the later commits), the undo takes it back
	const json then = documentOf(target);
	const json before = documentOf(objects_.at(target.parents.front()));
	if(const std::string model = changedModel(then, mainDocument(), before); !model.empty()) {
		const int number = newestChange("/models/" + detail::escapeKey(model), target);
		return blocked((number > 0 ? "revision " + std::to_string(number) : std::string("a later revision")) +
		               " changed the model '" + model + "' after it: undo that first");
	}
	const MergeResult merged = detail::merge(then, mainDocument(), before);
	sortConflicts(work, merged.conflicts);
	traceLater(work);
	for(const json* choice : {&options.drop, &options.keep})
		if(choice->is_array())
			for(const json& path : *choice)
				if(std::ranges::none_of(work.conflicts, [&](const json& c) { return c["path"] == path; }))
					return blocked("'" + path.get<std::string>() + "' is not a conflict of this undo");
	chooseConflicts(work, options);
	if(plan.outcome == "choose") return work;

	// Again, the dropped conflicts taking the value from before the commit
	std::set<std::string> dropped;
	for(const json& path : work.dropped) dropped.insert(path.get<std::string>());
	MergeOptions settle;
	settle.takeProposal = [&dropped](const std::string& path) { return dropped.contains(path); };
	work.document = dropped.empty() ? merged.merged : detail::merge(then, mainDocument(), before, settle).merged;
	work.patch = detail::delta(mainDocument(), work.document);
	undoState(work);
	if(options.then.is_array()) {
		work.then = options.then.size();
		work.patch.insert(work.patch.end(), options.then.begin(), options.then.end());
	}
	if(work.patch.empty()) {
		if(target.id == main_.back())
			return blocked("nothing to revert: " + describe(target) +
			               " changed neither the document nor, in this run, the live state");
		return blocked("nothing to undo: later changes already replaced or removed everything revision " +
		               std::to_string(target.number) + " did");
	}
	if(judged) judgeUndo(work, author);
	return work;
}

int Repository::newestChange(const std::string& pointer, const Commit& target) const {
	for(std::size_t n = main_.size() - 1U; n > static_cast<std::size_t>(target.number); --n) {
		const Commit& commit = objects_.at(main_[n]);
		const json worked = commit.delta.is_null() ? deltaOf(commit) : json(); // only for a history without deltas
		for(const json& op : commit.delta.is_null() ? worked : commit.delta) {
			const std::string path = op.value("path", std::string());
			if(path == pointer || path.starts_with(pointer + "/") || pointer.starts_with(path + "/") || path.empty())
				return commit.number;
		}
	}
	return 0;
}

void Repository::sortConflicts(UndoWork& work, const json& conflicts) const {
	for(const json& conflict : conflicts) {
		const std::string path = conflict["path"].get<std::string>();
		const std::string& reason = conflict["reason"].get_ref<const std::string&>();
		// main is the later changes, the proposal the document before the commit
		if(reason == "removed on main, changed in the proposal") {
			work.plan.gone.push_back(path);
			continue;
		}
		const char* why = reason.starts_with("reordered")              ? "reordered since"
		                  : reason == "changed on main, removed in the proposal" ? "changed since, and the undo takes it away"
		                                                                          : "changed since";
		work.conflicts.push_back({{"path", path},
		                          {"reason", why},
		                          {"then", conflict["base"]},
		                          {"now", conflict["main"]},
		                          {"before", conflict["proposal"]},
		                          {"commits", json::array()},
		                          {"choice", nullptr}});
	}
}

// Back from main's head to the commit, one delta at a time, a trail for each conflict
void Repository::traceLater(UndoWork& work) const {
	const Commit& target = *work.target;
	const std::size_t later = main_.size() - 1U - static_cast<std::size_t>(target.number);
	work.plan.untouched = later;
	if(work.conflicts.empty()) return;

	const json& head = mainDocument();
	std::vector<std::pair<std::size_t, detail::Trail>> trails; // a conflict, and its trail
	for(std::size_t i = 0; i < work.conflicts.size() && i < maxUndoListed; ++i) {
		try {
			const std::string pointer = resolvePath(head, work.conflicts[i]["path"].get<std::string>(), "undo");
			if(detail::locate(head, pointer)) trails.emplace_back(i, detail::Trail(head, pointer));
		} catch(const DocumentError&) {
			continue; // a node main no longer has: nothing to follow
		}
	}
	std::map<int, json> parts; // later commits that built on a conflict, by number, and the paths
	for(std::size_t n = main_.size() - 1U, walked = 0; n > static_cast<std::size_t>(target.number) && !trails.empty() &&
	                                                   walked < maxUndoWalk;
	    --n, ++walked) {
		const Commit& commit = objects_.at(main_[n]);
		const json worked = commit.delta.is_null() ? deltaOf(commit) : json(); // only for a history without deltas
		const json& delta = commit.delta.is_null() ? worked : commit.delta;
		std::erase_if(trails, [&](std::pair<std::size_t, detail::Trail>& traced) {
			const detail::TrailStep step = traced.second.stepBack(delta);
			if(step == detail::TrailStep::Untouched) return false;
			json& conflict = work.conflicts[traced.first];
			conflict["commits"].insert(conflict["commits"].begin(), commit.number);
			json& listed = parts[commit.number];
			if(listed.is_null()) listed = json::array();
			if(std::ranges::find(listed, conflict["path"]) == listed.end()) listed.push_back(conflict["path"]);
			return step == detail::TrailStep::Replaced;
		});
	}
	for(auto& [number, paths] : parts) {
		if(work.plan.later.size() >= maxUndoListed) {
			++work.plan.laterMore;
			continue;
		}
		const Commit& commit = objects_.at(main_[static_cast<std::size_t>(number)]);
		work.plan.later.push_back({{"number", number},
		                           {"id", commit.id},
		                           {"short", commit.id.substr(0, 7)},
		                           {"author", commit.author},
		                           {"message", commit.message},
		                           {"parts", std::move(paths)}});
	}
	work.plan.untouched = later - parts.size();
}

void Repository::chooseConflicts(UndoWork& work, const UndoOptions& options) const {
	UndoPlan& plan = work.plan;
	bool open = false;
	for(json& conflict : work.conflicts) {
		const std::string path = conflict["path"].get<std::string>();
		if(names(options.drop, path) || (options.drop == "all" && !names(options.keep, path))) conflict["choice"] = "drop";
		else if(names(options.keep, path) || options.keep == "all") conflict["choice"] = "keep";
		if(conflict["choice"] == "drop") work.dropped.push_back(path);
		open = open || conflict["choice"].is_null();
		if(plan.conflicts.size() >= maxUndoListed) {
			++plan.conflictsMore;
			continue;
		}
		json listed = conflict;
		for(const char* key : {"then", "now", "before"}) listed[key] = shown(conflict[key]);
		plan.conflicts.push_back(std::move(listed));
	}
	plan.outcome = work.conflicts.empty() ? "clean" : open ? "choose" : "ready";
}

void Repository::undoState(UndoWork& work) const {
	const Commit& target = *work.target;
	if(target.stateUndo.is_null() || target.stateUndo.empty()) return; // it changed none, or none in this run
	const bool unchanged = std::ranges::all_of(target.stateAfter, [&](const json& part) {
		const std::string& path = part["path"].get_ref<const std::string&>();
		const json* now = detail::locate(committedState(), path.substr(std::string_view("/state").size()));
		return part.contains("value") ? now && *now == part["value"] : now == nullptr;
	});
	json& listed = work.plan.state[unchanged ? "undone" : "kept"];
	for(const json& part : target.stateAfter) listed.push_back(part["path"]);
	if(unchanged) work.patch.insert(work.patch.end(), target.stateUndo.begin(), target.stateUndo.end());
}

void Repository::judgeUndo(UndoWork& work, std::string_view author) const {
	UndoPlan& plan = work.plan;
	const Commit& target = *work.target;
	// The undo of the person's own change to a program is judged as that change was
	const bool own = target.ownInput && detail::channelOf(author) == personChannel &&
	                 std::ranges::all_of(work.patch, [](const json& op) {
		                 return op.is_object() && !op.contains("from") &&
		                        op.value("path", std::string()).starts_with("/program/");
	                 });
	const bool wasOwn = std::exchange(ownInput_, own);
	try {
		const Prepared next = prepare(work.patch, mainDocument(), committedState());
		ownInput_ = wasOwn;
		const Verdict verdict = evaluate(next.before, next.after, std::string(author), true);
		if(verdict.kind == Verdict::Kind::Deny) {
			plan.outcome = "blocked";
			plan.blocked = "revert: " + verdict.reason;
			return;
		}
		plan.changes = changeLines(next.patch);
		plan.impact = rules_.impact(next.before, next.after);
		plan.warnings = rules_.brokenLines(next.before, next.after);
		for(const Proposal& open : proposals_)
			if(!detail::merge(documentOf(objects_.at(open.base)), next.document, documentOf(objects_.at(open.head)))
			        .clean())
				plan.proposals.push_back(open.id);
	} catch(const DocumentError& error) {
		ownInput_ = wasOwn;
		plan.outcome = "blocked";
		plan.blocked = std::string("revert: ") + error.what();
		return;
	}
	if(target.run.is_array() && !target.run.empty())
		plan.warnings.push_back("it ran " + std::to_string(target.run.size()) + " step" +
		                        (target.run.size() == 1U ? "" : "s") + " once when it landed: those are not undone");
}

std::vector<int> Repository::laterNumbers(const UndoWork& work, const std::string& path) {
	const auto conflict =
	    std::ranges::find_if(work.conflicts, [&](const json& listed) { return listed["path"] == path; });
	if(conflict == work.conflicts.end()) return {};
	return (*conflict)["commits"].get<std::vector<int>>();
}

/* Checking a revision that says it is an undo */

void Repository::checkUndoMark(const json& mark, const std::string& where) {
	if(!mark.is_object() || mark.size() != 3U || !mark.contains("of") || !mark.contains("drop") ||
	   !mark.contains("then"))
		fail(where, R"(expected {"of", "drop", "then"})");
	if(!mark["of"].is_string() || !detail::isObjectId(mark["of"].get_ref<const std::string&>()))
		fail(where + ".of", "expected a commit id");
	if(!mark["drop"].is_array() || mark["drop"].size() > 256U ||
	   !std::ranges::all_of(mark["drop"], [](const json& path) { return path.is_string(); }))
		fail(where + ".drop", "expected a list of at most 256 paths");
	if(!mark["then"].is_number_unsigned()) fail(where + ".then", "expected a whole number");
}

bool Repository::undoNeedsReview(const json& mark) {
	if(!mark.is_object()) return false;
	const auto drop = mark.find("drop");
	const auto then = mark.find("then");
	return (drop != mark.end() && drop->is_array() && !drop->empty()) ||
	       (then != mark.end() && then->is_number_unsigned() && then->get<std::size_t>() > 0U);
}

void Repository::checkUndo(const json& revision, const std::string& author) const {
	for(const auto& [key, value] : revision.items())
		if(key != "author" && key != "message" && key != "parent" && key != "patch" && key != "undo")
			fail("revision.undo", "an undo carries only author, message, parent, patch and undo, not '" + key + "'");
	const json& mark = revision["undo"];
	checkUndoMark(mark, "revision.undo");
	if(!revision.contains("parent")) fail("revision.undo", "an undo names main's head as its parent");
	const auto patch = revision.find("patch");
	if(patch == revision.end() || !patch->is_array()) fail("revision.patch", "expected a JSON Patch array");
	const auto thenCount = mark["then"].get<std::size_t>();
	const std::string of = mark["of"].get<std::string>();
	if(thenCount > patch->size()) fail("revision.undo.then", "more operations than the patch has");
	if(!objects_.contains(of)) fail("revision.undo.of", "no commit " + of.substr(0, 7));

	UndoOptions options;
	options.drop = mark["drop"];
	options.keep = "all";
	for(std::size_t i = patch->size() - thenCount; i < patch->size(); ++i) options.then.push_back((*patch)[i]);
	const UndoWork work = workOutUndo(json(of), author, options, false);
	const bool same = (work.plan.outcome == "clean" || work.plan.outcome == "ready") && work.patch == *patch &&
	                  work.dropped == mark["drop"];
	if(!same) fail("revision.undo", "this is not the undo of " + of.substr(0, 7) + " as main is now");
}

/* Proposals that undo */

void Repository::describeUndoProposal(Proposal& proposal) const {
	if(!undoNeedsReview(proposal.undo)) return;
	proposal.remember.clear(); // what it takes out is never remembered: each one is the person's choice
	if(!proposal.conflicts.empty()) return;
	const json& drop = proposal.undo["drop"];
	if(drop.empty()) {
		proposal.reason = "it rebuilds a later change";
		return;
	}
	std::string paths;
	for(std::size_t i = 0; i < drop.size() && i < 4U; ++i) paths += (i ? ", " : "") + drop[i].get<std::string>();
	if(drop.size() > 4U) paths += " and " + std::to_string(drop.size() - 4U) + " more";
	proposal.reason = "it takes out what later changes built: " + paths;
}

} // namespace affresco::vcs
