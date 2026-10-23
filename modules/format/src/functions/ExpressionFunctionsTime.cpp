/// @file
/// @brief Expression functions: the clock and times as text (now, time).

#include "ExpressionFunctions.hpp"

#include "affresco/core/Portable.hpp"

#include <array>
#include <chrono>
#include <ctime>
#include <utility>

namespace affresco::format::detail {
namespace {

/// @brief Formats a time like "HH:mm" or "ddd D MMM": YYYY, MM, MMM, DD, D, ddd, HH, mm, ss.
/// @param milliseconds Since 1970, UTC.
/// @param pattern The format.
/// @return The local time as text.
std::string formatTime(double milliseconds, std::string_view pattern) {
	const auto seconds = static_cast<std::time_t>(saturated(milliseconds / 1000.0));
	const std::tm local = localTime(seconds);

	using Field = std::pair<std::string_view, const char*>; // a token, and strftime()'s code for it
	static constexpr std::array<Field, 9> fields{{{"YYYY", "%Y"},
	                                              {"MMM", "%b"},
	                                              {"MM", "%m"},
	                                              {"ddd", "%a"},
	                                              {"DD", "%d"},
	                                              {"D", "%e"},
	                                              {"HH", "%H"},
	                                              {"mm", "%M"},
	                                              {"ss", "%S"}}};
	std::string result;
	for(std::size_t i = 0; i < pattern.size();) {
		bool matched = false;
		for(const auto& [token, code] : fields) {
			if(!pattern.substr(i).starts_with(token)) continue;
			std::array<char, 32> buffer{};
			std::strftime(buffer.data(), buffer.size(), code, &local);
			result += trim(buffer.data());
			i += token.size();
			matched = true;
			break;
		}
		if(!matched) result += pattern[i++];
	}
	return result;
}

/// @brief `now()`.
/// @return Milliseconds since 1970, UTC.
Value now(const Call& /*call*/) {
	return Value::fromInteger(
	    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
	        .count());
}

/// @brief `time(ms, pattern)`.
/// @param call [milliseconds, pattern].
/// @return The local time as text.
Value time(const Call& call) { return Value::fromText(formatTime(call.number(0), call[1].toText())); }

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "now", .arguments = "", .about = "Milliseconds since 1970 (UTC)", .apply = now, .changes = true},
    {.name = "time",
	 .arguments = "milliseconds, 'ddd D MMM HH:mm'",
	 .about = "The local time as text: YYYY, MM, MMM, DD, D, ddd, HH, mm, ss",
	 .fewest = 2,
	 .most = 2,
	 .apply = time},
});

} // namespace

std::span<const Function> timeFunctions() { return table; }

} // namespace affresco::format::detail
