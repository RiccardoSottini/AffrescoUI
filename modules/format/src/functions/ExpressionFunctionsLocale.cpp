/// @file
/// @brief Expression functions: translations and the locale's formats (t, formatNumber, formatPercent, formatCurrency,
/// formatDate, formatTime, formatRelative, formatList, compareText, sortText).

#include "ExpressionFunctions.hpp"

#include "Locale.hpp"
#include "Strings.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace affresco::format::detail {
namespace {

/// @brief The locale a call formats in: the document's, or English for a tool's evaluation.
/// @param call The call.
/// @return Its data.
const LocaleData& localeOf(const Call& call) noexcept {
	const Strings* strings = call.strings();
	return strings != nullptr ? strings->data() : findLocale("en");
}

/// @brief An optional whole-number argument, clamped.
/// @param call The call.
/// @param index Its position.
/// @param fallback Its value if it isn't given.
/// @return It, from 0 to 20.
int decimalsArgument(const Call& call, std::size_t index, int fallback) {
	if(call.size() <= index || call[index].isNull()) return fallback;
	return static_cast<int>(std::clamp<std::int64_t>(saturated(call.number(index)), 0, 20));
}

/// @brief Reads a date's or time's optional style and zone.
/// @param call [milliseconds, style, zone].
/// @param style Its style if none is given.
/// @param universal Set to whether the zone is "utc".
/// @return The style or pattern.
std::string styleAndZone(const Call& call, std::string_view style, bool& universal) {
	const std::string zone = call.size() > 2 ? call[2].toText() : "local";
	if(zone != "utc" && zone != "local") call.fail("the zone is \"utc\" or \"local\"");
	universal = zone == "utc";
	return call.size() > 1 && !call[1].isNull() ? call[1].toText() : std::string(style);
}

/// @brief `t(key, {params})`.
/// @param call [key, parameters].
/// @return The key's message in the locale, formatted; the key itself if no locale has it.
Value translate(const Call& call) {
	const std::string key = call[0].toText();
	const Strings* strings = call.strings();
	if(strings == nullptr) return Value::fromText(key);
	return Value::fromText(strings->translate(key, call.size() > 1 ? call[1] : Value()));
}

/// @brief `formatNumber(n, fewest, most)`.
/// @param call [number, fewest decimals, most decimals].
/// @return The number in the locale's way.
Value formatNumberValue(const Call& call) {
	const int fewest = decimalsArgument(call, 1, 0);
	const int most = std::max(fewest, decimalsArgument(call, 2, std::max(fewest, 3)));
	return Value::fromText(formatNumber(localeOf(call), call.number(0), fewest, most));
}

/// @brief `formatPercent(ratio, decimals)`.
/// @param call [ratio, decimals].
/// @return The percent.
Value formatPercentValue(const Call& call) {
	return Value::fromText(formatPercent(localeOf(call), call.number(0), decimalsArgument(call, 1, 0)));
}

/// @brief `formatCurrency(amount, code)`.
/// @param call [amount, currency code].
/// @return The amount with the currency's symbol.
Value formatCurrencyValue(const Call& call) {
	return Value::fromText(formatCurrency(localeOf(call), call.number(0), call[1].toText().substr(0, 3)));
}

/// @brief `formatDate(ms, style, zone)`.
/// @param call [milliseconds, style or pattern, "utc" or "local"].
/// @return The date.
Value formatDateValue(const Call& call) {
	bool universal = false;
	const std::string style = styleAndZone(call, "medium", universal);
	return Value::fromText(formatDate(localeOf(call), call.number(0), style, universal));
}

/// @brief `formatTime(ms, style, zone)`.
/// @param call [milliseconds, style or pattern, "utc" or "local"].
/// @return The time of day.
Value formatTimeValue(const Call& call) {
	bool universal = false;
	const std::string style = styleAndZone(call, "short", universal);
	return Value::fromText(formatTime(localeOf(call), call.number(0), style, universal));
}

/// @brief `formatRelative(value, unit)`.
/// @param call [how many units, the unit].
/// @return "in 3 days", "yesterday".
Value formatRelativeValue(const Call& call) {
	std::string text;
	if(!formatRelative(localeOf(call), call.number(0), call[1].toText(), text))
		call.fail("the unit is year, month, week, day, hour, minute or second");
	return Value::fromText(std::move(text));
}

/// @brief `formatList(list, kind)`.
/// @param call [list, "and" or "or"].
/// @return "a, b and c".
Value formatListValue(const Call& call) {
	if(!call[0].isList()) call.fail("expected a list");
	const std::string kind = call.size() > 1 ? call[1].toText() : "and";
	if(kind != "and" && kind != "or") call.fail("the kind is \"and\" or \"or\"");
	std::vector<std::string> items;
	items.reserve(call[0].size());
	for(std::size_t i = 0; i < call[0].size(); ++i) items.push_back(call[0].item(i).toText());
	return Value::fromText(formatList(localeOf(call), items, kind == "or"));
}

/// @brief `compareText(a, b)`.
/// @param call [a, b].
/// @return -1, 0 or 1: letters first without accents or case, then accents, then case.
Value compareTextValue(const Call& call) {
	const int order = compareText(call[0].toText(), call[1].toText());
	return Value::fromInteger(order < 0 ? -1 : order > 0 ? 1 : 0);
}

/// @brief `sortText(list)`.
/// @param call [list].
/// @return The list, sorted as compareText() orders its items' text.
Value sortTextValue(const Call& call) {
	if(!call[0].isList()) call.fail("expected a list");
	std::vector<std::pair<std::string, Value>> keyed;
	keyed.reserve(call[0].size());
	for(std::size_t i = 0; i < call[0].size(); ++i) {
		Value item = call[0].item(i);
		keyed.emplace_back(item.toText(), std::move(item));
	}
	std::ranges::stable_sort(keyed, [](const auto& a, const auto& b) { return compareText(a.first, b.first) < 0; });
	std::vector<Value> items;
	items.reserve(keyed.size());
	for(auto& entry : keyed) items.push_back(std::move(entry.second));
	return Value::fromList(std::move(items));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "t",
	 .arguments = "'key', {params}",
	 .about = "The key's text in the locale (\"strings\"), its {params} filled; the key itself if none has it",
	 .fewest = 1,
	 .most = 2,
	 .apply = translate},
    {.name = "formatNumber",
	 .arguments = "n, fewest?, most?",
	 .about = "A number in the locale's way, with fewest to most decimals (0 and 3): 1,234.5 or 1.234,5",
	 .fewest = 1,
	 .most = 3,
	 .apply = formatNumberValue},
    {.name = "formatPercent",
	 .arguments = "ratio, decimals?",
	 .about = "A ratio as a percent in the locale's way: 0.25 is 25%",
	 .fewest = 1,
	 .most = 2,
	 .apply = formatPercentValue},
    {.name = "formatCurrency",
	 .arguments = "amount, 'EUR'",
	 .about = "An amount with its currency's symbol and decimals, in the locale's way: €1,234.50",
	 .fewest = 2,
	 .most = 2,
	 .apply = formatCurrencyValue},
    {.name = "formatDate",
	 .arguments = "milliseconds, style?, zone?",
	 .about = "A date in the locale: style full, long, medium (default), short or a CLDR pattern; zone local or utc",
	 .fewest = 1,
	 .most = 3,
	 .apply = formatDateValue},
    {.name = "formatTime",
	 .arguments = "milliseconds, style?, zone?",
	 .about = "A time of day in the locale: style full, long, medium, short (default) or a pattern; zone local or utc",
	 .fewest = 1,
	 .most = 3,
	 .apply = formatTimeValue},
    {.name = "formatRelative",
	 .arguments = "value, 'day'",
	 .about = "A relative time in the locale: -1 day is yesterday, 3 hour is in 3 hours (year to second)",
	 .fewest = 2,
	 .most = 2,
	 .apply = formatRelativeValue},
    {.name = "formatList",
	 .arguments = "list, 'and' | 'or'",
	 .about = "Items joined as a list in the locale: a, b and c",
	 .fewest = 1,
	 .most = 2,
	 .apply = formatListValue},
    {.name = "compareText",
	 .arguments = "a, b",
	 .about = "-1, 0 or 1, ordering text for people: letters, then accents, then case",
	 .fewest = 2,
	 .most = 2,
	 .apply = compareTextValue},
    {.name = "sortText",
	 .arguments = "list",
	 .about = "The list sorted as compareText orders its items",
	 .fewest = 1,
	 .most = 1,
	 .apply = sortTextValue},
});

} // namespace

std::span<const Function> localeFunctions() { return table; }

} // namespace affresco::format::detail
