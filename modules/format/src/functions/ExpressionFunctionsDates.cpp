/// @file
/// @brief Expression functions: calendar dates as ISO 8601 texts, "YYYY-MM-DD" (today, addDays, addMonths,
/// daysBetween, weekday, daysInMonth, dateOf, dateMs, isDate). A text that isn't a date gives null.

#include "ExpressionFunctions.hpp"

#include "affresco/core/Portable.hpp"
#include "affresco/render/CivilDate.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <ctime>

namespace affresco::format::detail {
namespace {

constexpr double millisecondsPerDay = 86400000.0; ///< The milliseconds in a day.

/// @brief Reads an argument as a date.
/// @param call The call.
/// @param index The argument's position.
/// @return The date, or std::nullopt if it isn't a text holding one.
std::optional<CivilDate> dateArgument(const Call& call, std::size_t index) {
	if(!call[index].isText()) return std::nullopt;
	return parseIsoDate(call[index].toText());
}

/// @brief Reads an argument as a whole count, rounded toward zero and kept within a few million years.
/// @param call The call.
/// @param index The argument's position.
/// @return The count.
std::int64_t countArgument(const Call& call, std::size_t index) {
	const double value = call.number(index);
	if(!std::isfinite(value)) return 0;
	return static_cast<std::int64_t>(std::clamp(std::trunc(value), -4.0e9, 4.0e9));
}

/// @brief `today()`.
/// @return The local date.
Value today(const Call& /*call*/) {
	const std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	const std::tm local = localTime(seconds);
	return Value::fromText(isoDate({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday}));
}

/// @brief `addDays(date, days)`.
/// @param call [date, days].
/// @return The date, or null.
Value addDaysValue(const Call& call) {
	const std::optional<CivilDate> date = dateArgument(call, 0);
	return date ? Value::fromText(isoDate(addDays(*date, countArgument(call, 1)))) : Value();
}

/// @brief `addMonths(date, months)`.
/// @param call [date, months].
/// @return The date, its day clamped to the month's last, or null.
Value addMonthsValue(const Call& call) {
	const std::optional<CivilDate> date = dateArgument(call, 0);
	return date ? Value::fromText(isoDate(addMonths(*date, countArgument(call, 1)))) : Value();
}

/// @brief `daysBetween(from, to)`.
/// @param call [from, to].
/// @return The days from the first to the second, negative if it is earlier; or null.
Value daysBetween(const Call& call) {
	const std::optional<CivilDate> from = dateArgument(call, 0), to = dateArgument(call, 1);
	return from && to ? Value::fromInteger(daysSinceEpoch(*to) - daysSinceEpoch(*from)) : Value();
}

/// @brief `weekday(date)`.
/// @param call [date].
/// @return 0 for Sunday to 6 for Saturday, or null.
Value weekdayValue(const Call& call) {
	const std::optional<CivilDate> date = dateArgument(call, 0);
	return date ? Value::fromInteger(weekday(*date)) : Value();
}

/// @brief `daysInMonth(date)`.
/// @param call [date].
/// @return 28 to 31, or null.
Value daysInMonthValue(const Call& call) {
	const std::optional<CivilDate> date = dateArgument(call, 0);
	return date ? Value::fromInteger(daysInMonth(date->year, date->month)) : Value();
}

/// @brief `dateOf(ms)`.
/// @param call [milliseconds since 1970, UTC].
/// @return The UTC date it falls on.
Value dateOf(const Call& call) {
	const double milliseconds = call.number(0);
	if(!std::isfinite(milliseconds)) return {};
	const double days = std::floor(std::clamp(milliseconds / millisecondsPerDay, -1.0e7, 1.0e7));
	return Value::fromText(isoDate(dateFromDays(static_cast<std::int64_t>(days))));
}

/// @brief `dateMs(date)`.
/// @param call [date].
/// @return Milliseconds since 1970 at its midnight, UTC (for formatDate with 'utc'), or null.
Value dateMs(const Call& call) {
	const std::optional<CivilDate> date = dateArgument(call, 0);
	return date ? Value::fromInteger(daysSinceEpoch(*date) * 86400000) : Value();
}

/// @brief `isDate(text)`.
/// @param call [text].
/// @return Whether it is an ISO 8601 date, "YYYY-MM-DD".
Value isDate(const Call& call) {
	return Value::fromBoolean(call[0].isText() && call[0].toText().size() == 10 && dateArgument(call, 0));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "today", .arguments = "", .about = "The local date, 'YYYY-MM-DD'", .apply = today, .changes = true},
    {.name = "addDays",
     .arguments = "date, days",
     .about = "The date some days later (earlier if negative)",
     .fewest = 2,
     .most = 2,
     .apply = addDaysValue},
    {.name = "addMonths",
     .arguments = "date, months",
     .about = "The date some months later, its day kept within the month",
     .fewest = 2,
     .most = 2,
     .apply = addMonthsValue},
    {.name = "daysBetween",
     .arguments = "from, to",
     .about = "The days from one date to another",
     .fewest = 2,
     .most = 2,
     .apply = daysBetween},
    {.name = "weekday", .arguments = "date", .about = "0 (Sunday) to 6 (Saturday)", .fewest = 1, .most = 1, .apply = weekdayValue},
    {.name = "daysInMonth",
     .arguments = "date",
     .about = "The days in the date's month",
     .fewest = 1,
     .most = 1,
     .apply = daysInMonthValue},
    {.name = "dateOf",
     .arguments = "milliseconds",
     .about = "The UTC date a time falls on, 'YYYY-MM-DD'",
     .fewest = 1,
     .most = 1,
     .apply = dateOf},
    {.name = "dateMs",
     .arguments = "date",
     .about = "Milliseconds since 1970 at the date's midnight, UTC",
     .fewest = 1,
     .most = 1,
     .apply = dateMs},
    {.name = "isDate", .arguments = "text", .about = "Whether a text is a date, 'YYYY-MM-DD'", .fewest = 1, .most = 1, .apply = isDate},
});

} // namespace

std::span<const Function> dateFunctions() { return table; }

} // namespace affresco::format::detail
