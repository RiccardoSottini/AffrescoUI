#include "Permissions.hpp"

#include <algorithm>
#include <map>

namespace affresco::vcs::detail {

namespace {

/// Stands in for masked-out parts: equal on both sides, whatever was there.
constexpr const char* masked = "\x01masked";

/// @brief Splits a JSON Pointer into its (still escaped) segments.
/// @param pointer e.g. "/root/children/0".
/// @return e.g. {"root", "children", "0"}; nothing for "".
std::vector<std::string> split(std::string_view pointer) {
	std::vector<std::string> segments;
	std::size_t start = 1;
	while(start <= pointer.size() && !pointer.empty()) {
		const std::size_t slash = pointer.find('/', start);
		segments.emplace_back(
		    pointer.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start));
		if(slash == std::string_view::npos) break;
		start = slash + 1;
	}
	return segments;
}

/// @brief Joins the first segments into a JSON Pointer.
/// @param segments Escaped segments.
/// @param count How many.
/// @return The pointer.
std::string join(const std::vector<std::string>& segments, std::size_t count) {
	std::string pointer;
	for(std::size_t i = 0; i < count; ++i) pointer += "/" + segments[i];
	return pointer;
}

/// @brief Escapes an object key for a JSON Pointer.
/// @param key The key.
/// @return '~' as "~0", '/' as "~1".
std::string escape(std::string_view key) {
	std::string escaped;
	for(const char c : key) {
		if(c == '~') escaped += "~0";
		else if(c == '/') escaped += "~1";
		else escaped += c;
	}
	return escaped;
}

/// @brief One place a pattern covers.
struct Place {
	std::string name;    ///< As the pattern names it: "#id/..." stays "#id/...", so moved nodes compare alike.
	std::string pointer; ///< Where it is in the view.
};

/// @brief Lists the places a pattern covers in a view, expanding "*" (one segment) and "#id" (a node).
/// @param rules Finds "#id" nodes.
/// @param view The view.
/// @param pattern The pattern (empty: the whole view).
/// @return The places; none if a "#id" isn't there.
std::vector<Place> expand(const DocumentRules& rules, const json& view, const std::string& pattern) {
	std::vector<Place> places;
	std::string rest = pattern;
	if(pattern.starts_with('#')) {
		const std::size_t slash = pattern.find('/');
		const std::string id = pattern.substr(1, slash == std::string::npos ? std::string::npos : slash - 1);
		const auto node = rules.findNode(view, id);
		if(!node) return {};
		places.push_back({"#" + id, *node});
		rest = slash == std::string::npos ? "" : pattern.substr(slash);
	} else {
		places.push_back({"", ""});
	}

	for(const std::string& segment : split(rest)) {
		std::vector<Place> next;
		for(const Place& place : places) {
			if(segment != "*") {
				next.push_back({place.name + "/" + segment, place.pointer + "/" + segment});
				continue;
			}
			const json* value = locate(view, place.pointer);
			if(value && value->is_object()) {
				for(const auto& [key, item] : value->items())
					next.push_back({place.name + "/" + escape(key), place.pointer + "/" + escape(key)});
			} else if(value && value->is_array()) {
				for(std::size_t i = 0; i < value->size(); ++i)
					next.push_back({place.name + "/" + std::to_string(i), place.pointer + "/" + std::to_string(i)});
			}
		}
		places = std::move(next);
	}
	return places;
}

/// @brief Masks what patterns cover out of a view: object members are removed, array elements replaced, so the
/// positions of the others don't move.
/// @param rules Finds "#id" nodes.
/// @param view The view.
/// @param patterns The patterns.
/// @return The rest of the view.
json mask(const DocumentRules& rules, json view, const std::vector<std::string>& patterns) {
	std::vector<std::string> pointers;
	for(const std::string& pattern : patterns)
		for(Place& place : expand(rules, view, pattern)) pointers.push_back(std::move(place.pointer));

	for(const std::string& pointer : pointers) {
		if(pointer.empty()) return masked;
		try {
			const json::json_pointer p(pointer);
			if(!view.contains(p)) continue; // absent, or inside something already masked
			json& parent = view[p.parent_pointer()];
			if(parent.is_object()) parent.erase(p.back());
			else view[p] = masked;
		} catch(const json::exception&) {
		}
	}
	return view;
}

/// @brief The id of the node at a pointer, if it is one "#id" would find.
/// @param rules Finds "#id" nodes.
/// @param view The view.
/// @param pointer The pointer.
/// @return The id, or std::nullopt.
std::optional<std::string> nodeIdAt(const DocumentRules& rules, const json& view, const std::string& pointer) {
	const json* value = locate(view, pointer);
	if(!value || !value->is_object() || !value->contains("id") || !(*value)["id"].is_string()) return std::nullopt;
	std::string id = (*value)["id"].get<std::string>();
	if(rules.findNode(view, id) != pointer) return std::nullopt;
	return id;
}

} // namespace

const json* locate(const json& value, const std::string& pointer) {
	try {
		const json::json_pointer p(pointer);
		return value.contains(p) ? &value.at(p) : nullptr;
	} catch(const json::exception&) {
		return nullptr;
	}
}

json view(const json& document, const json& state) {
	json result = document;
	result["state"] = state;
	return result;
}

std::string channelOf(std::string_view author) { return std::string(author.substr(0, author.find('/'))); }

bool authorMatches(std::string_view ruleAuthor, std::string_view author) {
	return ruleAuthor == "*" || ruleAuthor == author || ruleAuthor == channelOf(author);
}

// Compared by name ("#id/..."), so a protected node that only moved (its siblings changed) is unchanged
bool changes(const DocumentRules& rules, const json& before, const json& after, const std::string& pattern) {
	auto contents = [&](const json& view) {
		std::map<std::string, const json*> found;
		for(const Place& place : expand(rules, view, pattern)) found[place.name] = locate(view, place.pointer);
		return found;
	};
	const auto a = contents(before), b = contents(after);
	return !std::ranges::equal(a, b, [](const auto& x, const auto& y) {
		return x.first == y.first && (x.second && y.second ? *x.second == *y.second : x.second == y.second);
	});
}

bool onlyInside(const DocumentRules& rules, const json& before, const json& after,
                const std::vector<std::string>& patterns) {
	return mask(rules, before, patterns) == mask(rules, after, patterns);
}

std::string area(const DocumentRules& rules, const json& view, const std::string& pointer) {
	std::vector<std::string> segments = split(pointer);
	if(segments.empty()) return "/root";

	const json* container = locate(view, join(segments, segments.size() - 1));
	if(segments.back() == "-" || (container && container->is_array())) segments.pop_back();
	if(segments.empty()) return "/root";

	if(segments[0] == "root" || segments[0] == "templates") {
		for(std::size_t n = segments.size(); n > 0; --n) {
			const std::string at = join(segments, n);
			if(at == "/root") break; // the root node names everything
			if(const auto id = nodeIdAt(rules, view, at)) return "#" + *id;
		}
		if(segments[0] == "root") return "/root";
	}
	return join(segments, std::min<std::size_t>(2, segments.size()));
}

bool narrow(std::string_view area) { return area.starts_with('#') || split(area).size() >= 2; }

} // namespace affresco::vcs::detail
