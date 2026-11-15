/// @file
/// @brief Bencode's decode() and encode() (Bencode.hpp). A stub until the wire's package (f2b) fills it: nothing
/// calls it yet.

#include "Bencode.hpp"

namespace affresco::net::bencode {

Decoded decode(std::string_view bytes, const Limits& limits) {
	static_cast<void>(bytes);
	static_cast<void>(limits);
	return {.status = Read::Malformed, .value = {}, .used = 0, .why = "not built yet"};
}

std::string encode(const Value& value) { // NOLINT(modernize-use-string-view): f2b builds the bytes here
	static_cast<void>(value);
	return {};
}

} // namespace affresco::net::bencode
