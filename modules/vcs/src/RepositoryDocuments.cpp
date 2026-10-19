/// @file
/// @brief vcs::Repository's documents: rebuilding a commit's document from the nearest snapshot and the deltas after
/// it, and blame, which follows a node or a path back through the deltas to the commit that last changed it.

#include "affresco/vcs/Repository.hpp"

#include "Deltas.hpp"
#include "Permissions.hpp"
#include "Reasons.hpp"
#include "Sha256.hpp"

#include <algorithm>
#include <cctype>

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

		// An item added or removed in a list that holds it moves it one place
		const std::size_t list = at.size() - 1;
		if(at.empty() || list >= path_.size() || !lists_[list] ||
		   !std::equal(at.begin(), std::prev(at.end()), path_.begin()))
			return false;
		const long long changed = positionOf(at.back()), position = positionOf(path_[list]);
		if(changed < 0 || position < 0) return false;
		if(kind == "add" && position > changed) path_[list] = std::to_string(position - 1);
		if(kind == "remove" && position >= changed) path_[list] = std::to_string(position + 1);
		return false;
	}

	std::vector<std::string> path_; ///< Where it is, in the document of the commit reached.
	std::vector<bool> lists_;       ///< For each segment: whether what holds it is a list.
};

/// @brief Tells whether an operation of a revision's patch reaches a path of the live state.
/// @param op The operation, as submitted.
/// @param target The path's segments ("state", ...).
/// @return true if its path or its "from" is the path, inside it, or holds it.
bool reaches(const json& op, const std::vector<std::string>& target) {
	for(const char* key : {"path", "from"}) {
		if(!op.contains(key) || !op[key].is_string() || !op[key].get_ref<const std::string&>().starts_with("/state"))
			continue;
		const std::vector<std::string> at = detail::segmentsOf(op[key].get_ref<const std::string&>());
		if(startsWith(at, target) || startsWith(target, at)) return true;
	}
	return false;
}

} // namespace

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
	for(auto it = chain.rbegin(); it != chain.rend(); ++it) {
		try {
			detail::applyDelta(document, (*it)->delta, "delta");
		} catch(const json::exception& error) {
			fail("commit " + (*it)->id,
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

CommitInfo Repository::blame(std::string_view target) const {
	const std::string text(target);
	if(text.empty() || (text.front() != '#' && text.front() != '/'))
		fail("blame", "expected a node (\"#board\", \"#board/text\") or a path (\"/colors/accent\", \"/state/cards\")");

	// The live state isn't in the documents: the commits whose patches reached the path changed it
	if(text == "/state" || text.starts_with("/state/")) {
		const std::vector<std::string> path = detail::segmentsOf(text);
		for(auto id = main_.rbegin(); id != main_.rend(); ++id) {
			const Commit& commit = objects_.at(*id);
			if(std::ranges::any_of(commit.patch, [&](const json& op) { return op.is_object() && reaches(op, path); }))
				return info(commit);
		}
		return info(objects_.at(main_.front())); // as the screen file has it
	}

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

} // namespace affresco::vcs
