/// @file
/// @brief vcs::Presence: who has a document open, and where.

#include "affresco/vcs/Presence.hpp"

#include <algorithm>
#include <utility>

namespace affresco::vcs {

bool Presence::join(int peer, std::string author) {
	if(entries_.size() >= maxPeers) return false;
	entries_.push_back({peer, std::move(author), {}, {}});
	return true;
}

bool Presence::set(int peer, std::string_view where, std::string_view doing) {
	if(where.size() > maxWhereBytes || doing.size() > maxDoingBytes) return false;
	const auto entry = std::ranges::find(entries_, peer, &PresenceEntry::peer);
	if(entry == entries_.end()) return false;
	entry->where = where;
	entry->doing = doing;
	return true;
}

void Presence::leave(int peer) { std::erase_if(entries_, [peer](const PresenceEntry& e) { return e.peer == peer; }); }

json Presence::toJson() const {
	json list = json::array();
	for(const PresenceEntry& entry : entries_)
		list.push_back({{"author", entry.author}, {"where", entry.where}, {"doing", entry.doing}});
	return list;
}

} // namespace affresco::vcs
