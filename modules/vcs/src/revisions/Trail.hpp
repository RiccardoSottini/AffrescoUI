#pragma once

/// @file
/// @brief detail::Trail: where a node or a value is, followed back through the commits on main, for blame and for an
/// undo's plan (which later commits built on a part).

#include "affresco/core/Json.hpp"

#include "revisions/Deltas.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::vcs::detail {

/// @brief Tells whether one path is the start of another (or the same path).
/// @param path The longer path's segments.
/// @param prefix The shorter path's.
/// @return true if every segment of @p prefix begins @p path.
inline bool startsWith(const std::vector<std::string>& path, const std::vector<std::string>& prefix) {
	return prefix.size() <= path.size() && std::equal(prefix.begin(), prefix.end(), path.begin());
}

/// @brief Reads a list position.
/// @param segment A path segment.
/// @return It as a number, or -1 if it isn't one.
inline long long positionOf(const std::string& segment) {
	if(segment.empty() || segment.size() > 18 ||
	   !std::ranges::all_of(segment, [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
		return -1;
	return std::stoll(segment);
}

/// @brief One operation of a delta, its path split into segments: split once for all the trails that read it.
struct TrailOp {
	std::string kind;            ///< "add", "remove" or "replace".
	std::vector<std::string> at; ///< Its path's segments.
};

/// @brief Splits a delta's operations for trails.
/// @param delta A commit's delta.
/// @return Its operations, in order.
inline std::vector<TrailOp> trailOps(const json& delta) {
	std::vector<TrailOp> ops;
	ops.reserve(delta.size());
	for(const json& op : delta) ops.push_back({op.value("op", ""), segmentsOf(op.value("path", ""))});
	return ops;
}

/// @brief What one commit did to the part a trail follows.
enum class TrailStep {
	Untouched, ///< Nothing (an item added or removed before it in a list only moved it).
	Inside,    ///< It changed the part's value or something inside it: the part was there before, so the trail goes on.
	Replaced,  ///< It added the part, or added or replaced what holds it: before it, the part wasn't there; the trail
	           ///< ends.
};

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
	Trail(const json& head, std::string_view pointer) : path_(segmentsOf(pointer)) {
		const json* value = &head;
		for(const std::string& segment : path_) {
			lists_.push_back(value->is_array());
			value = value->is_array() ? &(*value)[static_cast<std::size_t>(positionOf(segment))] : &(*value)[segment];
		}
	}

	/// @brief Moves back through one commit.
	/// @param delta The commit's delta: how its document differs from its first parent's.
	/// @return What the commit did to the part. After Replaced the trail is spent: don't move it back further.
	TrailStep stepBack(const json& delta) {
		TrailStep step = TrailStep::Untouched;
		for(auto op = delta.rbegin(); op != delta.rend() && step != TrailStep::Replaced; ++op) {
			const TrailStep met = stepBack(op->value("op", ""), segmentsOf(op->value("path", "")));
			if(met != TrailStep::Untouched) step = met;
		}
		return step;
	}

	/// @brief Moves back through one commit whose delta is already split (several trails reading the same commits).
	/// @param ops The commit's delta, split by trailOps().
	/// @return What the commit did to the part, as stepBack(const json&) tells it.
	TrailStep stepBack(const std::vector<TrailOp>& ops) {
		TrailStep step = TrailStep::Untouched;
		for(auto op = ops.rbegin(); op != ops.rend() && step != TrailStep::Replaced; ++op) {
			const TrailStep met = stepBack(op->kind, op->at);
			if(met != TrailStep::Untouched) step = met;
		}
		return step;
	}

	/// @brief Moves back through one commit, for blame.
	/// @param delta The commit's delta.
	/// @return true if the commit changed what the trail follows, in any way; blame stops there.
	bool changedBy(const json& delta) { return stepBack(delta) != TrailStep::Untouched; }

private:
	/// @brief Moves back through one operation.
	/// @param kind "add", "remove" or "replace" (the only ones detail::delta() writes).
	/// @param at Its path.
	/// @return What it did to the part.
	TrailStep stepBack(const std::string& kind, const std::vector<std::string>& at) {
		if(at.size() > path_.size() && startsWith(at, path_)) return TrailStep::Inside;
		// The part itself: a new value at the same place, or the item at its place in a list removed (which moved it
		// there): either way, it was there before. Added, it wasn't
		if(at == path_) return kind == "add" ? TrailStep::Replaced : TrailStep::Inside;
		if(kind != "remove" && startsWith(path_, at)) return TrailStep::Replaced; // what holds it was added or replaced
		if(kind == "replace") return TrailStep::Untouched;
		if(kind != "add" && kind != "remove") return TrailStep::Replaced; // an operation deltas don't hold: the worst
		moveBack(kind == "add", at);
		return TrailStep::Untouched;
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

} // namespace affresco::vcs::detail
