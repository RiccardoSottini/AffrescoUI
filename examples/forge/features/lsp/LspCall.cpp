/// @file
/// @brief The arguments and columns Forge's language server client reads (LspCall.hpp).

#include "features/lsp/LspCall.hpp"

#include <algorithm>
#include <stdexcept>

namespace forge::lsp {

using affresco::json;

/// @brief A text argument of a call.
/// @param with The arguments.
/// @param key The argument.
/// @param bytes At most this long.
/// @return It, or "".
/// @throws std::runtime_error If it is longer.
std::string textArgument(const json& with, const char* key, std::size_t bytes) {
	const json& value = with.is_object() && with.contains(key) ? with[key] : json();
	if(!value.is_string()) return {};
	if(value.get_ref<const std::string&>().size() > bytes) throw std::runtime_error(std::string(key) + " is too long");
	return value.get<std::string>();
}

/// @brief A positive number argument of a call.
/// @param with The arguments.
/// @param key The argument.
/// @return It, or 1 if it isn't one.
std::size_t positionArgument(const json& with, const char* key) {
	const json& value = with.is_object() && with.contains(key) ? with[key] : json();
	return value.is_number_unsigned() && value.get<std::size_t>() > 0
	           ? std::min(value.get<std::size_t>(), std::size_t{1} << 30)
			   : 1;
}

/// @brief UTF-16 units of the first code points of a line.
/// @param line The line.
/// @param columns How many code points.
/// @return Their units.
std::size_t utf16Units(std::u32string_view line, std::size_t columns) {
	std::size_t units = 0;
	for(std::size_t i = 0; i < std::min(columns, line.size()); ++i) units += line[i] > 0xFFFF ? 2 : 1;
	return units + (columns > line.size() ? columns - line.size() : 0);
}

/// @brief The code points of a line that make up a number of UTF-16 units.
/// @param line The line.
/// @param units The units.
/// @return The code points (a unit inside a pair counts its whole code point).
std::size_t codePointsOf(std::u32string_view line, std::size_t units) {
	std::size_t i = 0;
	for(std::size_t at = 0; i < line.size() && at < units; ++i) at += line[i] > 0xFFFF ? 2 : 1;
	return i + (units > utf16Units(line, line.size()) ? units - utf16Units(line, line.size()) : 0);
}

} // namespace forge::lsp
