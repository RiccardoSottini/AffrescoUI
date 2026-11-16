/// @file
/// @brief format::RunUndo: what the newest runs changed, and undo.

#include "RunUndo.hpp"

#include <affresco/data/Store.hpp>

#include <algorithm>
#include <functional>
#include <utility>

namespace affresco::format {

namespace {

/// @brief The value at a place of a JSON value, if it has one.
/// @param value The value.
/// @param path The place.
/// @return A copy of it, or std::nullopt.
std::optional<json> valueAt(const json& value, const json::json_pointer& path) {
	if(!value.contains(path)) return std::nullopt;
	return value.at(path);
}

/// @brief The bytes a place takes: its path and its values as JSON text.
/// @param place The place.
/// @return They.
std::size_t bytesOf(const RunUndo::Place& place) {
	std::size_t bytes = place.path.to_string().size();
	if(place.before) bytes += place.before->dump(-1, ' ', false, json::error_handler_t::replace).size();
	if(place.after) bytes += place.after->dump(-1, ' ', false, json::error_handler_t::replace).size();
	return bytes;
}

/// @brief Reads a store's journal, newest first, while each entry is wanted: page after page, each one larger.
/// @param store The store.
/// @param wanted Takes an entry; false stops the reading.
void readJournal(const data::Store& store, const std::function<bool(const data::JournalEntry&)>& wanted) {
	std::size_t limit = 64;
	std::size_t seen = 0;
	while(true) {
		const std::vector<data::JournalEntry> entries = store.journal(limit);
		for(std::size_t at = seen; at < entries.size(); ++at)
			if(!wanted(entries[at])) return;
		seen = entries.size();
		if(entries.size() < limit || limit >= data::Limits::journalEntries) return;
		limit *= 4U;
	}
}

/// @brief The name of what a journal entry did to a record, as a run's counts say it.
/// @param action "insert", "update" or "delete".
/// @return "inserted", "updated" or "deleted".
const char* countName(const std::string& action) {
	return action == "insert" ? "inserted" : action == "update" ? "updated" : "deleted";
}

} // namespace

std::vector<RunUndo::Place> RunUndo::placesOf(const json& before, const json& after, const json& patch) {
	std::vector<json::json_pointer> paths;
	for(const json& operation : patch) {
		json::json_pointer path(operation["path"].get<std::string>());
		const std::string op = operation["op"].get<std::string>();
		// An element added to or taken from an array moves the others: the array is the place
		if((op == "add" || op == "remove") && !path.empty()) {
			const json::json_pointer parent = path.parent_pointer();
			const json& holder = op == "add" ? after : before;
			if(holder.contains(parent) && holder.at(parent).is_array()) path = parent;
		}
		paths.push_back(std::move(path));
	}
	std::ranges::sort(paths, {}, [](const json::json_pointer& path) { return path.to_string(); });
	std::vector<Place> places;
	for(const json::json_pointer& path : paths) {
		const std::string text = path.to_string();
		const bool inside = !places.empty() && [&] {
			const std::string outer = places.back().path.to_string();
			return text == outer || text.starts_with(outer + "/");
		}();
		if(inside) continue; // within a place already kept, or the same one
		places.push_back({path, valueAt(before, path), valueAt(after, path)});
	}
	return places;
}

void RunUndo::keep(Kept kept) {
	kept.bytes = 0;
	for(const Place& place : kept.places) kept.bytes += bytesOf(place);
	if(kept.bytes > maxBytes) { // nothing of it, rather than part of it
		kept.places.clear();
		kept.groups.clear();
		kept.tooLarge = true;
		kept.bytes = 0;
	}
	newest_ = std::max(newest_, kept.run);
	bytes_ += kept.bytes;
	kept_.push_back(std::move(kept));
	while(kept_.size() > maxUndoableRuns || bytes_ > maxBytes) {
		bytes_ -= kept_.front().bytes;
		kept_.pop_front();
	}
}

const RunUndo::Kept* RunUndo::find(int run) const {
	const auto found = std::ranges::find(kept_, run, &Kept::run);
	return found == kept_.end() ? nullptr : &*found;
}

std::optional<std::string> RunUndo::stateRefusal(const Kept& kept, const json& state) {
	for(const Place& place : kept.places) {
		// What holds the place must still hold values: a key the run removed has none now either, in a number too
		const bool held = place.path.empty() || [&] {
			const json::json_pointer parent = place.path.parent_pointer();
			return state.contains(parent) && state.at(parent).is_structured();
		}();
		if(!held || valueAt(state, place.path) != place.after)
			return "run " + std::to_string(kept.run) + " changed /state" + place.path.to_string() +
			       ", which changed since";
	}
	return std::nullopt;
}

void RunUndo::restore(const Kept& kept, json& state) {
	for(const Place& place : kept.places) {
		if(place.before) {
			state[place.path] = *place.before;
		} else if(!place.path.empty()) {
			json& holder = state.at(place.path.parent_pointer());
			if(holder.is_object()) holder.erase(place.path.back());
		}
	}
}

std::optional<std::string> RunUndo::undo(int run, json& state, data::Store& store) {
	const std::string named = "run " + std::to_string(run);
	const auto found = std::ranges::find(kept_, run, &Kept::run);
	if(found == kept_.end()) return run > newest_ || run < 1 ? "there is no " + named : named + " is too old to undo";
	Kept& kept = *found;
	if(kept.undone) return named + " is undone already";
	if(kept.tooLarge) return named + " changed more than undo keeps";
	if(kept.places.empty() && kept.groups.empty()) return named + " changed nothing";
	if(std::optional<std::string> why = stateRefusal(kept, state)) return why;
	json restored = state; // put back on a copy first: nothing changes, records nor state, if it can't be
	try {
		restore(kept, restored);
	} catch(const json::exception&) {
		return named + " can't be undone: what holds what it changed changed since";
	}
	if(!kept.groups.empty()) {
		std::vector<std::int64_t> newest = newestGroups(store, kept.groups.size());
		std::ranges::reverse(newest);
		if(newest != kept.groups) return named + "'s records can't be undone: later writes came after";
		std::size_t undone = 0;
		try {
			for(; undone < kept.groups.size(); ++undone) static_cast<void>(store.undo());
		} catch(const data::DataError& error) { // a model changed since: put back what was undone, then refuse
			for(; undone > 0; --undone) static_cast<void>(store.redo());
			return named + "'s records can't be undone: " + error.what();
		}
	}
	state = std::move(restored);
	kept.undone = true;
	return std::nullopt;
}

std::int64_t newestSequence(const data::Store& store) {
	const std::vector<data::JournalEntry> newest = store.journal(1);
	return newest.empty() ? 0 : newest.front().sequence;
}

RecordWrites recordWritesSince(const data::Store& store, std::int64_t after) {
	RecordWrites writes;
	readJournal(store, [&](const data::JournalEntry& entry) {
		if(entry.sequence <= after) return false;
		if(entry.undone) return true; // an all-or-nothing write that undid itself
		if(std::ranges::find(writes.groups, entry.group) == writes.groups.end()) writes.groups.push_back(entry.group);
		json& counts = writes.counts[entry.collection];
		if(counts.is_null()) counts = {{"inserted", 0}, {"updated", 0}, {"deleted", 0}};
		counts[countName(entry.action)] = counts[countName(entry.action)].get<std::size_t>() + 1U;
		++writes.count;
		return true;
	});
	std::ranges::reverse(writes.groups); // read newest first
	return writes;
}

std::vector<std::int64_t> newestGroups(const data::Store& store, std::size_t count) {
	std::vector<std::int64_t> groups;
	readJournal(store, [&](const data::JournalEntry& entry) {
		if(groups.size() >= count) return false;
		if(!entry.undone && std::ranges::find(groups, entry.group) == groups.end()) groups.push_back(entry.group);
		return true;
	});
	return groups;
}

} // namespace affresco::format
