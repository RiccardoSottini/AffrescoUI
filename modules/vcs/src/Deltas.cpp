/// @file
/// @brief Deltas between documents, exact for ordered JSON, and applying them safely.

#include "Deltas.hpp"

#include "Permissions.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace affresco::vcs::detail {

using expect::fail;

namespace {

/// @brief Escapes an object's key for a JSON Pointer: '~' as "~0", '/' as "~1".
/// @param key The key.
/// @return The segment.
std::string escaped(std::string_view key) {
	std::string result;
	result.reserve(key.size());
	for(const char c : key) {
		if(c == '~') result += "~0";
		else if(c == '/') result += "~1";
		else result += c;
	}
	return result;
}

/// @brief Tells whether two values are the same JSON, written the same way: unlike operator==, 1 and 1.0 differ, and
/// so do objects with the same keys in another order (their text, which a commit's id hashes, differs).
/// @param a A value.
/// @param b Another.
/// @return true if they are identical.
bool identical(const json& a, const json& b) {
	if(a.type() != b.type()) return a.is_number_integer() && b.is_number_integer() && a == b; // 5 and 5u write alike
	switch(a.type()) {
	case json::value_t::object:
		if(a.size() != b.size()) return false;
		for(auto x = a.begin(), y = b.begin(); x != a.end(); ++x, ++y)
			if(x.key() != y.key() || !identical(*x, *y)) return false;
		return true;
	case json::value_t::array:
		return a.size() == b.size() &&
		       std::equal(a.begin(), a.end(), b.begin(), [](const json& x, const json& y) { return identical(x, y); });
	case json::value_t::number_float: // bit for bit: 0.0 and -0.0 write differently
		return std::bit_cast<std::uint64_t>(a.get<double>()) == std::bit_cast<std::uint64_t>(b.get<double>());
	default:
		return a == b;
	}
}

void collect(const json& from, const json& to, const std::string& path, json& out);

/// @brief Tells whether patching an object key by key gives the target's order of keys: the keys both have, in the
/// source's order, then the keys only the target has, appended in its order.
/// @param from The object before.
/// @param to The object after.
/// @return true if key-by-key operations rebuild it exactly.
bool keepsOrder(const json& from, const json& to) {
	std::vector<std::string_view> order;
	order.reserve(to.size());
	for(const auto& [key, value] : from.items())
		if(to.contains(key)) order.push_back(key);
	for(const auto& [key, value] : to.items())
		if(!from.contains(key)) order.push_back(key);
	auto key = order.begin();
	for(auto it = to.begin(); it != to.end(); ++it, ++key)
		if(*key != it.key()) return false;
	return true;
}

/// @brief The operations between two objects: key by key, or one replace if that would change the order of keys.
/// @param from The object before.
/// @param to The object after.
/// @param path Its pointer.
/// @param out Receives the operations.
void collectObject(const json& from, const json& to, const std::string& path, json& out) {
	if(!keepsOrder(from, to)) {
		out.push_back({{"op", "replace"}, {"path", path}, {"value", to}});
		return;
	}
	for(const auto& [key, value] : from.items()) {
		const std::string inner = path + "/" + escaped(key);
		if(const auto there = to.find(key); there != to.end()) collect(value, *there, inner, out);
		else out.push_back({{"op", "remove"}, {"path", inner}});
	}
	for(const auto& [key, value] : to.items())
		if(!from.contains(key)) out.push_back({{"op", "add"}, {"path", path + "/" + escaped(key)}, {"value", value}});
}

/// @brief The operations between two lists: their common start and end stay, the items between are compared in pairs,
/// and the rest are removed or added.
/// @param from The list before.
/// @param to The list after.
/// @param path Its pointer.
/// @param out Receives the operations.
void collectList(const json& from, const json& to, const std::string& path, json& out) {
	const std::size_t n = from.size(), m = to.size();
	std::size_t start = 0;
	while(start < n && start < m && identical(from[start], to[start])) ++start;
	std::size_t end = 0;
	while(end < n - start && end < m - start && identical(from[n - 1 - end], to[m - 1 - end])) ++end;

	const std::size_t removed = n - start - end, added = m - start - end, paired = std::min(removed, added);
	for(std::size_t i = 0; i < paired; ++i)
		collect(from[start + i], to[start + i], path + "/" + std::to_string(start + i), out);
	for(std::size_t i = paired; i < removed; ++i) // each removal moves the next one into the same place
		out.push_back({{"op", "remove"}, {"path", path + "/" + std::to_string(start + paired)}});
	for(std::size_t i = paired; i < added; ++i)
		out.push_back({{"op", "add"}, {"path", path + "/" + std::to_string(start + i)}, {"value", to[start + i]}});
}

/// @brief The operations between two values.
/// @param from The value before.
/// @param to The value after.
/// @param path Its pointer.
/// @param out Receives the operations.
void collect(const json& from, const json& to, const std::string& path, json& out) {
	if(from.is_object() && to.is_object()) return collectObject(from, to, path, out);
	if(from.is_array() && to.is_array()) return collectList(from, to, path, out);
	if(!identical(from, to)) out.push_back({{"op", "replace"}, {"path", path}, {"value", to}});
}

} // namespace

json delta(const json& from, const json& to) {
	json out = json::array();
	collect(from, to, "", out);
	return out;
}

void applyOperation(json& document, json operation, const std::string& where) {
	const auto text = [&](const char* key) { return operation.contains(key) && operation[key].is_string(); };
	const std::string kind = text("op") ? operation["op"].get<std::string>() : "";
	if(kind == "move" && text("path") && text("from")) {
		json value = document.at(json::json_pointer(operation["from"].get<std::string>()));
		document.patch_inplace(json::array({{{"op", "remove"}, {"path", operation["from"]}}}));
		operation = {{"op", "add"}, {"path", operation["path"]}, {"value", std::move(value)}};
	}
	if(text("path") && (kind == "add" || kind == "copy" || kind == "move")) {
		const json::json_pointer path(operation["path"].get<std::string>());
		const json* parent = path.empty() ? nullptr : locate(document, path.parent_pointer().to_string());
		if(parent && !parent->is_structured() && !parent->is_null()) // null becomes an object, as in nlohmann::json
			fail(where, "'" + path.parent_pointer().to_string() + "' is a " + parent->type_name() +
			                ": nothing can be added inside it");
	}
	document.patch_inplace(json::array({std::move(operation)}));
}

void applyDelta(json& document, const json& patch, const std::string& where) {
	if(!patch.is_array()) fail(where, "expected a list of operations");
	for(std::size_t i = 0; i < patch.size(); ++i)
		applyOperation(document, patch[i], where + "[" + std::to_string(i) + "]");
}

std::vector<std::string> segmentsOf(std::string_view pointer) {
	std::vector<std::string> segments;
	if(pointer.empty()) return segments;
	if(pointer.front() != '/') fail("pointer", "'" + std::string(pointer) + "' is not a JSON Pointer");
	std::string segment;
	for(std::size_t i = 1; i <= pointer.size(); ++i) {
		if(i == pointer.size() || pointer[i] == '/') {
			segments.push_back(std::move(segment));
			segment.clear();
		} else if(pointer[i] == '~' && i + 1 < pointer.size() && (pointer[i + 1] == '0' || pointer[i + 1] == '1')) {
			segment += pointer[++i] == '0' ? '~' : '/';
		} else {
			segment += pointer[i];
		}
	}
	return segments;
}

std::string pointerOf(const std::vector<std::string>& segments) {
	std::string pointer;
	for(const std::string& segment : segments) pointer += "/" + escaped(segment);
	return pointer;
}

} // namespace affresco::vcs::detail
