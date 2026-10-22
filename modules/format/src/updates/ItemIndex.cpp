/// @file
/// @brief format::ItemIndex: which children of a long list read which item.

#include "ItemIndex.hpp"

#include <algorithm>

namespace affresco::format {

namespace {

/// @brief The place of the item a child's variables come from: the nearest scope with one.
/// @param locals The variables.
/// @return The place, or empty if none comes from the state.
std::string_view originOf(const Scope* locals) noexcept {
	for(const Scope* scope = locals; scope; scope = scope->parent)
		if(!scope->origin.empty()) return scope->origin;
	return {};
}

/// @brief Whether everything read is inside one item.
/// @param origin The item's place (empty: none).
/// @param reads What was read.
/// @return true if every place read is the item or inside it.
bool readsOnly(std::string_view origin, const ReadSet& reads) {
	if(origin.empty() || reads.always) return false;
	return std::ranges::all_of(reads.paths, [origin](const std::string& path) { return contains(origin, path); });
}

} // namespace

void ItemIndex::add(std::size_t child, const Scope* locals, const ReadSet& reads) {
	if(!built_) return;
	const std::string_view origin = originOf(locals);
	if(!readsOnly(origin, reads)) {
		others_.push_back(child);
		return;
	}
	const std::string_view list = origin.substr(0, origin.rfind('/'));
	if(std::ranges::find(lists_, list) == lists_.end()) {
		if(lists_.size() >= maxLists) {
			drop();
			return;
		}
		lists_.emplace_back(list);
	}
	local_.emplace(std::string(origin), child);
}

bool ItemIndex::keeps(const Scope* locals, const ReadSet& reads) const {
	if(!built_) return true;
	const std::string_view origin = originOf(locals);
	// A child not kept by its item is with the others, visited every time; one kept by it must still read only it.
	return readsOnly(origin, reads) || origin.empty() || !local_.contains(origin);
}

std::optional<std::vector<std::size_t>> ItemIndex::visits(const StateChanges& changes, std::size_t children) const {
	if(!built_ || changes.all || children != children_) return std::nullopt;
	std::vector<std::size_t> chosen = others_;
	for(const std::string& written : changes.paths) {
		if(std::ranges::any_of(lists_, [&written](const std::string& list) { return contains(written, list, true); }))
			return std::nullopt; // the list itself, or a place around it: every item may have changed
		for(std::size_t end = written.find('/', 1); ; end = written.find('/', end + 1)) {
			const auto [first, last] = local_.equal_range(std::string_view(written).substr(0, end));
			for(auto it = first; it != last; ++it) chosen.push_back(it->second);
			if(end == std::string::npos) break;
		}
	}
	std::ranges::sort(chosen);
	const auto [from, to] = std::ranges::unique(chosen);
	chosen.erase(from, to);
	return chosen;
}

} // namespace affresco::format
