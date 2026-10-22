/// @file
/// @brief vcs::Repository's documents: rebuilding a commit's document from the nearest snapshot and the deltas after
/// it, blame, which follows a node or a path back through the deltas to the commit that last changed it, and the
/// revisions that restore or revert a commit.

#include "affresco/vcs/Repository.hpp"

#include "revisions/Deltas.hpp"
#include "permissions/Permissions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <ranges>

namespace affresco::vcs {

using expect::fail;

namespace {

/// @brief Tells whether one path is the start of another (or the same path).
/// @param prefix The shorter path's segments.
/// @param path The other's.
/// @return true if every segment of @p prefix begins @p path.
bool startsWith(const std::vector<std::string>& path, const std::vector<std::string>& prefix) {
	return prefix.size() <= path.size() && std::equal(prefix.begin(), prefix.end(), path.begin());
}

/// @brief Reads a list position.
/// @param segment A path segment.
/// @return It as a number, or -1 if it isn't one.
long long positionOf(const std::string& segment) {
	if(segment.empty() || segment.size() > 18 ||
	   !std::ranges::all_of(segment, [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
		return -1;
	return std::stoll(segment);
}

/// @brief Where a node or a value is, followed back through the commits on main.
///
/// It starts at a path in main's head and moves back one commit at a time, through each delta's operations in
/// reverse: an operation that adds, removes or replaces it, or something inside it or around it (one of the objects
/// or lists that hold it), changed it. Adding or removing an item before it in a list only moves it. The containers
/// along the path keep their kind (list or object) until something replaces one, which changes what is inside it.
class Trail {
public:
	/// @brief Starts at a path in main's head.
	/// @param head main's head's document.
	/// @param pointer The path; something is there.
	Trail(const json& head, std::string_view pointer) : path_(detail::segmentsOf(pointer)) {
		const json* value = &head;
		for(const std::string& segment : path_) {
			lists_.push_back(value->is_array());
			value = value->is_array() ? &(*value)[static_cast<std::size_t>(positionOf(segment))] : &(*value)[segment];
		}
	}

	/// @brief Moves back through one commit.
	/// @param delta The commit's delta: how its document differs from its first parent's.
	/// @return true if the commit changed what the trail follows; then the trail stops there.
	bool changedBy(const json& delta) {
		for(auto op = delta.rbegin(); op != delta.rend(); ++op)
			if(changedBy(op->value("op", ""), detail::segmentsOf(op->value("path", "")))) return true;
		return false;
	}

private:
	/// @brief Moves back through one operation.
	/// @param kind "add", "remove" or "replace" (the only ones detail::delta() writes).
	/// @param at Its path.
	/// @return true if it changed what the trail follows.
	bool changedBy(const std::string& kind, const std::vector<std::string>& at) {
		if(startsWith(at, path_)) return true;                     // inside it
		if(kind != "remove" && startsWith(path_, at)) return true; // it, or something holding it, was added or replaced
		if(kind == "replace") return false;
		if(kind != "add" && kind != "remove") return true; // an operation deltas don't hold: assume the worst
		moveBack(kind == "add", at);
		return false;
	}

	/// @brief Follows an item added or removed in a list that holds it: that moves it one place.
	/// @param added Whether the item was added (else removed).
	/// @param at The item's path.
	void moveBack(bool added, const std::vector<std::string>& at) {
		if(at.empty() || at.size() > path_.size() || !lists_[at.size() - 1] ||
		   !std::equal(at.begin(), std::prev(at.end()), path_.begin()))
			return;
		std::string& segment = path_[at.size() - 1];
		const long long changed = positionOf(at.back()), position = positionOf(segment);
		if(changed < 0 || position < 0) return;
		if(added && position > changed) segment = std::to_string(position - 1);
		if(!added && position >= changed) segment = std::to_string(position + 1);
	}

	std::vector<std::string> path_; ///< Where it is, in the document of the commit reached.
	std::vector<bool> lists_;       ///< For each segment: whether what holds it is a list.
};

/// @brief Tells whether an operation of a revision's patch reaches a path of the live state.
/// @param op The operation, as submitted.
/// @param target The path's segments ("state", ...).
/// @return true if its path or its "from" is the path, inside it, or holds it.
bool reaches(const json& op, const std::vector<std::string>& target) {
	return std::ranges::any_of(std::array{"path", "from"}, [&](const char* key) {
		const auto at = op.find(key);
		if(at == op.end() || !at->is_string() || !at->get_ref<const std::string&>().starts_with("/state")) return false;
		const std::vector<std::string> segments = detail::segmentsOf(at->get_ref<const std::string&>());
		return startsWith(segments, target) || startsWith(target, segments);
	});
}

/// @brief Tells whether a revision's patch can change the live state: a path or a "from" at "/state" or inside it, or
/// the whole document ("").
/// @param patch The patch, as stored.
/// @return true if it can.
bool touchesState(const json& patch) {
	return std::ranges::any_of(patch, [](const json& op) {
		return std::ranges::any_of(std::array{"path", "from"}, [&](const char* key) {
			const auto at = op.find(key);
			if(at == op.end() || !at->is_string()) return false;
			const std::string& path = at->get_ref<const std::string&>();
			return path.empty() || path == "/state" || path.starts_with("/state/");
		});
	});
}

/// @brief The part of the live state an operation of a delta changes: its path, or the list that holds it, since
/// adding or removing an item moves the ones after it.
/// @param state The live state after the delta.
/// @param path The operation's path, in the live state.
/// @return The part's path.
std::string partOf(const json& state, const std::string& path) {
	const std::string holder = path.substr(0, path.rfind('/'));
	const json* list = path.empty() ? nullptr : detail::locate(state, holder);
	return list && list->is_array() ? holder : path;
}

/// @brief The part of a document a reverted operation must find as its commit left it: its path, or the list that
/// holds it, since adding or removing an item moves the ones after it.
/// @param then The document as the commit left it.
/// @param op The operation of the revert.
/// @return The part's JSON Pointer.
std::string revertedPart(const json& then, const json& op) {
	const json::json_pointer path(op["path"].get<std::string>());
	const json* container = detail::locate(then, path.parent_pointer().to_string());
	if(!path.empty() && (path.back() == "-" || (container && container->is_array())))
		return path.parent_pointer().to_string();
	return path.to_string();
}

/// @brief Tells whether a part of a document is as a commit left it, inside the same nodes (each node around it
/// keeps its id).
/// @param head The document now.
/// @param then The document as the commit left it.
/// @param part The part's JSON Pointer.
/// @return true if it is.
bool unchangedSince(const json& head, const json& then, const std::string& part) {
	const auto same = [&](const std::string& pointer) {
		const json *now = detail::locate(head, pointer), *before = detail::locate(then, pointer);
		return now && before ? *now == *before : now == before;
	};
	bool unchanged = same(part);
	for(json::json_pointer prefix(part); unchanged && !prefix.empty(); prefix = prefix.parent_pointer()) {
		const json* node = detail::locate(then, prefix.to_string());
		if(node && node->is_object() && node->contains("id")) unchanged = same(prefix.to_string() + "/id");
	}
	return unchanged;
}

} // namespace

// Only revisions that reach the live state can change it: what actions change between commits isn't a commit's
void Repository::recordStateChange(Commit& commit, const json& before, const json& after) {
	if(!touchesState(commit.patch)) return;
	const json change = detail::delta(before, after);
	if(change.empty()) return;
	commit.stateUndo = json::array();
	for(json op : detail::delta(after, before)) {
		op["path"] = "/state" + op["path"].get<std::string>();
		commit.stateUndo.push_back(std::move(op));
	}
	commit.stateAfter = json::array();
	for(const json& op : change) {
		const std::string part = partOf(after, op["path"].get<std::string>());
		if(std::ranges::any_of(commit.stateAfter, [&](const json& kept) { return kept["path"] == "/state" + part; }))
			continue;
		json kept = {{"path", "/state" + part}};
		if(const json* value = detail::locate(after, part)) kept["value"] = *value;
		commit.stateAfter.push_back(std::move(kept));
	}
}

json Repository::stateRevert(const Commit& target) const {
	if(target.stateUndo.is_null()) return json::array();
	for(const json& part : target.stateAfter) {
		const std::string& path = part["path"].get_ref<const std::string&>();
		const json* now = detail::locate(committedState(), path.substr(std::string_view("/state").size()));
		const bool unchanged = part.contains("value") ? now && *now == part["value"] : now == nullptr;
		if(!unchanged)
			fail("revert", "conflict: " + path + " changed after " + describe(target) +
			                   "; restore an earlier revision "
			                   "instead");
	}
	return target.stateUndo;
}

json Repository::documentOf(const Commit& commit) const {
	if(commit.id == main_.back()) return mainDocument();
	if(commit.snapshot) return *commit.snapshot;
	return rebuild(objects_, commit);
}

json Repository::rebuild(const std::unordered_map<std::string, Commit>& objects, const Commit& commit) {
	std::vector<const Commit*> chain;
	const Commit* base = &commit;
	for(; !base->snapshot; base = &objects.at(base->parents.front())) chain.push_back(base);

	json document = *base->snapshot;
	for(const Commit* next : chain | std::views::reverse) {
		try {
			detail::applyDelta(document, next->delta, "delta");
		} catch(const json::exception& error) {
			fail("commit " + next->id,
			     "its delta doesn't apply (" + detail::reasonOf(error) + "): the history is damaged");
		}
	}
	// Deltas read from disk aren't covered by the ids: the document they give must be the one the commit names
	if(!chain.empty() && detail::sha256(document.dump()) != commit.tree)
		fail("commit " + commit.id, "its deltas don't give the document its id names: the history is damaged");
	return document;
}

json Repository::deltaOf(const Commit& commit) const {
	if(!commit.delta.is_null()) return commit.delta;
	// A commit from a history written before deltas keeps its whole document, as its parent does
	return detail::delta(documentOf(objects_.at(commit.parents.front())), documentOf(commit));
}

// The live state isn't in the documents: the commits whose patches reached the path changed it
CommitInfo Repository::blameState(const std::string& pointer) const {
	const std::vector<std::string> path = detail::segmentsOf(pointer);
	for(const std::string& id : main_ | std::views::reverse) {
		const Commit& commit = objects_.at(id);
		if(std::ranges::any_of(commit.patch, [&](const json& op) { return op.is_object() && reaches(op, path); }))
			return info(commit);
	}
	return info(objects_.at(main_.front())); // as the screen file has it
}

CommitInfo Repository::blame(std::string_view target) const {
	const std::string text(target);
	if(text.empty() || (text.front() != '#' && text.front() != '/'))
		fail("blame", R"(expected a node ("#board", "#board/text") or a path ("/colors/accent", "/state/cards"))");

	if(text == "/state" || text.starts_with("/state/")) return blameState(text);

	const json& head = mainDocument();
	const std::string pointer = resolvePath(head, text, "blame");
	if(!detail::locate(head, pointer)) fail("blame", "nothing at '" + text + "' on main's head");
	Trail trail(head, pointer);
	for(std::size_t n = main_.size() - 1; n > 0; --n) {
		const Commit& commit = objects_.at(main_[n]);
		const json worked = commit.delta.is_null() ? deltaOf(commit) : json(); // only for a history without deltas
		if(trail.changedBy(commit.delta.is_null() ? worked : commit.delta)) return info(commit);
	}
	return info(objects_.at(main_.front())); // unchanged since the screen file
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
		fail("revert", "nothing to revert: " + describe(target) +
		                   " changed neither the document nor, in this run, the live state");
	patch.insert(patch.end(), undo.begin(), undo.end());
	return {
	    {"author", std::string(author)}, {"message", "Revert \"" + target.message + "\""}, {"patch", std::move(patch)}};
}

void Repository::checkDocumentRevert(const Commit& target, const json& then, const json& patch) const {
	for(const json& op : patch) {
		const std::string checked = revertedPart(then, op);
		if(!unchangedSince(mainDocument(), then, checked))
			fail("revert", "conflict: " + (checked.empty() ? std::string("the screen") : checked) + " changed after " +
			                   describe(target) + "; restore an earlier revision instead");
	}
}

json Repository::diff(const json& from, const json& to) const {
	return json::diff(documentOf(find(from)), documentOf(find(to)));
}

} // namespace affresco::vcs
