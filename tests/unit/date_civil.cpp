/// @file
/// @brief Unit tests for CivilDate and TimeOfDay (affresco/render/CivilDate.hpp): leap years, day counts both ways
/// over four centuries, month arithmetic with clamped days, weekdays, the range's ends and ISO 8601 text.

#include <affresco/render/CivilDate.hpp>

#include <doctest/doctest.h>

#include <cstdint>

using namespace affresco;

TEST_CASE("CivilDate: leap years and month lengths") {
	CHECK(isLeapYear(2024));
	CHECK(isLeapYear(2000));
	CHECK_FALSE(isLeapYear(1900));
	CHECK_FALSE(isLeapYear(2026));
	CHECK(daysInMonth(2024, 2) == 29);
	CHECK(daysInMonth(2026, 2) == 28);
	CHECK(daysInMonth(2026, 4) == 30);
	CHECK(daysInMonth(2026, 12) == 31);
	CHECK(daysInMonth(2026, 13) == 0);
	CHECK(isValid({2024, 2, 29}));
	CHECK_FALSE(isValid({2026, 2, 29}));
	CHECK_FALSE(isValid({0, 1, 1}));
	CHECK_FALSE(isValid({10000, 1, 1}));
}

TEST_CASE("CivilDate: day counts round-trip over four centuries and match known days") {
	CHECK(daysSinceEpoch({1970, 1, 1}) == 0);
	CHECK(daysSinceEpoch({2000, 3, 1}) == 11017);
	CHECK(daysSinceEpoch({1969, 12, 31}) == -1);
	CHECK(dateFromDays(0) == CivilDate{1970, 1, 1});
	CHECK(dateFromDays(-1) == CivilDate{1969, 12, 31});
	CivilDate expected{1900, 1, 1};
	for(std::int64_t days = daysSinceEpoch(expected); days < daysSinceEpoch({2300, 1, 1}); ++days) {
		const CivilDate date = dateFromDays(days);
		REQUIRE(date == expected);
		REQUIRE(daysSinceEpoch(date) == days);
		expected.day += 1;
		if(expected.day > daysInMonth(expected.year, expected.month)) {
			expected.day = 1;
			if(++expected.month > 12) {
				expected.month = 1;
				++expected.year;
			}
		}
	}
}

TEST_CASE("CivilDate: adding days and months, weekdays and weeks") {
	CHECK(addDays({2024, 2, 28}, 1) == CivilDate{2024, 2, 29});
	CHECK(addDays({2026, 12, 31}, 1) == CivilDate{2027, 1, 1});
	CHECK(addDays({2026, 1, 1}, -1) == CivilDate{2025, 12, 31});
	CHECK(addMonths({2025, 1, 31}, 1) == CivilDate{2025, 2, 28});
	CHECK(addMonths({2024, 1, 31}, 1) == CivilDate{2024, 2, 29});
	CHECK(addMonths({2026, 1, 15}, -1) == CivilDate{2025, 12, 15});
	CHECK(addMonths({2026, 3, 31}, 12) == CivilDate{2027, 3, 31});
	CHECK(weekday({1970, 1, 1}) == 4); // a Thursday
	CHECK(weekday({2026, 10, 22}) == 4);
	CHECK(weekday({1969, 12, 28}) == 0); // a Sunday, before 1970
	CHECK(weekday({2000, 1, 1}) == 6);
	CHECK(startOfWeek({2026, 10, 22}, 1) == CivilDate{2026, 10, 19});
	CHECK(startOfWeek({2026, 10, 22}, 0) == CivilDate{2026, 10, 18});
	CHECK(startOfWeek({2026, 10, 18}, 0) == CivilDate{2026, 10, 18});
	CHECK(startOfWeek({2026, 10, 22}, 6) == CivilDate{2026, 10, 17});
}

TEST_CASE("CivilDate: arithmetic stays within the years it holds") {
	CHECK(addDays({9999, 12, 31}, 1) == CivilDate{9999, 12, 31});
	CHECK(addDays({1, 1, 1}, -1) == CivilDate{1, 1, 1});
	CHECK(addDays({2026, 1, 1}, INT64_MAX) == CivilDate{9999, 12, 31});
	CHECK(addDays({2026, 1, 1}, INT64_MIN) == CivilDate{1, 1, 1});
	CHECK(addMonths({9999, 12, 31}, 1) == CivilDate{9999, 12, 31});
	CHECK(addMonths({2026, 1, 1}, INT64_MIN) == CivilDate{1, 1, 1});
	CHECK(dateFromDays(INT64_MAX) == CivilDate{9999, 12, 31});
}

TEST_CASE("CivilDate: ISO 8601 text") {
	CHECK(parseIsoDate("2026-10-22") == CivilDate{2026, 10, 22});
	CHECK(parseIsoDate("2026-10-22T07:30:00Z") == CivilDate{2026, 10, 22});
	CHECK(parseIsoDate("0001-01-01") == CivilDate{1, 1, 1});
	CHECK_FALSE(parseIsoDate("2026-02-30"));
	CHECK_FALSE(parseIsoDate("2026-1-5"));
	CHECK_FALSE(parseIsoDate("26-10-22"));
	CHECK_FALSE(parseIsoDate(""));
	CHECK_FALSE(parseIsoDate("0000-01-01"));
	CHECK(isoDate({2026, 3, 7}) == "2026-03-07");
	CHECK(isoDate({1, 1, 1}) == "0001-01-01");
}

TEST_CASE("TimeOfDay: text and seconds") {
	CHECK(parseTimeOfDay("07:30") == TimeOfDay{7, 30, 0});
	CHECK(parseTimeOfDay("23:59:59") == TimeOfDay{23, 59, 59});
	CHECK_FALSE(parseTimeOfDay("24:00"));
	CHECK_FALSE(parseTimeOfDay("7:30"));
	CHECK_FALSE(parseTimeOfDay("07:30:"));
	CHECK_FALSE(parseTimeOfDay("07:60"));
	CHECK_FALSE(parseTimeOfDay("07:30 "));
	CHECK(isoTime({7, 5, 0}) == "07:05");
	CHECK(isoTime({7, 5, 9}) == "07:05:09");
	CHECK(secondsSinceMidnight({1, 1, 1}) == 3661);
	CHECK(timeFromSeconds(3661) == TimeOfDay{1, 1, 1});
	CHECK(timeFromSeconds(-1) == TimeOfDay{23, 59, 59});
	CHECK(timeFromSeconds(86400 * 3 + 60) == TimeOfDay{0, 1, 0});
}
