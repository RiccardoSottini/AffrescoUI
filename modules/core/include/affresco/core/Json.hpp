#pragma once

/// @file
/// @brief The data the document modules exchange: JSON values, document errors and type-checked readers.

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

using json = nlohmann::ordered_json; ///< A JSON value.

/// @brief A malformed document or a refused change.
///
/// what() names the place, e.g. "root.children[1].anchors: unknown edge 'lefft'".
class DocumentError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

/// Deepest nesting accepted from outside the app: far beyond real screens (about 15 levels), shallow enough that the
/// recursive work done on a value (copying, comparing, evaluating) can't overflow the stack.
inline constexpr int maxNesting = 256;

/// @brief Parses JSON that comes from outside the app (peers, revisions, change sets).
/// @param text The JSON text.
/// @param allowComments Whether to skip comments.
/// @return The value.
/// @throws json::parse_error If the text is not JSON.
/// @throws DocumentError If it nests deeper than maxNesting (it is never built), or holds a number too large for a
/// double, such as 1e400.
inline json parseUntrusted(std::string_view text, bool allowComments = false) {
	const auto limit = [](int depth, json::parse_event_t event, json&) {
		// An array or object is reported at the depth outside it: its own level is one deeper
		const bool opens = event == json::parse_event_t::array_start || event == json::parse_event_t::object_start;
		if(depth + (opens ? 1 : 0) > maxNesting)
			throw DocumentError("AffrescoUI screen: JSON nested more than " + std::to_string(maxNesting) + " levels deep");
		return true;
	};
	try {
		return json::parse(text, limit, true, allowComments);
	} catch(const json::out_of_range& error) {
		// Valid JSON that no number can hold: nlohmann::json throws out_of_range, which callers expecting a
		// parse_error or a DocumentError would let through (and a peer could stop the app with it)
		throw DocumentError(std::string("AffrescoUI screen: ") + error.what());
	}
}

/// Deepest a screen's document, or its live state, may become through revisions and steps. maxNesting holds for each
/// message, not for what messages build: a revision can add a value inside one an earlier revision added, and a step
/// can wrap a value in itself. This limit stays below maxNesting so that the few levels a message, a history file or
/// the state file wrap a document in still leave it readable with parseUntrusted().
inline constexpr int maxDocumentNesting = 200;

/// @brief How deeply a value nests, counted the way parseUntrusted() counts: 0 for text or a number, 1 for a list of
/// them, and so on. Walks without recursion, since the value may be deeper than the stack allows.
/// @param value The value.
/// @return Its depth.
[[nodiscard]] inline int nestingDepth(const json& value) {
	if(!value.is_structured()) return 0; // most values written by steps: no walk, no allocation
	int deepest = 0;
	std::vector<std::pair<const json*, int>> pending{{&value, 0}};
	while(!pending.empty()) {
		const auto [item, depth] = pending.back();
		pending.pop_back();
		if(!item->is_structured()) continue;
		deepest = std::max(deepest, depth + 1);
		for(const json& child : *item) pending.emplace_back(&child, depth + 1);
	}
	return deepest;
}

/// @brief How many levels hold the value a JSON Pointer names: one per segment ("/a/0" is held by two).
/// @param pointer The pointer.
/// @return Its number of segments.
[[nodiscard]] inline std::size_t pointerDepth(const json::json_pointer& pointer) {
	const std::string text = pointer.to_string(); // a '/' inside a key is written "~1"
	return static_cast<std::size_t>(std::ranges::count(text, '/'));
}

/// @brief Formats a value as it appears inside text.
/// @param value Any value.
/// @return Strings as they are, null as "", whole numbers without decimals, the rest as JSON.
inline std::string toText(const json& value) {
	if(value.is_string()) return value.get<std::string>();
	if(value.is_number_float()) {
		const double v = value.get<double>();
		if(std::trunc(v) == v && std::abs(v) < 1e15) return std::to_string(static_cast<std::int64_t>(v));
	}
	if(value.is_null()) return "";
	return value.dump();
}

/// @brief Typed reads that throw a DocumentError naming the place in the document.
namespace expect {

/// @brief Reports a problem in a document.
/// @param path Where the problem is (may be empty).
/// @param message What is wrong.
/// @throws DocumentError Always: "AffrescoUI screen: <path>: <message>".
[[noreturn]] inline void fail(const std::string& path, const std::string& message) {
	throw DocumentError("AffrescoUI screen: " + (path.empty() ? message : path + ": " + message));
}

/// @brief Reads an object.
///
/// @p path is a view, not a `const std::string&`: GCC's -Wdangling-reference would take the temporary path string
/// for what the returned reference points into.
/// @param value The value.
/// @param path Its place, for the error.
/// @return @p value.
/// @throws DocumentError If it is not an object.
inline const json& object(const json& value, std::string_view path) {
	if(!value.is_object()) fail(std::string(path), "expected an object");
	return value;
}

/// @brief Reads a number.
/// @param value The value.
/// @param path Its place, for the error.
/// @return The number.
/// @throws DocumentError If it is not a number.
inline float number(const json& value, const std::string& path) {
	if(!value.is_number()) fail(path, "expected a number");
	return value.get<float>();
}

/// @brief Reads an integer.
/// @param value The value.
/// @param path Its place, for the error.
/// @return The integer.
/// @throws DocumentError If it is not an integer.
inline int integer(const json& value, const std::string& path) {
	if(!value.is_number_integer()) fail(path, "expected an integer");
	return value.get<int>();
}

/// @brief Reads a boolean.
/// @param value The value.
/// @param path Its place, for the error.
/// @return The boolean.
/// @throws DocumentError If it is not true or false.
inline bool boolean(const json& value, const std::string& path) {
	if(!value.is_boolean()) fail(path, "expected true or false");
	return value.get<bool>();
}

/// @brief Reads a string.
/// @param value The value.
/// @param path Its place, for the error.
/// @return The string.
/// @throws DocumentError If it is not a string.
inline std::string string(const json& value, const std::string& path) {
	if(!value.is_string()) fail(path, "expected a string");
	return value.get<std::string>();
}

/// @brief Refuses a value whose place would make a document or the live state nest deeper than maxDocumentNesting.
/// @param value The value written.
/// @param levelsAbove How many levels hold it: the segments of its JSON Pointer (see pointerDepth()), one more for
/// an item added to the list there.
/// @param path Its place, for the error.
/// @throws DocumentError If it would.
inline void shallowEnough(const json& value, std::size_t levelsAbove, const std::string& path) {
	if(levelsAbove + static_cast<std::size_t>(nestingDepth(value)) > static_cast<std::size_t>(maxDocumentNesting))
		fail(path, "would nest the state more than " + std::to_string(maxDocumentNesting) + " levels deep");
}

} // namespace expect

} // namespace affresco
