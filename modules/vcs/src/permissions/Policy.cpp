/// @file
/// @brief Budgets and data rules, judged by a change's effect.

#include "permissions/Policy.hpp"

#include "permissions/Permissions.hpp"
#include "revisions/Deltas.hpp"

#include <algorithm>
#include <map>
#include <tuple>
#include <utility>

namespace affresco::vcs::detail {

namespace {

using Segments = std::vector<std::string>; ///< A JSON Pointer's segments, still escaped.

/// @brief Splits a JSON Pointer into its segments.
/// @param pointer e.g. "/state/cards/0".
/// @return e.g. {"state", "cards", "0"}; nothing for "" or "/".
Segments segmentsOf(std::string_view pointer) {
	Segments segments;
	while(pointer.starts_with('/')) {
		pointer.remove_prefix(1);
		const std::size_t slash = pointer.find('/');
		segments.emplace_back(pointer.substr(0, slash));
		if(slash == std::string_view::npos) break;
		pointer.remove_prefix(slash);
	}
	if(segments.size() == 1 && segments[0].empty()) segments.clear();
	return segments;
}

/// @brief Where a path is against a pattern.
enum class Fit {
	None,     ///< Apart.
	Ancestor, ///< The path holds what the pattern names.
	Inside,   ///< The path is what the pattern names, or inside it.
};

/// @brief Compares a path with a pattern, "*" matching any one segment.
/// @param pattern The pattern's segments.
/// @param path The path's segments.
/// @return Where the path is.
Fit fit(const Segments& pattern, const Segments& path) noexcept {
	const std::size_t common = std::min(pattern.size(), path.size());
	for(std::size_t i = 0; i < common; ++i)
		if(pattern[i] != "*" && pattern[i] != path[i]) return Fit::None;
	return pattern.size() <= path.size() ? Fit::Inside : Fit::Ancestor;
}

/// @brief The data rules covering one author, merged.
struct Applied {
	std::vector<std::string> read, write, never; ///< As written; "read" includes the write paths.
	std::vector<std::string> scope;              ///< Top-level sections they limit, escaped: "state" and those named.
	bool hasRead = false;                        ///< Some rule lists "read" paths.
	bool hasWrite = false;                       ///< Some rule lists "write" paths.
};

/// @brief Merges the data rules covering an author.
/// @param policy The policy.
/// @param author The author.
/// @param roles Who plays which role.
/// @return The merged rules.
Applied appliedTo(const Policy& policy, std::string_view author, const Roles& roles) {
	Applied applied;
	applied.scope.emplace_back("state");
	for(const DataRule& rule : policy.data) {
		if(!ruleCovers(rule.author, author, roles)) continue;
		applied.hasRead = applied.hasRead || !rule.read.empty();
		applied.hasWrite = applied.hasWrite || !rule.write.empty();
		applied.read.insert(applied.read.end(), rule.read.begin(), rule.read.end());
		applied.read.insert(applied.read.end(), rule.write.begin(), rule.write.end());
		applied.write.insert(applied.write.end(), rule.write.begin(), rule.write.end());
		applied.never.insert(applied.never.end(), rule.never.begin(), rule.never.end());
	}
	for(const auto* list : {&applied.read, &applied.never})
		for(const std::string& pattern : *list)
			if(const Segments s = segmentsOf(pattern);
			   !s.empty() && std::ranges::find(applied.scope, s[0]) == applied.scope.end())
				applied.scope.push_back(s[0]);
	return applied;
}

/// @brief Splits each pattern.
/// @param patterns The patterns.
/// @return Their segments.
std::vector<Segments> segmentsOf(const std::vector<std::string>& patterns) {
	std::vector<Segments> all;
	all.reserve(patterns.size());
	for(const std::string& pattern : patterns) all.push_back(segmentsOf(pattern));
	return all;
}

/// @brief The sections of a view a scope names, the rest left out.
/// @param view The view.
/// @param scope Top-level keys, escaped.
/// @return The sections.
json scoped(const json& view, const std::vector<std::string>& scope) {
	json result = json::object();
	for(const std::string& section : scope)
		if(const json* value = locate(view, "/" + section)) result[json::json_pointer("/" + section)] = *value;
	return result;
}

/// @brief What patterns leave readable of a value.
struct Filter {
	const std::vector<Segments>* keep; ///< Only these (and what they hold); nullptr: everything.
	const std::vector<Segments>& drop; ///< Never these.
};

/// @brief How a filter covers a path.
struct Cover {
	bool whole = false;     ///< Kept whole: a kept path holds it, or nothing limits what is kept.
	bool partial = false;   ///< Kept in part: it holds a kept path.
	bool dropBelow = false; ///< It holds a dropped path.
};

/// @brief How a filter covers a path.
/// @param path The path.
/// @param filter What to keep and drop.
/// @return How, or std::nullopt if a dropped path holds it.
std::optional<Cover> coverOf(const Segments& path, const Filter& filter) {
	Cover cover{.whole = filter.keep == nullptr};
	for(const Segments& drop : filter.drop) {
		const Fit f = fit(drop, path);
		if(f == Fit::Inside) return std::nullopt;
		cover.dropBelow = cover.dropBelow || f == Fit::Ancestor;
	}
	if(filter.keep)
		for(const Segments& keep : *filter.keep) {
			const Fit f = fit(keep, path);
			cover.whole = cover.whole || f == Fit::Inside;
			cover.partial = cover.partial || f == Fit::Ancestor;
		}
	return cover;
}

std::optional<json> filtered(const json& value, Segments& path, const Filter& filter);

/// @brief Filters what an array or an object holds.
/// @param value The array or the object.
/// @param path Its path, extended while descending.
/// @param filter What to keep and drop.
/// @return What is readable of it: array elements read as null, so positions stay; unreadable keys are left out.
json filteredItems(const json& value, Segments& path, const Filter& filter) {
	json result = value.is_array() ? json::array() : json::object();
	if(value.is_array()) {
		for(std::size_t i = 0; i < value.size(); ++i) {
			path.push_back(std::to_string(i));
			result.push_back(filtered(value[i], path, filter).value_or(json()));
			path.pop_back();
		}
		return result;
	}
	for(const auto& [key, item] : value.items()) {
		path.push_back(escapeKey(key));
		if(std::optional<json> kept = filtered(item, path, filter)) result[key] = std::move(*kept);
		path.pop_back();
	}
	return result;
}

/// @brief Filters a value at a path.
/// @param value The value.
/// @param path Its path, extended while descending.
/// @param filter What to keep and drop.
/// @return What is readable of it, or std::nullopt if none is.
std::optional<json> filtered(const json& value, Segments& path, const Filter& filter) {
	const std::optional<Cover> cover = coverOf(path, filter);
	if(!cover || (!cover->whole && !cover->partial)) return std::nullopt;
	if(cover->whole && !cover->dropBelow) return value;
	if(!value.is_structured()) return cover->whole ? std::optional<json>(value) : std::nullopt;
	return filteredItems(value, path, filter);
}

/// @brief Tells whether an author's "read" paths limit a top-level section.
/// @param applied The author's data rules.
/// @param section The section, escaped.
/// @return true if only its "read" paths of it are readable.
bool readLimitedIn(const Applied& applied, const std::string& section) {
	return applied.hasRead && std::ranges::find(applied.scope, section) != applied.scope.end();
}

/// @brief What an author may read, ready to check paths against.
struct ReadLimits {
	const Applied& applied;     ///< The author's data rules.
	std::vector<Segments> keep; ///< Its "read" paths, split.
	std::vector<Segments> drop; ///< Its "never" paths, split, in the order of applied.never.
};

/// @brief The path an operation names, as it is in a view: a node's ("#id/rest") is where the node is now.
/// @param rules Finds "#id" nodes.
/// @param view The view.
/// @param operation The operation.
/// @param key "path" or "from".
/// @return The JSON Pointer, or std::nullopt if it names none or names a node that isn't there (applying it fails,
/// saying nothing about readable parts).
std::optional<std::string> namedPath(const DocumentRules& rules, const json& view, const json& operation,
                                     const char* key) {
	if(!operation.is_object() || !operation.contains(key) || !operation[key].is_string()) return std::nullopt;
	std::string path = operation[key].get<std::string>();
	if(!path.starts_with('#')) return path;
	const std::size_t slash = path.find('/');
	const auto node = rules.findNode(view, path.substr(1, slash == std::string::npos ? slash : slash - 1));
	if(!node) return std::nullopt;
	return *node + (slash == std::string::npos ? "" : path.substr(slash));
}

/// @brief What of a path an author may not read.
/// @param limits What it may read.
/// @param path The path.
/// @return The path itself, the "never" pattern that covers it, or "the whole screen"; std::nullopt if it may.
std::optional<std::string> unreadable(const ReadLimits& limits, const std::string& path) {
	const Segments segments = segmentsOf(path);
	if(segments.empty()) return "the whole screen";
	for(std::size_t i = 0; i < limits.drop.size(); ++i)
		if(fit(limits.drop[i], segments) != Fit::None) return limits.applied.never[i];
	if(readLimitedIn(limits.applied, segments[0]) &&
	   std::ranges::none_of(limits.keep, [&](const Segments& k) { return fit(k, segments) == Fit::Inside; }))
		return path;
	return std::nullopt;
}

/// @brief Collects the nodes under one: by id, or by place for a node with no id (or the id of one seen before), so
/// that nodes without ids count against a budget too.
/// @param node The node.
/// @param at Its JSON Pointer.
/// @param into Gets "#id" or the pointer -> node.
void collectNodes(const json& node, const std::string& at, std::map<std::string, const json*>& into) {
	if(!node.is_object()) return;
	const auto id = node.find("id");
	if(id == node.end() || !id->is_string() || !into.emplace("#" + id->get<std::string>(), &node).second)
		into.emplace(at, &node);
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(std::size_t i = 0; i < children->size(); ++i)
			collectNodes((*children)[i], at + "/children/" + std::to_string(i), into);
}

/// @brief The nodes of a view, by id (or place).
/// @param view The view.
/// @return "#id" or JSON Pointer -> node.
std::map<std::string, const json*> nodesOf(const json& view) {
	std::map<std::string, const json*> nodes;
	if(const json* root = locate(view, "/root")) collectNodes(*root, "/root", nodes);
	for(const char* section : {"/screens", "/templates"})
		if(const json* all = locate(view, section); all && all->is_object())
			for(const auto& [name, node] : all->items())
				collectNodes(node, std::string(section) + "/" + escapeKey(name), nodes);
	return nodes;
}

/// @brief Tells whether two nodes have the same own keys (all but "children").
/// @param a One node.
/// @param b The other.
/// @return true if they do.
bool sameOwnKeys(const json& a, const json& b) {
	std::size_t count = 0;
	for(const auto& [key, value] : a.items()) {
		if(key == "children") continue;
		const auto other = b.find(key);
		if(other == b.end() || *other != value) return false;
		++count;
	}
	return count == b.size() - (b.contains("children") ? 1 : 0);
}

/// @brief A budget as a reviewer reads it.
/// @param budget The budget.
/// @return e.g. "agent: 20 added, 5 removed, 50 changed".
std::string describe(const ChangeBudget& budget) {
	std::string text = budget.author + ":";
	const char* separator = " ";
	for(const auto& [limit, name] : {std::pair{budget.added, "added"}, std::pair{budget.removed, "removed"},
	                                 std::pair{budget.changed, "changed"}}) {
		if(limit < 0) continue;
		text += separator + std::to_string(limit) + " " + name;
		separator = ", ";
	}
	return text;
}

/// @brief Reads a list of strings from a JSON value.
/// @param value The value (may be null).
/// @return The strings in it.
std::vector<std::string> stringsOf(const json& value) {
	std::vector<std::string> strings;
	if(value.is_array())
		for(const json& item : value)
			if(item.is_string()) strings.push_back(item.get<std::string>());
	return strings;
}

/// @brief A budget's limit.
/// @param item The budget, an object.
/// @param key "added", "removed" or "changed".
/// @return The limit, or -1 for none.
long long limitOf(const json& item, const char* key) {
	return item.contains(key) && item[key].is_number_integer() ? item[key].get<long long>() : -1;
}

/// @brief Tells whether a budget or a data rule names its author, so it can apply to someone.
/// @param item The budget or the rule.
/// @return true if it is an object with an "author" string.
bool namesAuthor(const json& item) { return item.is_object() && item.contains("author") && item["author"].is_string(); }

} // namespace

Policy policyOf(const json& document) {
	Policy policy;
	const json* permissions = locate(document, "/permissions");
	if(!permissions || !permissions->is_object()) return policy;
	for(const json& item : permissions->value("budgets", json::array())) {
		if(!namesAuthor(item)) continue;
		policy.budgets.push_back({item["author"].get<std::string>(), limitOf(item, "added"), limitOf(item, "removed"),
		                          limitOf(item, "changed"), item.value("over", json("deny")) != "ask"});
	}
	for(const json& item : permissions->value("data", json::array())) {
		if(!namesAuthor(item)) continue;
		policy.data.push_back({item["author"].get<std::string>(), stringsOf(item.value("read", json())),
		                       stringsOf(item.value("write", json())), stringsOf(item.value("never", json()))});
	}
	return policy;
}

NodeCounts countNodes(const json& before, const json& after) {
	const auto a = nodesOf(before), b = nodesOf(after);
	NodeCounts counts;
	for(const auto& [id, node] : a) {
		const auto other = b.find(id);
		if(other == b.end()) ++counts.removed;
		else if(!sameOwnKeys(*node, *other->second)) ++counts.changed;
	}
	for(const auto& [id, node] : b)
		if(!a.contains(id)) ++counts.added;
	return counts;
}

std::optional<PolicyVerdict> overBudget(const Policy& policy, const json& before, const json& after,
                                        std::string_view author, const Roles& roles) {
	std::optional<NodeCounts> counts; // counted once, only if a budget covers the author
	for(const ChangeBudget& budget : policy.budgets) {
		if(!ruleCovers(budget.author, author, roles)) continue;
		if(!counts) counts = countNodes(before, after);
		for(const auto& [count, limit, name] : {std::tuple{counts->added, budget.added, "new nodes"},
		                                        std::tuple{counts->removed, budget.removed, "removals"},
		                                        std::tuple{counts->changed, budget.changed, "changed nodes"}})
			if(limit >= 0 && std::cmp_greater(count, limit))
				return PolicyVerdict{budget.deny, "over the budget \"" + describe(budget) +
				                                      "\": " + std::to_string(count) + " " + name + ", at most " +
				                                      std::to_string(limit)};
	}
	return std::nullopt;
}

std::optional<std::string> dataDenial(const DocumentRules& rules, const Policy& policy, const json& before,
                                      const json& after, const Actor& actor) {
	if(policy.data.empty()) return std::nullopt;
	const std::string_view author = actor.author;
	const Applied applied = appliedTo(policy, author, actor.roles);
	for(const std::string& pattern : applied.never)
		if(changes(rules, before, after, pattern))
			return "data rule: '" + std::string(author) + "' may never touch " + pattern;
	if(applied.hasWrite &&
	   !onlyInside(rules, scoped(before, applied.scope), scoped(after, applied.scope), applied.write)) {
		std::string list;
		for(const std::string& pattern : applied.write) list += (list.empty() ? "" : ", ") + pattern;
		return "data rule: '" + std::string(author) + "' may write only " + list;
	}
	return std::nullopt;
}

bool readLimited(const Policy& policy, std::string_view author, const Roles& roles) {
	return std::ranges::any_of(policy.data, [&](const DataRule& rule) {
		return (!rule.read.empty() || !rule.never.empty()) && ruleCovers(rule.author, author, roles);
	});
}

json readableView(const Policy& policy, const json& view, std::string_view author, const Roles& roles) {
	if(!readLimited(policy, author, roles) || !view.is_object()) return view;
	const Applied applied = appliedTo(policy, author, roles);
	const std::vector<Segments> keep = segmentsOf(applied.read), drop = segmentsOf(applied.never);
	json result = json::object();
	for(const auto& [key, value] : view.items()) {
		Segments path{escapeKey(key)};
		const bool limited = readLimitedIn(applied, path[0]);
		if(std::optional<json> kept = filtered(value, path, {limited ? &keep : nullptr, drop}))
			result[key] = std::move(*kept);
	}
	return result;
}

std::optional<std::string> unreadablePath(const DocumentRules& rules, const Policy& policy, const json& view,
                                          const json& patch, const Actor& actor) {
	if(!patch.is_array() || !readLimited(policy, actor.author, actor.roles)) return std::nullopt;
	const Applied applied = appliedTo(policy, actor.author, actor.roles);
	const ReadLimits limits{applied, segmentsOf(applied.read), segmentsOf(applied.never)};
	for(const json& operation : patch)
		for(const char* key : {"path", "from"}) {
			const std::optional<std::string> path = namedPath(rules, view, operation, key);
			if(!path) continue;
			if(const auto why = unreadable(limits, *path))
				return "data rule: '" + std::string(actor.author) + "' may not read " + *why;
		}
	return std::nullopt;
}

} // namespace affresco::vcs::detail
