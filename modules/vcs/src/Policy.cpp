/// @file
/// @brief Budgets and data rules, judged by a change's effect.

#include "Policy.hpp"

#include "Permissions.hpp"

#include <algorithm>
#include <map>
#include <tuple>

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

/// @brief Escapes an object key for a JSON Pointer.
/// @param key The key.
/// @return '~' as "~0", '/' as "~1".
std::string escape(std::string_view key) {
	std::string escaped;
	for(const char c : key) escaped += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
	return escaped;
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

/// @brief Filters a value at a path.
/// @param value The value.
/// @param path Its path, extended while descending.
/// @param filter What to keep and drop.
/// @return What is readable of it, or std::nullopt if none is.
std::optional<json> filtered(const json& value, Segments& path, const Filter& filter) {
	bool dropBelow = false, whole = filter.keep == nullptr, partial = false;
	for(const Segments& drop : filter.drop) {
		const Fit f = fit(drop, path);
		if(f == Fit::Inside) return std::nullopt;
		dropBelow = dropBelow || f == Fit::Ancestor;
	}
	if(filter.keep)
		for(const Segments& keep : *filter.keep) {
			const Fit f = fit(keep, path);
			whole = whole || f == Fit::Inside;
			partial = partial || f == Fit::Ancestor;
		}
	if(!whole && !partial) return std::nullopt;
	if(whole && !dropBelow) return value;
	if(!value.is_structured()) return whole ? std::optional<json>(value) : std::nullopt;

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
		path.push_back(escape(key));
		if(std::optional<json> kept = filtered(item, path, filter)) result[key] = std::move(*kept);
		path.pop_back();
	}
	return result;
}

/// @brief Collects the nodes under one, by id.
/// @param node The node.
/// @param into Gets id -> node; the first of an id is kept.
void collectNodes(const json& node, std::map<std::string, const json*>& into) {
	if(!node.is_object()) return;
	if(const auto id = node.find("id"); id != node.end() && id->is_string())
		into.emplace(id->get<std::string>(), &node);
	if(const auto children = node.find("children"); children != node.end() && children->is_array())
		for(const json& child : *children) collectNodes(child, into);
}

/// @brief The nodes of a view, by id.
/// @param view The view.
/// @return id -> node.
std::map<std::string, const json*> nodesOf(const json& view) {
	std::map<std::string, const json*> nodes;
	if(const json* root = locate(view, "/root")) collectNodes(*root, nodes);
	for(const char* section : {"/screens", "/templates"})
		if(const json* all = locate(view, section); all && all->is_object())
			for(const auto& [name, node] : all->items()) collectNodes(node, nodes);
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

} // namespace

Policy policyOf(const json& document) {
	Policy policy;
	const json* permissions = locate(document, "/permissions");
	if(!permissions || !permissions->is_object()) return policy;
	const auto limit = [](const json& item, const char* key) {
		return item.contains(key) && item[key].is_number_integer() ? item[key].get<long long>() : -1;
	};
	for(const json& item : permissions->value("budgets", json::array())) {
		if(!item.is_object() || !item.contains("author") || !item["author"].is_string()) continue;
		policy.budgets.push_back({item["author"].get<std::string>(), limit(item, "added"), limit(item, "removed"),
		                          limit(item, "changed"), item.value("over", json("deny")) != "ask"});
	}
	for(const json& item : permissions->value("data", json::array())) {
		if(!item.is_object() || !item.contains("author") || !item["author"].is_string()) continue;
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
			if(limit >= 0 && count > static_cast<std::size_t>(limit))
				return PolicyVerdict{budget.deny, "over the budget \"" + describe(budget) +
				                                      "\": " + std::to_string(count) + " " + name + ", at most " +
				                                      std::to_string(limit)};
	}
	return std::nullopt;
}

std::optional<std::string> dataDenial(const DocumentRules& rules, const Policy& policy, const json& before,
                                      const json& after, std::string_view author, const Roles& roles) {
	if(policy.data.empty()) return std::nullopt;
	const Applied applied = appliedTo(policy, author, roles);
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
		Segments path{escape(key)};
		const bool limited = applied.hasRead && std::ranges::find(applied.scope, path[0]) != applied.scope.end();
		if(std::optional<json> kept = filtered(value, path, {limited ? &keep : nullptr, drop}))
			result[key] = std::move(*kept);
	}
	return result;
}

std::optional<std::string> unreadablePath(const DocumentRules& rules, const Policy& policy, const json& view,
                                          const json& patch, std::string_view author, const Roles& roles) {
	if(!patch.is_array() || !readLimited(policy, author, roles)) return std::nullopt;
	const Applied applied = appliedTo(policy, author, roles);
	const std::vector<Segments> keep = segmentsOf(applied.read), drop = segmentsOf(applied.never);
	const std::string who = "data rule: '" + std::string(author) + "' may not read ";
	for(const json& operation : patch) {
		for(const char* key : {"path", "from"}) {
			if(!operation.is_object() || !operation.contains(key) || !operation[key].is_string()) continue;
			std::string path = operation[key].get<std::string>();
			if(path.starts_with('#')) { // a node: where it is now
				const std::size_t slash = path.find('/');
				const auto node = rules.findNode(view, path.substr(1, slash == std::string::npos ? slash : slash - 1));
				if(!node) continue; // applying it fails, saying nothing about readable parts
				path = *node + (slash == std::string::npos ? "" : path.substr(slash));
			}
			const Segments segments = segmentsOf(path);
			if(segments.empty()) return who + "the whole screen";
			for(std::size_t i = 0; i < drop.size(); ++i)
				if(fit(drop[i], segments) != Fit::None) return who + applied.never[i];
			const bool limited =
			    applied.hasRead && std::ranges::find(applied.scope, segments[0]) != applied.scope.end();
			if(limited &&
			   std::ranges::none_of(keep, [&](const Segments& k) { return fit(k, segments) == Fit::Inside; }))
				return who + path;
		}
	}
	return std::nullopt;
}

} // namespace affresco::vcs::detail
