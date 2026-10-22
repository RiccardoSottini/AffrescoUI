/// @file
/// @brief The "calendar" node type: a Calendar, its range, today, marked days, first day of the week, names and
/// colours, and its value as an ISO 8601 date, "YYYY-MM-DD".

#include "NodeKeys.hpp"
#include "TypeRegistry.hpp"

#include "affresco/render/Calendar.hpp"

#include <utility>

namespace affresco::format {

namespace {

/// @brief Reads an optional date: an ISO 8601 date, or null or "" for none.
/// @param values The node's values.
/// @param key The property.
/// @param path The node's place, for errors.
/// @return The date, or std::nullopt when not given or empty.
std::optional<CivilDate> dateOr(const json& values, const char* key, const std::string& path) {
	if(!values.contains(key) || values[key].is_null()) return std::nullopt;
	const std::string text = string(values[key], path + "." + key);
	if(text.empty()) return std::nullopt;
	const std::optional<CivilDate> date = parseIsoDate(text);
	if(!date) fail(path + "." + key, R"(expected a date, "YYYY-MM-DD")");
	return date;
}

/// @brief Reads a list of texts.
/// @param values The node's values.
/// @param key The property.
/// @param path The node's place, for errors.
/// @return The texts; none when not given.
std::vector<std::string> textsOr(const json& values, const char* key, const std::string& path) {
	std::vector<std::string> texts;
	if(!values.contains(key)) return texts;
	const json& list = values[key];
	if(!list.is_array()) fail(path + "." + key, "expected a list of texts");
	for(std::size_t index = 0; index < list.size(); ++index)
		texts.push_back(string(list[index], concatenate(path, ".", std::string(key) + "[" + std::to_string(index) + "]")));
	return texts;
}

/// @brief Sets a calendar's range, today, marked days, first day of the week, names, font and colours.
/// @param panel The panel, a Calendar.
/// @param values Its properties, if given.
/// @param reader Reads colours and fonts.
/// @param path The node's place, for errors.
void applyCalendar(Panel& panel, const json& values, const PropertyReader& reader, const std::string& path) {
	auto* calendar = dynamic_cast<Calendar*>(&panel);
	if(!calendar) return;
	if(panel.accessibleRole().empty()) panel.setAccessibleRole("grid"); // days in rows of weeks
	const auto colorOr = [&](const char* key, Color fallback) {
		return values.contains(key) ? reader.color(values[key], path + "." + key) : fallback;
	};
	calendar->setRange(dateOr(values, "min", path), dateOr(values, "max", path));
	calendar->setToday(dateOr(values, "today", path));
	std::vector<CivilDate> marked;
	for(const std::string& text : textsOr(values, "marked", path))
		if(const std::optional<CivilDate> date = parseIsoDate(text)) marked.push_back(*date); // others are ignored
	calendar->setMarked(std::move(marked));
	const int first = values.contains("firstDayOfWeek")
	                      ? static_cast<int>(number(values["firstDayOfWeek"], path + ".firstDayOfWeek"))
	                      : 1;
	calendar->setFirstDayOfWeek(first);
	calendar->setNames(textsOr(values, "monthNames", path), textsOr(values, "dayNames", path));
	calendar->setFont(values.contains("font") ? reader.font(values["font"], path + ".font") : nullptr);
	calendar->setColors(colorOr("textColor", colors::black), colorOr("accentColor", colors::accent),
	                    colorOr("pickedTextColor", colors::white));
}

/// @brief Shows a calendar's date: its bound value, else "value"; a value that isn't a date shows none.
/// @param panel The panel, a Calendar.
/// @param bound The bound state value, or nullptr.
/// @param properties The node's evaluated properties.
/// @param lastText Not used: a calendar shows a date.
void showDate(Panel& panel, const json* bound, const json& properties, std::optional<std::string>& lastText) {
	static_cast<void>(lastText);
	auto* calendar = dynamic_cast<Calendar*>(&panel);
	if(!calendar) return;
	const json& value = bound ? *bound : properties.contains("value") ? properties["value"] : emptyObject();
	calendar->setValue(value.is_string() ? parseIsoDate(value.get<std::string>()) : std::nullopt);
}

/// @brief Reports a date the user picked, as "YYYY-MM-DD".
/// @param panel The panel, a Calendar.
/// @param changed Gets the value.
/// @return The connection.
Connection listenToCalendar(Panel& panel, std::function<void(const json&)> changed) {
	auto* calendar = dynamic_cast<Calendar*>(&panel);
	if(!calendar) return 0;
	return calendar->onValueChanged.connect([changed = std::move(changed)](CivilDate day) { changed(json(isoDate(day))); });
}

/// @brief Disconnects listenToCalendar().
/// @param panel The panel.
/// @param connection The connection.
void stopListeningToCalendar(Panel& panel, Connection connection) {
	if(auto* calendar = dynamic_cast<Calendar*>(&panel)) calendar->onValueChanged.disconnect(connection);
}

} // namespace

void registerCalendarType(TypeRegistry& registry) {
	const auto set = std::make_shared<PropertySet>();
	set->properties = {{"bind", ValueKind::StatePath},     {"value", ValueKind::Text},      {"min", ValueKind::Text},
	                   {"max", ValueKind::Text},           {"today", ValueKind::Text},      {"marked", ValueKind::Any},
	                   {"firstDayOfWeek", ValueKind::Number}, {"monthNames", ValueKind::Any}, {"dayNames", ValueKind::Any},
	                   {"font", ValueKind::Font},          {"textColor", ValueKind::Color}, {"accentColor", ValueKind::Color},
	                   {"pickedTextColor", ValueKind::Color}};
	set->apply = applyCalendar;
	set->everyUpdate = true; // colours and names follow the theme and the locale
	set->keepsClicks = true;
	set->appliesTo = [](const Panel& panel) { return dynamic_cast<const Calendar*>(&panel) != nullptr; };
	set->value = {"value", showDate, listenToCalendar, stopListeningToCalendar};

	registry.add({"calendar", "A month of days to pick a date from, \"YYYY-MM-DD\": a range, today and marked days",
	              [] { return std::make_unique<Calendar>(); }, set, false});
}

} // namespace affresco::format
