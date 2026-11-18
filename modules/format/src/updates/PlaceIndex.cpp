/// @file
/// @brief format::PlaceIndex: the readers of the state by the place they read.

#include "PlaceIndex.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Takes the next key off a JSON Pointer.
/// @param pointer The pointer, "/key..."; loses its first key.
/// @return The key, escaped as in the pointer.
// NOLINTNEXTLINE(bugprone-exception-escape): substr starts at 0, so it doesn't throw
std::string_view takeKey(std::string_view& pointer) noexcept {
	pointer.remove_prefix(1);
	const std::size_t end = std::min(pointer.find('/'), pointer.size());
	const std::string_view key = pointer.substr(0, end);
	pointer.remove_prefix(end);
	return key;
}

/// @brief A place without its last key when that key is shapeKey: the list whose shape it reads.
/// @param place The place.
/// @param shape Gets whether it reads a shape.
/// @return The place to keep in the trie.
std::string_view shapeless(std::string_view place, bool& shape) noexcept {
	shape =
	    place.ends_with(shapeKey) && place.size() > shapeKey.size() && place[place.size() - shapeKey.size() - 1] == '/';
	if(shape) place.remove_suffix(shapeKey.size() + 1);
	return place;
}

/// @brief The key of the map of children: a parent and a key's number.
/// @param parent The node above.
/// @param key The key's number.
/// @return Both in one number.
constexpr std::uint64_t childKey(std::uint32_t parent, std::uint32_t key) noexcept {
	return (static_cast<std::uint64_t>(parent) << 32u) | key;
}

/// @brief Keeps a free number for reuse, if there is memory for it (else it is never used again).
/// @param free The free numbers.
/// @param number The number.
void keepFree(std::vector<std::uint32_t>& free, std::uint32_t number) noexcept {
	try {
		free.push_back(number);
	} catch(...) { // NOLINT(bugprone-empty-catch): out of memory, the number is never used again
	}
}

} // namespace

PlaceIndex::PlaceIndex() : nodes_(1), anyKeyId_(intern(anyKey)) { // the root, the whole state: its parent is itself
	keys_[anyKeyId_].uses = std::numeric_limits<std::uint32_t>::max() / 2; // kept for good
}

std::uint32_t PlaceIndex::intern(std::string_view key) {
	if(const auto found = keyIds_.find(key); found != keyIds_.end()) return found->second;
	std::uint32_t id = 0;
	if(freeKeys_.empty()) {
		id = static_cast<std::uint32_t>(keys_.size());
		keys_.emplace_back();
	} else {
		id = freeKeys_.back();
		freeKeys_.pop_back();
	}
	keys_[id].text = key;
	keyIds_.emplace(std::string(key), id);
	return id;
}

const std::uint32_t* PlaceIndex::child(std::uint32_t parent, std::uint32_t key) const {
	const auto found = childOf_.find(childKey(parent, key));
	return found == childOf_.end() ? nullptr : &found->second;
}

std::uint32_t PlaceIndex::childMade(std::uint32_t parent, std::string_view key) {
	const std::uint32_t keyId = intern(key);
	if(const std::uint32_t* found = child(parent, keyId)) return *found;
	std::uint32_t id = 0;
	if(freeNodes_.empty()) {
		id = static_cast<std::uint32_t>(nodes_.size());
		nodes_.emplace_back();
	} else {
		id = freeNodes_.back();
		freeNodes_.pop_back();
	}
	Node& node = nodes_[id];
	node.parent = parent;
	node.key = keyId;
	node.position = static_cast<std::uint32_t>(nodes_[parent].children.size());
	nodes_[parent].children.push_back(id);
	++keys_[keyId].uses;
	childOf_.emplace(childKey(parent, keyId), id);
	return id;
}

void PlaceIndex::link(Slot& slot, std::string_view place) {
	bool shape = false;
	std::string_view rest = shapeless(place, shape);
	std::uint32_t node = root;
	while(!rest.empty()) node = childMade(node, takeKey(rest));
	std::vector<Entry>& readers = nodes_[node].readers;
	readers.push_back({slot.id | (shape ? shapeBit : 0u), static_cast<std::uint32_t>(slot.links.size())});
	slot.links.push_back({node, static_cast<std::uint32_t>(readers.size() - 1)});
	++entries_;
}

void PlaceIndex::add(ReaderId reader, std::span<const std::string> places) {
	const std::uint32_t at = slotOf(reader);
	if(at >= slots_.size()) slots_.resize(at + 1u);
	Slot& slot = slots_[at];
	if(slot.id != reader) { // a new reader in the slot: the old one's places go
		remove(slot.id);
		slot.id = reader;
	}
	for(const std::string& place : places) link(slot, place);
}

void PlaceIndex::set(ReaderId reader, std::span<const std::string> places) {
	const std::uint32_t at = slotOf(reader);
	if(at >= slots_.size() || slots_[at].id != reader) {
		add(reader, places);
		return;
	}
	Slot& slot = slots_[at];
	// The old entries go first, their nodes kept until the new ones are made: the nodes both read stay
	for(std::size_t i = slot.links.size(); i-- > 0;) unlink(slot.links[i], false);
	scratch_.swap(slot.links);
	slot.links.clear();
	for(const std::string& place : places) link(slot, place);
	for(const Link& old : scratch_) prune(old.node);
	scratch_.clear();
}

void PlaceIndex::unlink(const Link& link, bool pruned) noexcept {
	std::vector<Entry>& readers = nodes_[link.node].readers;
	const Entry moved = readers.back();
	readers[link.position] = moved;
	readers.pop_back();
	if(link.position < readers.size()) { // the entry moved into the freed place: its reader learns where
		Slot& owner = slots_[slotOf(moved.reader & ~shapeBit)];
		owner.links[moved.link].position = link.position;
	}
	--entries_;
	if(pruned) prune(link.node);
}

void PlaceIndex::prune(std::uint32_t node) noexcept {
	while(node != root && nodes_[node].parent != freed && nodes_[node].readers.empty() &&
	      nodes_[node].children.empty()) {
		Node& gone = nodes_[node];
		const std::uint32_t parent = gone.parent;
		std::vector<std::uint32_t>& siblings = nodes_[parent].children;
		const std::uint32_t last = siblings.back();
		siblings[gone.position] = last;
		nodes_[last].position = gone.position;
		siblings.pop_back();
		childOf_.erase(childKey(parent, gone.key));
		if(--keys_[gone.key].uses == 0) {
			keyIds_.erase(keys_[gone.key].text);
			keys_[gone.key].text.clear();
			keepFree(freeKeys_, gone.key);
		}
		std::vector<Entry>().swap(gone.readers);
		std::vector<std::uint32_t>().swap(gone.children);
		gone.parent = freed;
		keepFree(freeNodes_, node);
		node = parent;
	}
}

void PlaceIndex::remove(ReaderId reader) noexcept {
	const std::uint32_t at = slotOf(reader);
	if(reader == 0 || at >= slots_.size() || slots_[at].id != reader) return;
	Slot& slot = slots_[at];
	slot.id = 0;
	// Last first: an entry moved by an unlink is always one of another reader's, or one not yet unlinked
	for(std::size_t i = slot.links.size(); i-- > 0;) unlink(slot.links[i], true);
	std::vector<Link>().swap(slot.links);
}

bool PlaceIndex::contains(ReaderId reader) const noexcept {
	const std::uint32_t at = slotOf(reader);
	return reader != 0 && at < slots_.size() && slots_[at].id == reader;
}

void PlaceIndex::collectAt(std::uint32_t node, bool shape, std::vector<ReaderId>& out) {
	for(const Entry& entry : nodes_[node].readers) {
		if(!shape && (entry.reader & shapeBit) != 0u) continue;
		const ReaderId id = entry.reader & ~shapeBit;
		std::uint32_t& seen = seen_[slotOf(id)];
		if(seen == collecting_) continue;
		seen = collecting_;
		out.push_back(id);
	}
}

void PlaceIndex::collectBelow(std::uint32_t node, std::vector<ReaderId>& out) {
	below_.assign(1, node);
	while(!below_.empty()) {
		const std::uint32_t at = below_.back();
		below_.pop_back();
		collectAt(at, true, out);
		below_.insert(below_.end(), nodes_[at].children.begin(), nodes_[at].children.end());
	}
}

bool PlaceIndex::reach(std::string_view place, bool ancestors, std::vector<ReaderId>& out) {
	frontier_.assign(1, root);
	while(!place.empty() && !frontier_.empty()) {
		const std::string_view key = takeKey(place);
		const auto found = keyIds_.find(key);
		next_.clear();
		for(const std::uint32_t node : frontier_) {
			if(ancestors) collectAt(node, false, out); // they read a place that contains it
			if(found != keyIds_.end() && found->second != anyKeyId_)
				if(const std::uint32_t* below = child(node, found->second)) next_.push_back(*below);
			if(const std::uint32_t* any = child(node, anyKeyId_)) next_.push_back(*any);
		}
		frontier_.swap(next_);
	}
	return !frontier_.empty();
}

void PlaceIndex::collectPlace(std::string_view place, std::vector<ReaderId>& out) {
	if(!reach(place, true, out)) return;
	for(const std::uint32_t node : frontier_) collectBelow(node, out);
}

void PlaceIndex::collectShape(std::string_view place, std::vector<ReaderId>& out) {
	if(!reach(place, false, out)) return;
	for(const std::uint32_t node : frontier_) collectAt(node, true, out);
}

void PlaceIndex::collect(const StateChanges& changes, std::vector<ReaderId>& out) {
	if(changes.all) return;
	if(seen_.size() < slots_.size()) seen_.resize(slots_.size(), 0);
	if(++collecting_ == 0) { // wrapped: no slot may look collected already
		std::ranges::fill(seen_, 0);
		collecting_ = 1;
	}
	for(const std::string& place : changes.paths) collectPlace(place, out);
	for(const ListChange& list : changes.lists) collectShape(list.place, out);
}

} // namespace affresco::format
