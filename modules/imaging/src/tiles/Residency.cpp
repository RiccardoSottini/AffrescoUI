/// @file
/// @brief TileResidency: a least-recently-used list within a budget of bytes.

#include "affresco/imaging/Residency.hpp"

namespace affresco::imaging {

Resident* TileResidency::find(const ResidentKey& key) {
	const auto found = entries_.find(key);
	if(found == entries_.end()) return nullptr;
	order_.splice(order_.begin(), order_, found->second);
	return &found->second->second;
}

bool TileResidency::admit(const ResidentKey& key, const Resident& resident, std::vector<Resident>& evicted) {
	evicted.clear();
	if(resident.bytes > budget_) return false;
	while(bytes_ + resident.bytes > budget_ && !order_.empty()) {
		const Entry& oldest = order_.back();
		evicted.push_back(oldest.second);
		bytes_ -= oldest.second.bytes;
		entries_.erase(oldest.first);
		order_.pop_back();
		++evictions_;
	}
	order_.emplace_front(key, resident);
	entries_[key] = order_.begin();
	bytes_ += resident.bytes;
	return true;
}

std::optional<Resident> TileResidency::forget(const ResidentKey& key) {
	const auto found = entries_.find(key);
	if(found == entries_.end()) return std::nullopt;
	const Resident resident = found->second->second;
	bytes_ -= resident.bytes;
	order_.erase(found->second);
	entries_.erase(found);
	return resident;
}

void TileResidency::clear(std::vector<Resident>& evicted) {
	evicted.clear();
	for(const Entry& entry : order_) evicted.push_back(entry.second);
	order_.clear();
	entries_.clear();
	bytes_ = 0;
}

} // namespace affresco::imaging
