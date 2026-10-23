/// @file
/// @brief Locales: dates and times by CLDR patterns ("EEEE d MMMM y", "h:mm a"), and relative times ("in 3 days").

#include "Locale.hpp"

#include "affresco/core/Portable.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <format>

namespace affresco::format::detail {
namespace {

/// @brief A time broken down, with what patterns read of it.
struct Moment {
	std::tm fields{};       ///< Year, month, day, hours, ... in the chosen time zone.
	int milliseconds = 0;   ///< Below the second.
	bool universal = false; ///< In UTC.
};

/// @brief Breaks a time down.
/// @param milliseconds Since 1970, UTC.
/// @param universal In UTC rather than the computer's time zone.
/// @return It.
Moment momentOf(double milliseconds, bool universal) noexcept {
	Moment moment;
	moment.universal = universal;
	if(!std::isfinite(milliseconds) || std::fabs(milliseconds) > 8.64e18) return moment;
	const double seconds = std::floor(milliseconds / 1000.0);
	moment.milliseconds = static_cast<int>(milliseconds - seconds * 1000.0);
	const auto time = static_cast<std::time_t>(seconds);
	moment.fields = universal ? utcTime(time) : localTime(time);
	return moment;
}

/// @brief A number padded with zeros.
/// @param value The number.
/// @param width Its fewest digits.
/// @return The text.
std::string padded(int value, std::size_t width) {
	std::string text = std::to_string(value);
	if(text.size() < width) text.insert(0, width - text.size(), '0');
	return text;
}

/// @brief The style's position in LocaleData's arrays.
/// @param style "full", "long", "medium" or "short".
/// @return 0 to 3, or -1 for a pattern.
int styleIndex(std::string_view style) noexcept {
	static constexpr std::array<std::string_view, 4> styles{"full", "long", "medium", "short"};
	for(std::size_t i = 0; i < styles.size(); ++i)
		if(styles[i] == style) return static_cast<int>(i);
	return -1;
}

/// @brief Writes a date field of a pattern: the year, the month, the day or the weekday.
/// @param out Where.
/// @param letter The letter.
/// @param count How many times it is repeated.
/// @param time The time's fields.
/// @param locale The locale.
/// @return true if the letter is a date field.
bool appendDateField(std::string& out, char letter, std::size_t count, const std::tm& time, const LocaleData& locale) {
	switch(letter) {
	case 'y':
		out += count == 2 ? padded((time.tm_year + 1900) % 100, 2) : padded(time.tm_year + 1900, count);
		return true;
	case 'M':
	case 'L':
		if(count >= 4) out += locale.months[static_cast<std::size_t>(time.tm_mon) % 12];
		else if(count == 3) out += locale.shortMonths[static_cast<std::size_t>(time.tm_mon) % 12];
		else out += padded(time.tm_mon + 1, count);
		return true;
	case 'd':
		out += padded(time.tm_mday, count);
		return true;
	case 'E':
	case 'c':
		out += (count >= 4 ? locale.days : locale.shortDays)[static_cast<std::size_t>(time.tm_wday) % 7];
		return true;
	default:
		return false;
	}
}

/// @brief Writes an hour field of a pattern: the hour on a 12 or 24 hour clock, or the period (AM or PM).
/// @param out Where.
/// @param letter The letter.
/// @param count How many times it is repeated.
/// @param hour The hour, 0 to 23.
/// @param locale The locale.
/// @return true if the letter is an hour field.
bool appendHourField(std::string& out, char letter, std::size_t count, int hour, const LocaleData& locale) {
	switch(letter) {
	case 'a':
	case 'b':
	case 'B':
		out += hour < 12 ? locale.am : locale.pm;
		return true;
	case 'h':
		out += padded(hour % 12 == 0 ? 12 : hour % 12, count);
		return true;
	case 'H':
		out += padded(hour, count);
		return true;
	case 'k':
		out += padded(hour == 0 ? 24 : hour, count);
		return true;
	case 'K':
		out += padded(hour % 12, count);
		return true;
	default:
		return false;
	}
}

/// @brief Writes one field of a pattern: a run of the same letter.
/// @param out Where.
/// @param letter The letter.
/// @param count How many times it is repeated.
/// @param moment The time.
/// @param locale The locale.
void appendField(std::string& out, char letter, std::size_t count, const Moment& moment, const LocaleData& locale) {
	const std::tm& time = moment.fields;
	if(appendDateField(out, letter, count, time, locale) || appendHourField(out, letter, count, time.tm_hour, locale))
		return;
	switch(letter) {
	case 'm':
		out += padded(time.tm_min, count);
		break;
	case 's':
		out += padded(time.tm_sec, count);
		break;
	case 'S':
		out += padded(moment.milliseconds, 3).substr(0, std::min<std::size_t>(count, 3));
		break;
	case 'z':
	case 'Z':
	case 'v':
	case 'V':
	case 'O': {
		std::array<char, 64> zone{};
		std::strftime(zone.data(), zone.size(), "%Z", &time);
		out += moment.universal ? "UTC" : zone.data();
		break;
	}
	default:
		break; // eras, quarters, weeks: not in the subset's patterns
	}
}

/// @brief Formats a time by a CLDR pattern.
/// @param pattern The pattern: letters are fields, text between quotes is written as is ('' is a quote).
/// @param moment The time.
/// @param locale The locale.
/// @return The text.
std::string byPattern(std::string_view pattern, const Moment& moment, const LocaleData& locale) {
	std::string out;
	for(std::size_t i = 0; i < pattern.size();) {
		const char character = pattern[i];
		if(character == '\'') {
			const std::size_t end = pattern.find('\'', i + 1);
			if(end == i + 1) out += '\'';
			else out += pattern.substr(i + 1, end == std::string_view::npos ? end : end - i - 1);
			i = end == std::string_view::npos ? pattern.size() : end + 1;
		} else if((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z')) {
			std::size_t count = 1;
			while(i + count < pattern.size() && pattern[i + count] == character) ++count;
			appendField(out, character, count, moment, locale);
			i += count;
		} else out += pattern[i++];
	}
	return out;
}

} // namespace

std::string formatDate(const LocaleData& locale, double milliseconds, std::string_view pattern, bool universal) {
	const int style = styleIndex(pattern);
	const Moment moment = momentOf(milliseconds, universal);
	if(style < 0) return byPattern(pattern.substr(0, 256), moment, locale);
	return byPattern(locale.dateFormats[static_cast<std::size_t>(style)], moment, locale);
}

std::string formatTime(const LocaleData& locale, double milliseconds, std::string_view style, bool universal) {
	const int index = styleIndex(style);
	const Moment moment = momentOf(milliseconds, universal);
	if(index < 0) return byPattern(style.substr(0, 256), moment, locale);
	return byPattern(locale.timeFormats[static_cast<std::size_t>(index)], moment, locale);
}

bool formatRelative(const LocaleData& locale, double value, std::string_view unit, std::string& text) {
	static constexpr std::array<std::string_view, relativeUnitCount> units{"year", "month",  "week",  "day",
	                                                                       "hour", "minute", "second"};
	if(unit.ends_with('s')) unit.remove_suffix(1);
	std::size_t index = 0;
	while(index < units.size() && units[index] != unit) ++index;
	if(index == units.size() || !std::isfinite(value)) return false;
	const RelativeUnit& data = locale.relative[index];

	if(std::trunc(value) == value && std::fabs(value) <= 1) {
		const std::string_view word = data.words[static_cast<std::size_t>(value + 1)];
		if(!word.empty()) {
			text = word;
			return true;
		}
	}
	const double size = std::fabs(value);
	const int decimals = std::trunc(size) == size ? 0 : 1;
	const auto& patterns = value < 0 ? data.past : data.future;
	std::string_view pattern = patterns[static_cast<std::size_t>(pluralCategory(locale, size, decimals))];
	if(pattern.empty()) pattern = patterns[static_cast<std::size_t>(Plural::Other)];
	const std::string number = formatNumber(locale, size, decimals, decimals);
	text.clear();
	for(std::size_t i = 0; i < pattern.size(); ++i)
		if(pattern.substr(i).starts_with("{0}")) text += number, i += 2;
		else text += pattern[i];
	return true;
}

} // namespace affresco::format::detail
