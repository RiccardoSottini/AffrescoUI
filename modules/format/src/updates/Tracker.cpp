/// @file
/// @brief format::Tracker: the readers of the state and what each read.

#include "Tracker.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace affresco::format {

ReaderId Tracker::add(Reader& reader) {
	const ReaderId id = next_++;
	readers_.emplace(id, Entry{&reader, {}});
	return id;
}

void Tracker::remove(ReaderId id) noexcept { readers_.erase(id); }

void Tracker::setReads(ReaderId id, ReadSet reads) {
	if(const auto found = readers_.find(id); found != readers_.end()) found->second.reads = std::move(reads);
}

// Plainly: a scan of every reader
std::vector<Reader*> Tracker::touched(const StateChanges& changes) const {
	std::vector<Reader*> result;
	for(const auto& [id, entry] : readers_)
		if(entry.reads.touchedBy(changes)) result.push_back(entry.reader);
	std::ranges::sort(result, [](const Reader* a, const Reader* b) { return a->order() < b->order(); });
	return result;
}

void Tracker::update(StateChanges& changes) {
	if(readers_.empty() || changes.empty()) return;
	DocumentContext& context = engine_.document;
	const DocumentContext::UpdatingScope running(context, changes);
	std::unordered_set<const Reader*> ran;
	StateChanges round = changes;
	for(int i = 0; i < maxRounds; ++i) {
		std::vector<Reader*> due = touched(round);
		std::erase_if(due, [&ran](const Reader* reader) { return ran.contains(reader); });
		if(due.empty()) return;
		for(Reader* reader : due) {
			ran.insert(reader);
			reader->update(changes);
		}
		round = context.takeChanges(); // the places they marked belong to this update
		if(round.empty()) return;
		if(round.all) changes.addAll(); // the next round looks at these only
		for(const std::string& path : round.paths) changes.add(path);
		if(!changes.all) changes.lists.insert(changes.lists.end(), round.lists.begin(), round.lists.end());
	}
	std::string names;
	for(const Reader* reader : touched(round)) {
		if(!names.empty()) names += ", ";
		names += reader->name();
	}
	engine_.report("update: readers mark what each other read in a circle (more than " + std::to_string(maxRounds) +
	               " rounds): " + names);
}

} // namespace affresco::format
