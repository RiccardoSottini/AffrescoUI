#pragma once

// The data the document modules exchange: JSON values (nlohmann/json), errors that name a place in a document, and
// readers that check a value's type on the way in.

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace sgl {

using json = nlohmann::json;

// A malformed document or a refused change. what() names the place, e.g. "root.children[1].anchors: unknown edge
// 'lefft'"
class DocumentError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

// How a value appears inside text: strings as they are, null as nothing, whole numbers without decimals
inline std::string toText(const json& value) {
	if(value.is_string()) return value.get<std::string>();
	if(value.is_number_float()) {
		const double v = value.get<double>();
		if(std::trunc(v) == v && std::abs(v) < 1e15) return std::to_string(static_cast<std::int64_t>(v));
	}
	if(value.is_null()) return "";
	return value.dump();
}

// Typed reads: return the value, or throw a DocumentError naming `path`
namespace expect {

[[noreturn]] inline void fail(const std::string& path, const std::string& message) {
	throw DocumentError("sGLGUI screen: " + (path.empty() ? message : path + ": " + message));
}

inline const json& object(const json& value, const std::string& path) {
	if(!value.is_object()) fail(path, "expected an object");
	return value;
}

inline float number(const json& value, const std::string& path) {
	if(!value.is_number()) fail(path, "expected a number");
	return value.get<float>();
}

inline int integer(const json& value, const std::string& path) {
	if(!value.is_number_integer()) fail(path, "expected an integer");
	return value.get<int>();
}

inline bool boolean(const json& value, const std::string& path) {
	if(!value.is_boolean()) fail(path, "expected true or false");
	return value.get<bool>();
}

inline std::string string(const json& value, const std::string& path) {
	if(!value.is_string()) fail(path, "expected a string");
	return value.get<std::string>();
}

} // namespace expect

} // namespace sgl
