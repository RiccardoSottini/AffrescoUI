/// @file
/// @brief The three-way merge of JSON documents, with lists of nodes keyed by id (Merge.hpp).

#include "review/Merge.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace affresco::vcs::detail {

namespace {

/// @brief A value on one side, or nullptr where that side has none.
using Side = const json*;

/// @brief The ids of a keyed list on the base, main and the proposal, in order.
using Sides = std::array<std::vector<std::string>, 3>;

/// @brief Tells whether two sides hold the same value (or both none).
/// @param a One side.
/// @param b The other.
/// @return true if they do.
bool same(Side a, Side b) { return a && b ? *a == *b : a == b; }

/// @brief A side's value, to keep.
/// @param side The side.
/// @return Its value, or std::nullopt for none.
std::optional<json> taken(Side side) { return side ? std::optional<json>(*side) : std::nullopt; }

/// @brief An item's id, if it is an object with a string "id".
/// @param item The item.
/// @return The id, or nullptr.
const std::string* idOf(const json& item) {
	if(!item.is_object()) return nullptr;
	const auto it = item.find("id");
	return it != item.end() && it->is_string() ? &it->get_ref<const std::string&>() : nullptr;
}

/// @brief Tells whether a list merges by id: every item an object with a string id, each id once.
/// @param list The value.
/// @return true if it is such a list (an empty list is one).
bool keyed(const json& list) {
	if(!list.is_array()) return false;
	std::unordered_set<std::string> ids;
	return std::ranges::all_of(list, [&](const json& item) {
		const std::string* id = idOf(item);
		return id && ids.insert(*id).second;
	});
}

/// @brief The ids of a keyed list, in order.
/// @param list The list.
/// @return The ids.
std::vector<std::string> idsOf(const json& list) {
	std::vector<std::string> ids;
	for(const json& item : list) ids.push_back(*idOf(item));
	return ids;
}

/// @brief Finds an item of a keyed list.
/// @param list The list.
/// @param id The item's id.
/// @return The item, or nullptr.
Side itemOf(const json& list, const std::string& id) {
	const auto it = std::ranges::find_if(list, [&](const json& item) { return *idOf(item) == id; });
	return it == list.end() ? nullptr : &*it;
}

/// @brief Escapes a key for a JSON Pointer.
/// @param key The key.
/// @return It with '~' as "~0" and '/' as "~1".
std::string escaped(const std::string& key) {
	std::string result;
	for(const char c : key) result += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
	return result;
}

/// @brief Records a conflict.
/// @param conflicts The list.
/// @param path Where.
/// @param reason What happened on each side.
/// @param sides The base, main and the proposal.
void conflict(json& conflicts, const std::string& path, const std::string& reason, Side base, Side main,
              Side proposal) {
	const auto value = [](Side side) { return side ? *side : json(); };
	conflicts.push_back({{"path", path.empty() ? "/" : path},
	                     {"reason", reason},
	                     {"base", value(base)},
	                     {"main", value(main)},
	                     {"proposal", value(proposal)}});
}

/// @brief Why two sides conflict, for a reviewer.
/// @param main main's value.
/// @param proposal The proposal's value.
/// @return e.g. "removed on main, changed in the proposal".
std::string reasonOf(Side main, Side proposal) {
	if(!main) return "removed on main, changed in the proposal";
	if(!proposal) return "changed on main, removed in the proposal";
	return "changed on main and in the proposal";
}

std::optional<json> mergeValue(Side base, Side main, Side proposal, const std::string& path, json& conflicts);

/// @brief Merges three objects key by key: main's keys in order, then the proposal's new ones.
/// @param base The base.
/// @param main main's.
/// @param proposal The proposal's.
/// @param path Where they are.
/// @param conflicts Gets the conflicts.
/// @return The merged object.
json mergeObject(const json& base, const json& main, const json& proposal, const std::string& path, json& conflicts) {
	const auto find = [](const json& object, const std::string& key) -> Side {
		const auto it = object.find(key);
		return it == object.end() ? nullptr : &*it;
	};
	std::vector<std::string> keys;
	for(const auto& [key, value] : main.items()) keys.push_back(key);
	for(const auto& [key, value] : proposal.items())
		if(!main.contains(key)) keys.push_back(key);

	json merged = json::object();
	for(const std::string& key : keys) {
		std::optional<json> value =
		    mergeValue(find(base, key), find(main, key), find(proposal, key), path + "/" + escaped(key), conflicts);
		if(value) merged[key] = std::move(*value);
	}
	return merged;
}

/// @brief Puts the items one side added after the item each followed on that side (at the start if none did).
/// @param order The merged order so far; gets the items.
/// @param side The side's ids, in its order.
/// @param kept The ids the merge keeps.
void placeAdded(std::vector<std::string>& order, const std::vector<std::string>& side,
                const std::unordered_set<std::string>& kept) {
	for(std::size_t i = 0; i < side.size(); ++i) {
		if(!kept.contains(side[i]) || std::ranges::find(order, side[i]) != order.end()) continue;
		auto at = order.begin();
		for(std::size_t before = i; before-- > 0;)
			if(const auto it = std::ranges::find(order, side[before]); it != order.end()) {
				at = it + 1;
				break;
			}
		order.insert(at, side[i]);
	}
}

/// @brief The merged order of a keyed list: the order of the items all three sides keep, three-way merged, then the
/// items main and the proposal added.
/// @param sides The ids of the base, main and the proposal, in order.
/// @param kept The ids the merge keeps.
/// @param path Where the list is.
/// @param conflicts Gets a conflict if both sides reordered differently.
/// @return The ids, in order.
std::vector<std::string> mergeOrder(const Sides& sides, const std::unordered_set<std::string>& kept,
                                    const std::string& path, json& conflicts) {
	Sides common;
	for(std::size_t i = 0; i < 3; ++i)
		std::ranges::copy_if(sides[i], std::back_inserter(common[i]), [&](const std::string& id) {
			return kept.contains(id) && std::ranges::all_of(sides, [&](const std::vector<std::string>& side) {
				       return std::ranges::find(side, id) != side.end();
			       });
		});
	std::vector<std::string> order = common[1];
	if(common[0] == common[1]) order = common[2];
	else if(common[0] != common[2] && common[1] != common[2]) {
		const json base = common[0], main = common[1], proposal = common[2];
		conflict(conflicts, path, "reordered on main and in the proposal", &base, &main, &proposal);
	}
	placeAdded(order, sides[1], kept);
	placeAdded(order, sides[2], kept);
	return order;
}

/// @brief Merges three keyed lists item by item (keyed()).
/// @param base The base.
/// @param main main's.
/// @param proposal The proposal's.
/// @param path Where they are.
/// @param conflicts Gets the conflicts.
/// @return The merged list.
json mergeKeyed(const json& base, const json& main, const json& proposal, const std::string& path, json& conflicts) {
	const Sides sides = {idsOf(base), idsOf(main), idsOf(proposal)};
	std::unordered_map<std::string, json> items;
	std::unordered_set<std::string> seen, kept;
	for(const std::vector<std::string>* side : {&sides[1], &sides[2]})
		for(const std::string& id : *side) {
			if(!seen.insert(id).second) continue;
			std::optional<json> item =
			    mergeValue(itemOf(base, id), itemOf(main, id), itemOf(proposal, id), "#" + id, conflicts);
			if(!item) continue; // removed
			items.emplace(id, std::move(*item));
			kept.insert(id);
		}
	json merged = json::array();
	for(const std::string& id : mergeOrder(sides, kept, path, conflicts)) merged.push_back(std::move(items.at(id)));
	return merged;
}

/// @brief Merges one value.
/// @param base The base's, or nullptr.
/// @param main main's, or nullptr.
/// @param proposal The proposal's, or nullptr.
/// @param path Where it is.
/// @param conflicts Gets the conflicts.
/// @return The merged value, or std::nullopt where the merge removes it.
std::optional<json> mergeValue(Side base, Side main, Side proposal, const std::string& path, json& conflicts) {
	if(same(main, proposal) || same(base, proposal)) return taken(main);
	if(same(base, main)) return taken(proposal);
	if(base && main && proposal) {
		if(base->is_object() && main->is_object() && proposal->is_object())
			return mergeObject(*base, *main, *proposal, path, conflicts);
		if(keyed(*base) && keyed(*main) && keyed(*proposal))
			return mergeKeyed(*base, *main, *proposal, path, conflicts);
	}
	conflict(conflicts, path, reasonOf(main, proposal), base, main, proposal);
	return taken(main);
}

} // namespace

MergeResult merge(const json& base, const json& main, const json& proposal) {
	MergeResult result;
	result.merged = mergeValue(&base, &main, &proposal, "", result.conflicts).value_or(json());
	return result;
}

} // namespace affresco::vcs::detail
