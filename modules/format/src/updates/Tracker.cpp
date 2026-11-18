/// @file
/// @brief format::Tracker: the readers of the state and what each read.

#include "Tracker.hpp"
#include "Engine.hpp"

#include "affresco/core/Log.hpp"

#include <algorithm>
#include <span>
#include <utility>

namespace affresco::format {

namespace {

/// The generations a slot goes through before it starts again at 1 (0 never: slot 0's first handle would be 0).
constexpr std::uint16_t lastGeneration = (1u << readerGenerationBits) - 1u;

} // namespace

void Reader::position(std::vector<std::uint32_t>& out) const {
	const std::uint64_t at = order();
	out.push_back(0);
	out.push_back(static_cast<std::uint32_t>(at >> 32u));
	out.push_back(static_cast<std::uint32_t>(at));
}

Tracker::Running::Running(Tracker& tracker) noexcept : tracker_(tracker) {
	tracker.running_ = true;
	if(++tracker.stamp_ == 0) { // wrapped: no reader may look run already
		for(Entry& entry : tracker.entries_) entry.ran = 0;
		tracker.stamp_ = 1;
	}
}

Tracker::Running::~Running() {
	tracker_.running_ = false;
	if(tracker_.free_.empty()) {
		tracker_.free_.swap(tracker_.freed_);
		return;
	}
	try {
		tracker_.free_.insert(tracker_.free_.end(), tracker_.freed_.begin(), tracker_.freed_.end());
	} catch(...) { // NOLINT(bugprone-empty-catch): out of memory, those slots are never used again
	}
	tracker_.freed_.clear();
}

ReaderId Tracker::add(Reader& reader) {
	std::uint32_t slot = 0;
	if(free_.empty()) {
		slot = static_cast<std::uint32_t>(entries_.size());
		if(slot >= (1u << readerSlotBits)) fail("update", "too many readers of the state");
		entries_.emplace_back();
	} else {
		slot = free_.back();
		free_.pop_back();
	}
	Entry& entry = entries_[slot];
	entry.generation = entry.generation >= lastGeneration ? 1 : static_cast<std::uint16_t>(entry.generation + 1);
	entry.reader = &reader;
	entry.id = slot | (static_cast<std::uint32_t>(entry.generation) << readerSlotBits);
	entry.alwaysAt = noPlace;
	entry.ran = running_ ? stamp_ : 0; // built while an update runs: up to date with it
	entry.panel = reader.panelReader();
	return entry.id;
}

Tracker::Entry* Tracker::live(ReaderId id) noexcept {
	const std::uint32_t slot = slotOf(id);
	if(id == 0 || slot >= entries_.size()) return nullptr;
	Entry& entry = entries_[slot];
	return entry.reader && entry.id == id ? &entry : nullptr;
}

void Tracker::leaveAlways(Entry& entry) noexcept {
	if(entry.alwaysAt == noPlace) return;
	const ReaderId last = always_.back();
	always_[entry.alwaysAt] = last;
	entries_[slotOf(last)].alwaysAt = entry.alwaysAt;
	always_.pop_back();
	entry.alwaysAt = noPlace;
}

void Tracker::remove(ReaderId id) noexcept {
	Entry* entry = live(id);
	if(!entry) return;
	index_.remove(id);
	leaveAlways(*entry);
	entry->reader = nullptr;
	entry->id = 0;
	try {
		(running_ ? freed_ : free_).push_back(slotOf(id));
	} catch(...) { // NOLINT(bugprone-empty-catch): out of memory, the slot is never used again
	}
}

void Tracker::setReads(ReaderId id, ReadSet reads) {
	Entry* entry = live(id);
	if(!entry) return;
	if(running_) entry->ran = stamp_; // what it read now is up to date with this update
	if(!reads.always && index_.entries() + reads.paths.size() > maxEntries) {
		reads.addAlways(AlwaysReason::TooMany);
		if(!warned_)
			affresco::log(LogLevel::Warning, "update: more than " + std::to_string(maxEntries) +
			                                     " places read; readers past them are updated on every update");
		warned_ = true;
	}
	if(reads.always) {
		index_.remove(id);
		if(entry->alwaysAt == noPlace) {
			entry->alwaysAt = static_cast<std::uint32_t>(always_.size());
			always_.push_back(id);
		}
		return;
	}
	leaveAlways(*entry);
	index_.set(id, reads.paths);
}

std::vector<Reader*> Tracker::touched(const StateChanges& changes) const {
	std::vector<ReaderId> ids;
	if(changes.all) {
		for(const Entry& entry : entries_)
			if(entry.reader && !entry.panel) ids.push_back(entry.id);
	} else {
		index_.collect(changes, ids);
		ids.insert(ids.end(), always_.begin(), always_.end());
	}
	std::vector<std::pair<std::vector<std::uint32_t>, Reader*>> ordered;
	ordered.reserve(ids.size());
	for(const ReaderId id : ids) {
		Reader* reader = entries_[slotOf(id)].reader;
		ordered.emplace_back();
		reader->position(ordered.back().first);
		ordered.back().second = reader;
	}
	std::ranges::sort(ordered, [](const auto& a, const auto& b) { return a.first < b.first; });
	std::vector<Reader*> result;
	result.reserve(ordered.size());
	for(const auto& [position, reader] : ordered) result.push_back(reader);
	return result;
}

bool Tracker::enqueue(const std::vector<ReaderId>& ids, std::uint32_t round, std::size_t from) {
	bool again = false;
	const std::size_t start = due_.size();
	for(const ReaderId id : ids) {
		const Entry* entry = live(id);
		if(!entry) continue;
		if(entry->ran == stamp_) {
			again = true;
			continue;
		}
		const auto key = static_cast<std::uint32_t>(keys_.size());
		entry->reader->position(keys_);
		due_.push_back({id, round, key, static_cast<std::uint32_t>(keys_.size()) - key});
	}
	if(due_.size() == start) return again;
	const std::span<const std::uint32_t> keys(keys_);
	const auto before = [keys](const Due& a, const Due& b) {
		return std::ranges::lexicographical_compare(keys.subspan(a.key, a.length), keys.subspan(b.key, b.length));
	};
	std::sort(due_.begin() + static_cast<std::ptrdiff_t>(from), due_.end(), before); // after the one running
	return again;
}

void Tracker::join(StateChanges& changes, const StateChanges& marked) {
	if(marked.all) changes.addAll();
	for(const std::string& path : marked.paths) changes.add(path);
	if(!changes.all) changes.lists.insert(changes.lists.end(), marked.lists.begin(), marked.lists.end());
}

void Tracker::runAll(StateChanges& changes) {
	due_.clear();
	keys_.clear();
	found_.clear();
	for(const Entry& entry : entries_)
		if(entry.reader && !entry.panel) found_.push_back(entry.id);
	enqueue(found_, 0, 0);
	DocumentContext& context = engine_.document;
	for(const Due& due : due_) { // nothing joins due_ here: places marked are left to the full pass
		Entry* entry = live(due.id);
		if(!entry || entry->ran == stamp_) continue;
		entry->ran = stamp_;
		entry->reader->update(changes);
	}
	join(changes, context.takeChanges()); // the places they marked belong to this update
}

void Tracker::reportCircle(std::size_t from) {
	std::string names;
	for(std::size_t i = from; i < due_.size(); ++i) {
		const Entry* entry = live(due_[i].id);
		if(!entry || entry->ran == stamp_) continue;
		if(!names.empty()) names += ", ";
		names += entry->reader->name();
	}
	engine_.report("update: readers mark what each other read in a circle (more than " + std::to_string(maxRounds) +
	               " rounds): " + names);
}

void Tracker::collect(const StateChanges& changes, bool panels) {
	found_.clear();
	index_.collect(changes, found_);
	if(!panels) std::erase_if(found_, [this](ReaderId id) { return entries_[slotOf(id)].panel; });
}

void Tracker::shift(const std::string& list, std::size_t from, std::ptrdiff_t by) {
	if(by == 0 || std::ranges::find(shifted_, list) != shifted_.end()) return;
	if(running_) shifted_.push_back(list);
	index_.shift(list, from, by, pending_);
	if(!running_) {
		pending_.clear();
		return;
	}
	for(const ReaderId id : pending_)
		if(Entry* entry = live(id)) entry->ran = 0; // numbered again: they run even if kept (keepRows())
}

void Tracker::listRebuilt(const std::string& list) {
	if(running_ && std::ranges::find(rebuilt_, list) == rebuilt_.end()) rebuilt_.push_back(list);
}

bool Tracker::rebuiltInFull(const std::string& list) const {
	return running_ && std::ranges::find(rebuilt_, list) != rebuilt_.end();
}

void Tracker::keepRows(const std::string& list, const StateChanges& changes) {
	if(!running_) return;
	std::vector<ReaderId> rows;
	index_.collectRows(list, rows);
	if(rows.empty()) return;
	StateChanges others;
	for(const std::string& path : changes.paths)
		if(path != list) others.add(path);
	for(const ListChange& change : changes.lists)
		if(change.place != list) others.lists.push_back(change);
	std::vector<ReaderId> reading;
	index_.collect(others, reading);
	std::ranges::sort(reading);
	for(const ReaderId id : rows)
		if(Entry* entry = live(id); entry && !std::ranges::binary_search(reading, id)) entry->ran = stamp_;
}

void Tracker::rerunRows(const std::string& list) {
	if(!running_) return; // a full pass follows
	index_.collectRows(list, pending_);
}

void Tracker::rerunAlways() {
	for(const ReaderId id : always_) {
		Entry& entry = entries_[slotOf(id)];
		if(!entry.panel || entry.ran != stamp_) continue;
		entry.ran = 0; // what it read may have been built from the place marked: it runs again in this update
		found_.push_back(id);
	}
}

void Tracker::enqueuePending(std::uint32_t round, std::size_t from) {
	if(pending_.empty()) return;
	enqueue(pending_, round, from);
	pending_.clear();
}

bool Tracker::update(StateChanges& changes, bool panels) {
	if(changes.empty()) return true;
	DocumentContext& context = engine_.document;
	const DocumentContext::UpdatingScope updating(context, changes);
	const Running running(*this);
	pending_.clear();
	shifted_.clear();
	rebuilt_.clear();
	if(changes.all) {
		runAll(changes);
		return false;
	}
	due_.clear();
	keys_.clear();
	collect(changes, panels);
	for(const ReaderId id : always_)
		if(panels || !entries_[slotOf(id)].panel) found_.push_back(id); // a full pass builds the panels again
	enqueue(found_, 0, 0);
	StateChanges again; // marked places a reader run already reads: for the next update
	for(std::size_t next = 0; next < due_.size(); ++next) {
		const Due due = due_[next];
		Entry* entry = live(due.id);
		if(!entry || entry->ran == stamp_) continue; // removed by a reader before it, or built since
		if(due.round > static_cast<std::uint32_t>(maxRounds)) {
			reportCircle(next);
			break;
		}
		entry->ran = stamp_;
		entry->reader->update(changes);
		enqueuePending(due.round, next + 1);
		const StateChanges marked = context.takeChanges(); // the places it marked belong to this update
		if(marked.empty()) continue;
		join(changes, marked);
		if(changes.all) {
			runAll(changes);
			return false;
		}
		collect(marked, panels);
		if(panels) rerunAlways();
		if(enqueue(found_, due.round + 1, next + 1)) join(again, marked);
	}
	for(const std::string& path : again.paths) context.markPlaceChanged(path);
	return true;
}

} // namespace affresco::format
