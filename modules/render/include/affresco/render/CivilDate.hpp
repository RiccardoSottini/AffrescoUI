#pragma once

/// @file
/// @brief CivilDate and TimeOfDay: a day of the proleptic Gregorian calendar and a time within a day, with no time
/// zone, their arithmetic, and their ISO 8601 text ("2026-10-22", "07:30").

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace affresco {

/// @brief A day of the proleptic Gregorian calendar, with no time zone: what a date picker shows and a schedule
/// keeps. Years run from 1 to 9999, as ISO 8601 writes them with four digits.
struct CivilDate {
	int year = 1970; ///< 1 to 9999.
	int month = 1;   ///< 1 (January) to 12.
	int day = 1;     ///< 1 to daysInMonth(year, month).

	/// @brief Orders dates by time.
	auto operator<=>(const CivilDate&) const = default;
};

/// @brief A time within a day, to the second, with no time zone.
struct TimeOfDay {
	int hour = 0;   ///< 0 to 23.
	int minute = 0; ///< 0 to 59.
	int second = 0; ///< 0 to 59.

	/// @brief Orders times within a day.
	auto operator<=>(const TimeOfDay&) const = default;
};

/// @brief The first and last years a CivilDate holds.
inline constexpr int firstYear = 1, lastYear = 9999;

/// @brief Tells whether a year has 29 February.
/// @param year The year.
/// @return True every 4 years, except centuries not divisible by 400.
[[nodiscard]] bool isLeapYear(int year) noexcept;

/// @brief The number of days in a month.
/// @param year The year.
/// @param month 1 to 12.
/// @return 28 to 31; 0 for a month out of range.
[[nodiscard]] int daysInMonth(int year, int month) noexcept;

/// @brief Tells whether a date is in range and its day exists.
/// @param date The date.
/// @return True for a real day from 0001-01-01 to 9999-12-31.
[[nodiscard]] bool isValid(CivilDate date) noexcept;

/// @brief The days from 1970-01-01 to a date.
/// @param date A valid date.
/// @return The count, negative before 1970.
[[nodiscard]] std::int64_t daysSinceEpoch(CivilDate date) noexcept;

/// @brief The date a number of days after 1970-01-01, kept within the years CivilDate holds.
/// @param days The count, negative before 1970.
/// @return The date.
[[nodiscard]] CivilDate dateFromDays(std::int64_t days) noexcept;

/// @brief A date some days later, kept within the years CivilDate holds.
/// @param date A valid date.
/// @param days How many days; negative for earlier.
/// @return The date.
[[nodiscard]] CivilDate addDays(CivilDate date, std::int64_t days) noexcept;

/// @brief A date some months later, its day clamped to the new month's last (31 January plus a month is 28 or 29
/// February), kept within the years CivilDate holds.
/// @param date A valid date.
/// @param months How many months; negative for earlier.
/// @return The date.
[[nodiscard]] CivilDate addMonths(CivilDate date, std::int64_t months) noexcept;

/// @brief The day of the week.
/// @param date A valid date.
/// @return 0 for Sunday to 6 for Saturday.
[[nodiscard]] int weekday(CivilDate date) noexcept;

/// @brief The first day of the week a date is in.
/// @param date A valid date.
/// @param firstDayOfWeek 0 for Sunday to 6 for Saturday (1, Monday, in most of Europe).
/// @return The date, the same or earlier.
[[nodiscard]] CivilDate startOfWeek(CivilDate date, int firstDayOfWeek) noexcept;

/// @brief Reads an ISO 8601 calendar date, "YYYY-MM-DD"; a longer text such as "2026-10-22T07:30" is read up to its
/// date.
/// @param text The text.
/// @return The date, or std::nullopt if the text doesn't start with a valid one.
[[nodiscard]] std::optional<CivilDate> parseIsoDate(std::string_view text) noexcept;

/// @brief Writes a date as ISO 8601, "YYYY-MM-DD".
/// @param date A valid date.
/// @return The text.
[[nodiscard]] std::string isoDate(CivilDate date);

/// @brief Reads a time of day, "HH:MM" or "HH:MM:SS", on the 24-hour clock.
/// @param text The text.
/// @return The time, or std::nullopt if the text isn't one.
[[nodiscard]] std::optional<TimeOfDay> parseTimeOfDay(std::string_view text) noexcept;

/// @brief Writes a time of day, "HH:MM", with ":SS" when the seconds aren't 0.
/// @param time A valid time.
/// @return The text.
[[nodiscard]] std::string isoTime(TimeOfDay time);

/// @brief The seconds from midnight to a time of day.
/// @param time A valid time.
/// @return 0 to 86399.
[[nodiscard]] int secondsSinceMidnight(TimeOfDay time) noexcept;

/// @brief The time of day a number of seconds after midnight, wrapping around the clock.
/// @param seconds The count; any value, negative counting back from midnight.
/// @return The time.
[[nodiscard]] TimeOfDay timeFromSeconds(std::int64_t seconds) noexcept;

} // namespace affresco
