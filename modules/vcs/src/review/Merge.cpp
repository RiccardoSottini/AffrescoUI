/// @file
/// @brief The three-way merge of JSON documents, with lists of nodes keyed by id (Merge.hpp).

#include "review/Merge.hpp"

#include "revisions/Deltas.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
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

/// @brief The items of a keyed list by id, so that a merge finds each at once, not by searching the list.
using Items = std::unordered_map<std::string_view, Side>;

/// @brief Indexes the items of a keyed list.
/// @param list The list; outlives the index.
/// @return Its items by id.
Items itemsOf(const json& list) {
	Items items;
	for(const json& item : list) items.emplace(*idOf(item), &item);
	return items;
}

/// @brief Finds an item of a keyed list.
/// @param items The list's items (itemsOf()).
/// @param id The item's id.
/// @return The item, or nullptr.
Side itemOf(const Items& items, const std::string& id) {
	const auto it = items.find(id);
	return it == items.end() ? nullptr : it->second;
}

/// @brief Records a conflict.
/// @param conflicts The list.
/// @param path Where.
/// @param reason What happened on each side.
/// @param sides The base, main and the proposal.
void conflict(json& conflicts, const std::string& path, std::string_view reason, const std::array<Side, 3>& sides) {
	const auto value = [](Side side) { return side ? *side : json(); };
	conflicts.push_back({{"path", path.empty() ? "/" : path},
	                     {"reason", reason},
	                     {"base", value(sides[0])},
	                     {"main", value(sides[1])},
	                     {"proposal", value(sides[2])}});
}

/// @brief Why two sides conflict, for a reviewer.
/// @param main main's value.
/// @param proposal The proposal's value.
/// @return e.g. "removed on main, changed in the proposal".
std::string_view reasonOf(Side main, Side proposal) {
	if(!main) return "removed on main, changed in the proposal";
	if(!proposal) return "changed on main, removed in the proposal";
	return "changed on main and in the proposal";
}

/// @brief What a merge carries down to each value: the conflicts so far, and how to settle the next.
struct Merging {
	json& conflicts;             ///< The merge's conflicts; outlives this.
	const MergeOptions& options; ///< How conflicts are settled; outlives this.

	/// @brief Tells whether a conflict takes the proposal's value.
	/// @param path The conflict's path.
	/// @return true if the options say so.
	[[nodiscard]] bool takesProposal(const std::string& path) const {
		return options.takeProposal && options.takeProposal(path.empty() ? "/" : path);
	}
};

std::optional<json> mergeValue(Side base, Side main, Side proposal, const std::string& path, Merging& merging);

/// @brief The members of an object by key, so that a merge finds each at once (an ordered_json object searches its
/// keys one by one).
/// @param object The object; outlives the index.
/// @return Its members by key.
Items membersOf(const json& object) {
	Items members;
	members.reserve(object.size());
	for(auto it = object.begin(); it != object.end(); ++it) members.emplace(it.key(), &*it);
	return members;
}

/// @brief Finds a member in an index (membersOf()).
/// @param members The index.
/// @param key The key.
/// @return The member, or nullptr.
Side memberOf(const Items& members, std::string_view key) {
	const auto it = members.find(key);
	return it == members.end() ? nullptr : it->second;
}

/// @brief The merged order of an object's keys: main's, with each key only the proposal added after the key it
/// followed in the proposal (at the start if it followed none that main's order holds).
///
/// A program's definitions are an object's members in the order they are written, so a definition added in the
/// middle of a file stays in the middle. Keys main added, or both sides added, are in main's order already. The cost
/// follows the number of keys: each added key goes into the group after its anchor, not into a list searched again.
/// @param mainKeys main's keys that the merge keeps, in main's order.
/// @param proposal The proposal's object.
/// @param added Whether a key of the proposal is one the merge keeps that main lacks.
/// @return The keys, in order.
std::vector<std::string_view> mergeKeyOrder(const std::vector<std::string_view>& mainKeys, const json& proposal,
                                            const std::unordered_set<std::string_view>& added) {
	std::unordered_map<std::string_view, std::size_t> place; // main's key -> its group of followers (0 is the start)
	place.reserve(mainKeys.size());
	for(std::size_t i = 0; i < mainKeys.size(); ++i) place.emplace(mainKeys[i], i + 1);
	std::vector<std::vector<std::string_view>> followers(mainKeys.size() + 1);
	std::size_t anchor = 0;
	for(auto it = proposal.begin(); it != proposal.end(); ++it) {
		const std::string_view key = it.key();
		if(const auto kept = place.find(key); kept != place.end()) anchor = kept->second;
		else if(added.contains(key)) followers[anchor].push_back(key);
	}
	std::vector<std::string_view> order(followers[0]);
	order.reserve(mainKeys.size() + added.size());
	for(std::size_t i = 0; i < mainKeys.size(); ++i) {
		order.push_back(mainKeys[i]);
		order.insert(order.end(), followers[i + 1].begin(), followers[i + 1].end());
	}
	return order;
}

/// @brief Merges three objects key by key, in main's order, each key only the proposal added after the key it
/// followed there (mergeKeyOrder()).
/// @param base The base.
/// @param main main's.
/// @param proposal The proposal's.
/// @param path Where they are.
/// @param merging Gets the conflicts, and says how to settle them.
/// @return The merged object.
json mergeObject(const json& base, const json& main, const json& proposal, const std::string& path, Merging& merging) {
	const std::array<Items, 3> indexed = {membersOf(base), membersOf(main), membersOf(proposal)};
	std::unordered_map<std::string_view, json> values;
	std::vector<std::string_view> mainKeys;
	std::unordered_set<std::string_view> added;
	const auto mergeKey = [&](std::string_view key) {
		std::optional<json> value = mergeValue(memberOf(indexed[0], key), memberOf(indexed[1], key),
		                                       memberOf(indexed[2], key), path + "/" + escapeKey(key), merging);
		if(value) values.emplace(key, std::move(*value));
		return value.has_value();
	};
	for(auto it = main.begin(); it != main.end(); ++it) // conflicts in this order: main's keys, then the proposal's
		if(mergeKey(it.key())) mainKeys.push_back(it.key());
	for(auto it = proposal.begin(); it != proposal.end(); ++it)
		if(!indexed[1].contains(it.key()) && mergeKey(it.key())) added.insert(it.key());

	json merged = json::object();
	// Each key once, so they are appended as they are, not searched for first
	auto& members = static_cast<std::vector<std::pair<const std::string, json>>&>(merged.get_ref<json::object_t&>());
	members.reserve(values.size());
	for(const std::string_view key : mergeKeyOrder(mainKeys, proposal, added))
		members.emplace_back(std::string(key), std::move(values.at(key)));
	return merged;
}

/// @brief Puts the items one side added after the item each followed on that side (at the start if none did).
/// @param order The merged order so far; gets the items.
/// @param side The side's ids, in its order.
/// @param kept The ids the merge keeps.
void placeAdded(std::vector<std::string>& order, const std::vector<std::string>& side,
                const std::unordered_set<std::string>& kept) {
	std::unordered_set<std::string> placed(order.begin(), order.end()); // what order holds
	for(std::size_t i = 0; i < side.size(); ++i) {
		if(!kept.contains(side[i]) || placed.contains(side[i])) continue;
		auto at = order.begin();
		for(std::size_t before = i; before-- > 0;)
			if(placed.contains(side[before])) {
				at = std::ranges::find(order, side[before]) + 1;
				break;
			}
		order.insert(at, side[i]);
		placed.insert(side[i]);
	}
}

/// @brief The merged order of a keyed list: the order of the items all three sides keep, three-way merged, then the
/// items main and the proposal added.
/// @param sides The ids of the base, main and the proposal, in order.
/// @param kept The ids the merge keeps.
/// @param path Where the list is.
/// @param merging Gets a conflict if both sides reordered differently, and says whose order it takes.
/// @return The ids, in order.
std::vector<std::string> mergeOrder(const Sides& sides, const std::unordered_set<std::string>& kept,
                                    const std::string& path, Merging& merging) {
	const std::array<std::unordered_set<std::string_view>, 3> on = {
	    {{sides[0].begin(), sides[0].end()}, {sides[1].begin(), sides[1].end()}, {sides[2].begin(), sides[2].end()}}};
	Sides common;
	for(std::size_t i = 0; i < 3; ++i)
		std::ranges::copy_if(sides[i], std::back_inserter(common[i]), [&](const std::string& id) {
			return kept.contains(id) && std::ranges::all_of(on, [&](const std::unordered_set<std::string_view>& side) {
				       return side.contains(id);
			       });
		});
	std::vector<std::string> order = common[1];
	if(common[0] == common[1]) order = common[2];
	else if(common[0] != common[2] && common[1] != common[2]) {
		const json base = common[0], main = common[1], proposal = common[2];
		conflict(merging.conflicts, path, "reordered on main and in the proposal", {&base, &main, &proposal});
		if(merging.takesProposal(path)) order = common[2];
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
/// @param merging Gets the conflicts, and says how to settle them.
/// @return The merged list.
json mergeKeyed(const json& base, const json& main, const json& proposal, const std::string& path, Merging& merging) {
	const Sides sides = {idsOf(base), idsOf(main), idsOf(proposal)};
	const std::array<Items, 3> indexed = {itemsOf(base), itemsOf(main), itemsOf(proposal)};
	std::unordered_map<std::string, json> items;
	std::unordered_set<std::string> seen, kept;
	for(const std::vector<std::string>* side : {&sides[1], &sides[2]})
		for(const std::string& id : *side) {
			if(!seen.insert(id).second) continue;
			std::optional<json> item =
			    mergeValue(itemOf(indexed[0], id), itemOf(indexed[1], id), itemOf(indexed[2], id), "#" + id, merging);
			if(!item) continue; // removed
			items.emplace(id, std::move(*item));
			kept.insert(id);
		}
	json merged = json::array();
	for(const std::string& id : mergeOrder(sides, kept, path, merging)) merged.push_back(std::move(items.at(id)));
	return merged;
}

/// @brief Merges three objects key by key, or three keyed lists item by item, where all three are.
/// @param base The base's, or nullptr.
/// @param main main's, or nullptr.
/// @param proposal The proposal's, or nullptr.
/// @param path Where they are.
/// @param merging Gets the conflicts, and says how to settle them.
/// @return The merged value, or std::nullopt if they are not all objects or all keyed lists.
std::optional<json> mergeInside(Side base, Side main, Side proposal, const std::string& path, Merging& merging) {
	if(!base || !main || !proposal) return std::nullopt;
	if(base->is_object() && main->is_object() && proposal->is_object())
		return mergeObject(*base, *main, *proposal, path, merging);
	if(keyed(*base) && keyed(*main) && keyed(*proposal)) return mergeKeyed(*base, *main, *proposal, path, merging);
	return std::nullopt;
}

/// @brief Merges one value.
/// @param base The base's, or nullptr.
/// @param main main's, or nullptr.
/// @param proposal The proposal's, or nullptr.
/// @param path Where it is.
/// @param merging Gets the conflicts, and says how to settle them.
/// @return The merged value, or std::nullopt where the merge removes it.
std::optional<json> mergeValue(Side base, Side main, Side proposal, const std::string& path, Merging& merging) {
	if(same(main, proposal) || same(base, proposal)) return taken(main);
	if(same(base, main)) return taken(proposal);
	if(std::optional<json> merged = mergeInside(base, main, proposal, path, merging)) return merged;
	conflict(merging.conflicts, path, reasonOf(main, proposal), {base, main, proposal});
	return taken(merging.takesProposal(path) ? proposal : main);
}

} // namespace

MergeResult merge(const json& base, const json& main, const json& proposal, const MergeOptions& options) {
	MergeResult result;
	Merging merging{result.conflicts, options};
	result.merged = mergeValue(&base, &main, &proposal, "", merging).value_or(json());
	return result;
}

} // namespace affresco::vcs::detail

namespace affresco::vcs {

MergeResult mergeDocuments(const json& base, const json& main, const json& proposal, const MergeOptions& options) {
	return detail::merge(base, main, proposal, options);
}

} // namespace affresco::vcs
