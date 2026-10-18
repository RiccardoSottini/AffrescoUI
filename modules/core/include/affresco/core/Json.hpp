#pragma once

/// @file
/// @brief The data the document modules exchange: JSON values, document errors and type-checked readers.

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

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
/// @throws DocumentError If it nests deeper than maxNesting (it is never built).
inline json parseUntrusted(std::string_view text, bool allowComments = false) {
	const auto limit = [](int depth, json::parse_event_t event, json&) {
		// An array or object is reported at the depth outside it: its own level is one deeper
		const bool opens = event == json::parse_event_t::array_start || event == json::parse_event_t::object_start;
		if(depth + (opens ? 1 : 0) > maxNesting)
			throw DocumentError("AffrescoUI screen: JSON nested more than " + std::to_string(maxNesting) + " levels deep");
		return true;
	};
	return json::parse(text, limit, true, allowComments);
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

} // namespace expect

} // namespace affresco
