#pragma once

/// @file
/// @brief The data the document modules exchange: JSON values, document errors and type-checked readers.

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace sgl {

using json = nlohmann::json; ///< A JSON value.

/// @brief A malformed document or a refused change.
///
/// what() names the place, e.g. "root.children[1].anchors: unknown edge 'lefft'".
class DocumentError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

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
/// @throws DocumentError Always: "sGLGUI screen: <path>: <message>".
[[noreturn]] inline void fail(const std::string& path, const std::string& message) {
	throw DocumentError("sGLGUI screen: " + (path.empty() ? message : path + ": " + message));
}

/// @brief Reads an object.
/// @param value The value.
/// @param path Its place, for the error.
/// @return @p value.
/// @throws DocumentError If it is not an object.
inline const json& object(const json& value, const std::string& path) {
	if(!value.is_object()) fail(path, "expected an object");
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

} // namespace sgl
