/// @file
/// @brief Unit tests for the Calendar control (affresco/render/Calendar.hpp): its value, range, keyboard and what it
/// reports.

#include <affresco/render/Calendar.hpp>

#include <doctest/doctest.h>

#include <vector>

using namespace affresco;

namespace {

/// @brief A calendar whose key handler the tests can call.
struct TestCalendar : Calendar {
	using Calendar::keyEvent;

	/// @brief Presses a key.
	/// @param key The key.
	/// @param mods The modifiers' bits (1 for Shift).
	void press(Key key, int mods = 0) { keyEvent({key, 0, Action::Press, Mods{mods}}); }
};

} // namespace

TEST_CASE("Calendar: setValue turns to the month and fires nothing; pick fires once") {
	TestCalendar calendar;
	std::vector<CivilDate> picked;
	calendar.onValueChanged.connect([&picked](CivilDate day) { picked.push_back(day); });
	calendar.setValue(CivilDate{2026, 10, 22});
	CHECK(calendar.shownMonth() == CivilDate{2026, 10, 1});
	CHECK(calendar.focusedDay() == CivilDate{2026, 10, 22});
	CHECK(picked.empty());
	calendar.pick({2026, 10, 23});
	calendar.pick({2026, 10, 23}); // the same day again: no change
	REQUIRE(picked.size() == 1);
	CHECK(picked[0] == CivilDate{2026, 10, 23});
	calendar.setValue(CivilDate{2026, 2, 30}); // not a day: no value
	CHECK_FALSE(calendar.value());
}

TEST_CASE("Calendar: the keyboard moves by days, weeks, months and years, and Enter picks") {
	TestCalendar calendar;
	calendar.setFirstDayOfWeek(1);
	calendar.setValue(CivilDate{2026, 10, 22}); // a Thursday
	calendar.press(Key::Right);
	CHECK(calendar.focusedDay() == CivilDate{2026, 10, 23});
	calendar.press(Key::Down);
	CHECK(calendar.focusedDay() == CivilDate{2026, 10, 30});
	calendar.press(Key::PageDown);
	CHECK(calendar.focusedDay() == CivilDate{2026, 11, 30});
	calendar.press(Key::PageUp, 1); // Shift: a year
	CHECK(calendar.focusedDay() == CivilDate{2025, 11, 30});
	calendar.press(Key::Home); // Monday
	CHECK(calendar.focusedDay() == CivilDate{2025, 11, 24});
	calendar.press(Key::End); // Sunday
	CHECK(calendar.focusedDay() == CivilDate{2025, 11, 30});
	CHECK(calendar.value() == CivilDate{2026, 10, 22}); // moving doesn't pick
	calendar.press(Key::Enter);
	CHECK(calendar.value() == CivilDate{2025, 11, 30});
}

TEST_CASE("Calendar: days outside the range can't be picked or reached") {
	TestCalendar calendar;
	calendar.setRange(CivilDate{2026, 10, 1}, CivilDate{2026, 10, 31});
	calendar.setValue(CivilDate{2026, 10, 30});
	calendar.press(Key::Down);
	CHECK(calendar.focusedDay() == CivilDate{2026, 10, 31});
	calendar.press(Key::PageUp, 1);
	CHECK(calendar.focusedDay() == CivilDate{2026, 10, 1});
	CHECK_FALSE(calendar.isEnabled({2026, 11, 1}));
	calendar.pick({2026, 11, 1});
	CHECK(calendar.value() == CivilDate{2026, 10, 30});
}
