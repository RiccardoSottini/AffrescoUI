#pragma once

/// @file
/// @brief Calendar: a month of days to pick a date from, with the days outside a range disabled, marked days and
/// today, in the locale's month and day names and writing direction.

#include "affresco/render/CivilDate.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace affresco {

/// @brief A month view to pick a date. A header shows the month and year between a previous and a next button, a row
/// names the days of the week, and six rows of seven days follow. Keyboard: the arrows move the day the keyboard is on
/// by a day or a week (mirrored right to left), Page Up and Page Down by a month (with Shift, a year), Home and End
/// to the week's ends, and Enter or Space picks it. Clicking a day picks it.
class Calendar : public Panel {
public:
	static constexpr int weeksShown = 6; ///< The rows of days, enough for any month.

	/// @brief An empty calendar showing January 1970, the week starting on Monday, English names.
	Calendar();

	/// @brief Sets the picked date, without firing onValueChanged; the calendar turns to its month.
	/// @param value The date, or std::nullopt for none.
	void setValue(std::optional<CivilDate> value);
	/// @brief The picked date, if any.
	[[nodiscard]] const std::optional<CivilDate>& value() const noexcept { return value_; }
	/// @brief Sets the earliest and latest days that can be picked; the others show disabled.
	/// @param earliest The first day, or std::nullopt for no limit.
	/// @param latest The last day, or std::nullopt for no limit.
	void setRange(std::optional<CivilDate> earliest, std::optional<CivilDate> latest);
	/// @brief Sets today, drawn outlined (the document gives it, so screenshots don't change from day to day).
	/// @param today The day, or std::nullopt for none.
	void setToday(std::optional<CivilDate> today);
	/// @brief Sets the days marked with a dot (those with events, say).
	/// @param marked The days.
	void setMarked(std::vector<CivilDate> marked);
	/// @brief Sets the day weeks start on.
	/// @param day 0 for Sunday to 6 for Saturday; others are taken modulo 7.
	void setFirstDayOfWeek(int day);
	/// @brief Sets the names it shows, in the locale's language.
	/// @param months The twelve months, January first ("October"); missing ones stay English.
	/// @param days The seven days of the week, Sunday first, short ("Su"); missing ones stay English.
	void setNames(const std::vector<std::string>& months, const std::vector<std::string>& days);
	/// @brief Sets the font of the names and the days.
	/// @param font The font; nullptr for the window's default.
	void setFont(std::shared_ptr<const Font> font);
	/// @brief Sets the colours.
	/// @param text The names and the days.
	/// @param accent The picked day's fill, the keyboard's outline and the dots.
	/// @param onAccent The picked day's number.
	void setColors(Color text, Color accent, Color onAccent);
	/// @brief The day the keyboard is on.
	[[nodiscard]] CivilDate focusedDay() const noexcept { return focused_; }
	/// @brief Moves the day the keyboard is on (kept within the range), turning to its month.
	/// @param day The day.
	void setFocusedDay(CivilDate day);
	/// @brief The first of the month shown.
	[[nodiscard]] CivilDate shownMonth() const noexcept { return {focused_.year, focused_.month, 1}; }
	/// @brief Tells whether a day can be picked: within the range.
	/// @param day The day.
	[[nodiscard]] bool isEnabled(CivilDate day) const noexcept;
	/// @brief Picks a day, as a click does, firing onValueChanged if it changed.
	/// @param day The day; nothing happens if it can't be picked.
	void pick(CivilDate day);

	Signal<CivilDate> onValueChanged; ///< The user picked a date (not setValue()).

protected:
	void keyEvent(const KeyEvent& event) override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;

private:
	/// @brief Where the parts are, in layout units, for the current frame.
	struct Layout {
		Rect title;      ///< The month and year.
		Rect previous;   ///< The previous month's button.
		Rect next;       ///< The next month's button.
		Rect dayNames;   ///< The row of day names.
		Rect days;       ///< The six weeks.
		float cell = 0;  ///< A day's width.
		float row = 0;   ///< A week's height.
	};

	/// @brief Computes where the parts are within frame().
	[[nodiscard]] Layout layout() const noexcept;
	/// @brief The font used, or nullptr.
	[[nodiscard]] const Font* font() const noexcept;
	/// @brief The first day of the first week shown.
	[[nodiscard]] CivilDate firstShown() const noexcept;
	/// @brief Where a shown day is drawn.
	/// @param area The layout.
	/// @param index 0 to 41, the day's place from firstShown().
	[[nodiscard]] Rect dayRect(const Layout& area, int index) const noexcept;
	/// @brief A column's place on screen, mirrored right to left.
	/// @param column 0 to 6, in week order.
	[[nodiscard]] int screenColumn(int column) const noexcept;
	/// @brief Keeps a day within the range.
	/// @param day The day.
	[[nodiscard]] CivilDate clamped(CivilDate day) const noexcept;
	/// @brief Handles a key that moves the cursor.
	/// @param event The key.
	/// @return Whether it was handled.
	bool moveKey(const KeyEvent& event);
	/// @brief Draws the header and the day names.
	/// @param painter Where to draw.
	/// @param area The layout.
	void paintHeader(Painter& painter, const Layout& area) const;
	/// @brief Draws one shown day.
	/// @param painter Where to draw.
	/// @param area The layout.
	/// @param index 0 to 41, the day's place from firstShown().
	void paintDay(Painter& painter, const Layout& area, int index) const;

	std::optional<CivilDate> value_;              ///< See value().
	std::optional<CivilDate> earliest_, latest_;  ///< See setRange().
	std::optional<CivilDate> today_;              ///< See setToday().
	std::vector<CivilDate> marked_;               ///< See setMarked(), sorted.
	CivilDate focused_;                            ///< See focusedDay().
	int firstDayOfWeek_ = 1;                      ///< See setFirstDayOfWeek().
	std::array<std::u32string, 12> months_;       ///< See setNames().
	std::array<std::u32string, 7> days_;          ///< See setNames().
	std::shared_ptr<const Font> font_;            ///< See setFont().
	Color text_ = colors::black;                  ///< See setColors().
	Color accent_ = colors::accent;               ///< See setColors().
	Color onAccent_ = colors::white;              ///< See setColors().
};

} // namespace affresco
