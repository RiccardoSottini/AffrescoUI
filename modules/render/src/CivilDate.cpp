/// @file
/// @brief CivilDate and TimeOfDay: calendar arithmetic (Howard Hinnant's days-from-civil algorithms) and ISO 8601
/// text.

#include "affresco/render/CivilDate.hpp"

#include <algorithm>
#include <array>
#include <format>

namespace affresco {

namespace {

constexpr std::int64_t secondsPerDay = 86400; ///< The seconds in a day.

/// @brief Reads a run of decimal digits.
/// @param text The text.
/// @param at Where the digits start; moved past them.
/// @param count How many digits there must be.
/// @return Their value, or std::nullopt if there aren't that many digits.
std::optional<int> digits(std::string_view text, std::size_t& at, std::size_t count) noexcept {
	if(at + count > text.size()) return std::nullopt;
	int value = 0;
	for(std::size_t index = 0; index < count; ++index) {
		const char c = text[at + index];
		if(c < '0' || c > '9') return std::nullopt;
		value = value * 10 + (c - '0');
	}
	at += count;
	return value;
}

/// @brief Reads one character.
/// @param text The text.
/// @param at Where it should be; moved past it.
/// @param expected The character.
/// @return Whether it was there.
bool literal(std::string_view text, std::size_t& at, char expected) noexcept {
	if(at >= text.size() || text[at] != expected) return false;
	++at;
	return true;
}

/// @brief The days from 1970-01-01 to 0001-01-01 and to 9999-12-31, the range CivilDate holds.
/// @return The first and last day counts.
std::pair<std::int64_t, std::int64_t> dayRange() noexcept {
	return {daysSinceEpoch({firstYear, 1, 1}), daysSinceEpoch({lastYear, 12, 31})};
}

} // namespace

bool isLeapYear(int year) noexcept { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

int daysInMonth(int year, int month) noexcept {
	static constexpr std::array<int, 12> lengths = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if(month < 1 || month > 12) return 0;
	return month == 2 && isLeapYear(year) ? 29 : lengths[static_cast<std::size_t>(month - 1)];
}

bool isValid(CivilDate date) noexcept {
	return date.year >= firstYear && date.year <= lastYear && date.day >= 1 &&
	       date.day <= daysInMonth(date.year, date.month);
}

std::int64_t daysSinceEpoch(CivilDate date) noexcept {
	const std::int64_t year = date.month <= 2 ? date.year - 1 : date.year;
	const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
	const std::int64_t yearOfEra = year - era * 400;
	const std::int64_t month = date.month > 2 ? date.month - 3 : date.month + 9;
	const std::int64_t dayOfYear = (153 * month + 2) / 5 + date.day - 1;
	const std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
	return era * 146097 + dayOfEra - 719468;
}

CivilDate dateFromDays(std::int64_t days) noexcept {
	const auto [first, last] = dayRange();
	const std::int64_t shifted = std::clamp(days, first, last) + 719468;
	const std::int64_t era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
	const std::int64_t dayOfEra = shifted - era * 146097;
	const std::int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
	const std::int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
	const std::int64_t monthIndex = (5 * dayOfYear + 2) / 153;
	const auto day = static_cast<int>(dayOfYear - (153 * monthIndex + 2) / 5 + 1);
	const auto month = static_cast<int>(monthIndex < 10 ? monthIndex + 3 : monthIndex - 9);
	const auto year = static_cast<int>(yearOfEra + era * 400 + (month <= 2 ? 1 : 0));
	return {year, month, day};
}

CivilDate addDays(CivilDate date, std::int64_t days) noexcept {
	const auto [first, last] = dayRange();
	const std::int64_t from = daysSinceEpoch(date);
	return dateFromDays(std::clamp(days, first - from, last - from) + from); // no overflow for any count
}

CivilDate addMonths(CivilDate date, std::int64_t months) noexcept {
	constexpr std::int64_t firstMonth = std::int64_t{firstYear} * 12, lastMonth = std::int64_t{lastYear} * 12 + 11;
	const std::int64_t from = std::int64_t{date.year} * 12 + (date.month - 1);
	const std::int64_t to = from + std::clamp(months, firstMonth - from, lastMonth - from);
	const auto year = static_cast<int>(to / 12), month = static_cast<int>(to % 12) + 1;
	return {year, month, std::min(date.day, daysInMonth(year, month))};
}

int weekday(CivilDate date) noexcept {
	const std::int64_t days = daysSinceEpoch(date);
	return static_cast<int>(((days + 4) % 7 + 7) % 7); // 1970-01-01 was a Thursday
}

CivilDate startOfWeek(CivilDate date, int firstDayOfWeek) noexcept {
	const int back = ((weekday(date) - firstDayOfWeek) % 7 + 7) % 7;
	return addDays(date, -back);
}

std::optional<CivilDate> parseIsoDate(std::string_view text) noexcept {
	std::size_t at = 0;
	const std::optional<int> year = digits(text, at, 4);
	if(!year || !literal(text, at, '-')) return std::nullopt;
	const std::optional<int> month = digits(text, at, 2);
	if(!month || !literal(text, at, '-')) return std::nullopt;
	const std::optional<int> day = digits(text, at, 2);
	if(!day) return std::nullopt;
	const CivilDate date{*year, *month, *day};
	if(!isValid(date)) return std::nullopt;
	return date;
}

std::string isoDate(CivilDate date) { return std::format("{:04}-{:02}-{:02}", date.year, date.month, date.day); }

std::optional<TimeOfDay> parseTimeOfDay(std::string_view text) noexcept {
	std::size_t at = 0;
	const std::optional<int> hour = digits(text, at, 2);
	if(!hour || !literal(text, at, ':')) return std::nullopt;
	const std::optional<int> minute = digits(text, at, 2);
	if(!minute) return std::nullopt;
	std::optional<int> second = 0;
	if(literal(text, at, ':')) second = digits(text, at, 2);
	if(!second || at != text.size() || *hour > 23 || *minute > 59 || *second > 59) return std::nullopt;
	return TimeOfDay{*hour, *minute, *second};
}

std::string isoTime(TimeOfDay time) {
	if(time.second == 0) return std::format("{:02}:{:02}", time.hour, time.minute);
	return std::format("{:02}:{:02}:{:02}", time.hour, time.minute, time.second);
}

int secondsSinceMidnight(TimeOfDay time) noexcept { return time.hour * 3600 + time.minute * 60 + time.second; }

TimeOfDay timeFromSeconds(std::int64_t seconds) noexcept {
	const auto wrapped = static_cast<int>((seconds % secondsPerDay + secondsPerDay) % secondsPerDay);
	return {wrapped / 3600, wrapped / 60 % 60, wrapped % 60};
}

} // namespace affresco
