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
#include <unordered_map>
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

namespace detail {

/// @brief Builds a value from the parser's events, as json::parse() does, but finds the keys of a large object in a
/// hash table.
///
/// An ordered_json object searches its keys one by one, so json::parse() takes time in proportion to the square of
/// an object's size: one message of 1 MiB holding an object of 100,000 keys took 8.5 s. A repeated key keeps its
/// first place and takes the last value, as with json::parse(). The member names follow nlohmann::json's SAX
/// interface (json::sax_parse()).
class UntrustedBuilder {
public:
	/// @brief Builds into @p root.
	/// @param root Receives the value.
	explicit UntrustedBuilder(json& root) noexcept : root_(root) {}

	/// @brief A null. @return true, to go on.
	bool null() { return place(json()); }
	/// @brief A boolean. @param value It. @return true, to go on.
	bool boolean(bool value) { return place(json(value)); }
	/// @brief A negative integer. @param value It. @return true, to go on.
	bool number_integer(json::number_integer_t value) { return place(json(value)); }
	/// @brief A positive integer. @param value It. @return true, to go on.
	bool number_unsigned(json::number_unsigned_t value) { return place(json(value)); }
	/// @brief A number with a fraction or an exponent. @param value It. @return true, to go on.
	bool number_float(json::number_float_t value, const json::string_t& /*text*/) { return place(json(value)); }
	/// @brief A string. @param value It (moved from). @return true, to go on.
	bool string(json::string_t& value) { return place(json(std::move(value))); }
	/// @brief Binary data (never in JSON text). @param value It (moved from). @return true, to go on.
	bool binary(json::binary_t& value) {
		json result = json::binary({});
		result.get_binary() = std::move(value);
		return place(std::move(result));
	}
	/// @brief An object starts. @return true, to go on.
	/// @throws DocumentError If it would nest deeper than maxNesting.
	bool start_object(std::size_t /*size*/) { return open(json::object()); }
	/// @brief The key of an object's next member. @param name It. @return true, to go on.
	bool key(json::string_t& name);
	/// @brief An object ends. @return true, to go on.
	bool end_object() {
		open_.pop_back();
		return true;
	}
	/// @brief An array starts. @return true, to go on.
	/// @throws DocumentError If it would nest deeper than maxNesting.
	bool start_array(std::size_t /*size*/) { return open(json::array()); }
	/// @brief An array ends. @return true, to go on.
	bool end_array() {
		open_.pop_back();
		return true;
	}
	/// @brief The text isn't JSON. @param error What the parser found. @return Never: throws @p error.
	template <typename Exception>
	bool parse_error(std::size_t /*position*/, const std::string& /*token*/, const Exception& error) {
		throw error;
	}

private:
	using Members = std::vector<std::pair<const std::string, json>>; ///< What an ordered_json object stores.
	/// Members found by a search one by one; past this many, through the hash table.
	static constexpr std::size_t searchedMembers = 16;

	/// @brief An object or array being built.
	struct Open {
		json* value;                                     ///< It, inside its parent (or the root).
		json* member = nullptr;                          ///< In an object: where the value of the last key goes.
		std::unordered_map<std::string, std::size_t> at; ///< In a large object: each key's place.
	};

	/// @brief Puts a value where the next one goes: the root, the end of an array, or an object's last key.
	/// @param value The value.
	/// @return Where it is now.
	json* put(json value) {
		if(open_.empty()) return &(root_ = std::move(value));
		const Open& parent = open_.back();
		if(parent.value->is_array()) return &parent.value->emplace_back(std::move(value));
		return &(*parent.member = std::move(value));
	}
	/// @brief Puts a value that isn't an object or an array. @param value It. @return true, to go on.
	bool place(json value) {
		put(std::move(value));
		return true;
	}
	/// @brief Puts an empty object or array, and opens it for what goes inside.
	/// @param container The object or array.
	/// @return true, to go on.
	/// @throws DocumentError If it would nest deeper than maxNesting.
	bool open(json container) {
		if(open_.size() + 1 > static_cast<std::size_t>(maxNesting))
			throw DocumentError("AffrescoUI screen: JSON nested more than " + std::to_string(maxNesting) +
			                    " levels deep");
		open_.push_back({put(std::move(container)), nullptr, {}});
		return true;
	}

	json& root_;             ///< See the constructor.
	std::vector<Open> open_; ///< The objects and arrays not yet closed, outermost first.
};

inline bool UntrustedBuilder::key(json::string_t& name) {
	Open& object = open_.back();
	auto& members = static_cast<Members&>(object.value->get_ref<json::object_t&>());
	if(object.at.empty() && members.size() >= searchedMembers)
		for(std::size_t i = 0; i < members.size(); ++i) object.at.emplace(members[i].first, i);

	std::size_t index = members.size();
	if(!object.at.empty()) {
		index = object.at.try_emplace(name, members.size()).first->second;
	} else {
		const auto found = std::ranges::find(members, name, &Members::value_type::first);
		index = static_cast<std::size_t>(found - members.begin());
	}
	if(index == members.size()) members.emplace_back(std::move(name), json()); // the vector's own: no search
	object.member = &members[index].second;
	return true;
}

} // namespace detail

/// @brief Parses JSON that comes from outside the app (peers, revisions, change sets).
///
/// Takes time in proportion to the text, however many keys an object has (see detail::UntrustedBuilder).
/// @param text The JSON text.
/// @param allowComments Whether to skip comments.
/// @return The value.
/// @throws json::parse_error If the text is not JSON.
/// @throws DocumentError If it nests deeper than maxNesting (it is never built), or holds a number too large for a
/// double, such as 1e400.
inline json parseUntrusted(std::string_view text, bool allowComments = false) {
	json result;
	detail::UntrustedBuilder builder(result);
	try {
		json::sax_parse(text, &builder, json::input_format_t::json, true, allowComments);
		return result;
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
