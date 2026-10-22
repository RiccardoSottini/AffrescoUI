/// @file
/// @brief Unit tests for translations and locales (src/Strings.hpp, src/Locale.hpp): plural categories, the fallback
/// chain of a "strings" section, messages with parameters, plurals and selects, the formats of numbers, currencies,
/// dates, relative times and lists from the CLDR subset, text compared for sorting, and what the checker reports.

#include "Checker.hpp"
#include "Locale.hpp"
#include "Strings.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

using affresco::json;
using affresco::format::Strings;
using affresco::format::Value;
using affresco::format::detail::findLocale;
using affresco::format::detail::Plural;
using affresco::format::detail::pluralCategory;

namespace {

/// 2026-03-14 15:09:26 UTC, in milliseconds since 1970.
constexpr double saturdayAfternoon = 1773500966000.0;

/// @brief Makes the parameters of a message.
/// @param fields Their names and values.
/// @return An object of them.
Value parameters(std::vector<Value::Field> fields) { return Value::fromObject(std::move(fields)); }

/// @brief Formats a message in a locale.
/// @param message The message.
/// @param values Its parameters.
/// @param tag The locale.
/// @return The text.
std::string formatted(std::string_view message, const Value& values, std::string_view tag = "en") {
	std::vector<std::string> read;
	REQUIRE(affresco::format::checkMessage(message, read).empty());
	return affresco::format::formatMessage(message, values, findLocale(tag));
}

/// @brief Checks a document and returns its findings as lines.
/// @param document The document.
/// @return Each finding's text.
std::vector<std::string> findings(const json& document) {
	const auto found = affresco::format::check(document, document.value("state", json::object()), {});
	REQUIRE(found);
	std::vector<std::string> lines;
	if(!found) return lines; // never: REQUIRE has stopped the test
	for(const affresco::format::Finding& finding : *found) lines.push_back(finding.text());
	return lines;
}

/// @brief Tells whether some line holds a text.
/// @param lines The lines.
/// @param part The text.
/// @return Whether.
bool anyHas(const std::vector<std::string>& lines, std::string_view part) {
	return std::ranges::any_of(lines, [part](const std::string& line) { return line.contains(part); });
}

} // namespace

TEST_CASE("i18n: plural categories follow each language's CLDR rules") {
	struct Row {
		const char* tag;
		double number;
		int decimals;
		Plural expected;
	};
	const std::array rows{
	    Row{"en", 1, 0, Plural::One},  Row{"en", 1, 1, Plural::Other}, Row{"en", 0, 0, Plural::Other},
	    Row{"fr", 0, 0, Plural::One},  Row{"fr", 1.5, 1, Plural::One}, Row{"fr", 2, 0, Plural::Other},
	    Row{"ar", 0, 0, Plural::Zero}, Row{"ar", 1, 0, Plural::One},   Row{"ar", 2, 0, Plural::Two},
	    Row{"ar", 3, 0, Plural::Few},  Row{"ar", 11, 0, Plural::Many}, Row{"ar", 100, 0, Plural::Other},
	    Row{"he", 2, 0, Plural::Two},  Row{"ja", 1, 0, Plural::Other}, Row{"it", 1, 0, Plural::One},
	};
	for(const Row& row : rows) {
		CAPTURE(row.tag);
		CAPTURE(row.number);
		CHECK(pluralCategory(findLocale(row.tag), row.number, row.decimals) == row.expected);
	}
}

TEST_CASE("i18n: tags are normalized, and a language the subset hasn't formats as English") {
	using affresco::format::detail::normalizeTag;
	CHECK(normalizeTag("fr_CA.UTF-8") == "fr-CA");
	CHECK(normalizeTag("C") == "en");
	CHECK(normalizeTag("POSIX") == "en");
	CHECK(normalizeTag("") == "");
	CHECK(findLocale("fr-CA").tag == "fr");
	CHECK(findLocale("pt_BR.UTF-8").tag == "pt");
	CHECK(findLocale("xx").tag == "en");
	CHECK(findLocale("ar").rightToLeft);
	CHECK(findLocale("he").rightToLeft);
	CHECK_FALSE(findLocale("it").rightToLeft);
}

TEST_CASE("i18n: a key falls back from fr-CA to fr to the default, and a missing key shows itself") {
	const json document = json::parse(R"({"strings": {"default": "en",
		"en": {"hello": "Hello", "bye": "Bye", "only": "Only English"},
		"fr": {"hello": "Bonjour", "bye": "Au revoir"},
		"fr-CA": {"hello": "Allô"}}})");
	Strings strings;
	strings.read(document);
	strings.select("fr_CA.UTF-8");
	CHECK(strings.locale() == "fr-CA");
	CHECK(strings.chain() == std::vector<std::string>{"fr-ca", "fr", "en"});
	CHECK(strings.translate("hello", Value()) == "Allô");
	CHECK(strings.translate("bye", Value()) == "Au revoir");
	CHECK(strings.translate("only", Value()) == "Only English");
	CHECK(strings.translate("nowhere.at.all", Value()) == "nowhere.at.all");
	CHECK(strings.known("only"));
	CHECK_FALSE(strings.known("nowhere.at.all"));

	strings.select("");
	CHECK(strings.locale() == "en");
	CHECK(strings.translate("hello", Value()) == "Hello");
	strings.select("ar");
	CHECK(strings.rightToLeft());
	CHECK(strings.translate("bye", Value()) == "Bye");
}

TEST_CASE("i18n: messages fill parameters, choose plurals and selects, and quote braces") {
	const std::string files = "{n, plural, =0 {No files} one {# file} other {# files}}";
	CHECK(formatted(files, parameters({{"n", Value::fromInteger(0)}})) == "No files");
	CHECK(formatted(files, parameters({{"n", Value::fromInteger(1)}})) == "1 file");
	CHECK(formatted(files, parameters({{"n", Value::fromInteger(1234)}})) == "1,234 files");
	CHECK(formatted(files, parameters({{"n", Value::fromInteger(1234)}}), "de") == "1.234 files");

	const std::string arabic = "{n, plural, zero {z} one {o} two {t} few {f} many {m} other {x}}";
	std::string chosen;
	for(const int n : {0, 1, 2, 3, 11, 100})
		chosen += formatted(arabic, parameters({{"n", Value::fromInteger(n)}}), "ar");
	CHECK(chosen == "zotfmx");

	const std::string who = "{kind, select, cat {The cat} other {Someone}} says {word}";
	CHECK(formatted(who, parameters({{"kind", Value::fromText("cat")}, {"word", Value::fromText("meow")}})) ==
	      "The cat says meow");
	CHECK(formatted(who, parameters({{"kind", Value::fromText("dog")}})) == "Someone says {word}");
	CHECK(formatted("'{literal}' it''s", Value()) == "{literal} it's");
	CHECK(formatted("{n, plural, one {{who} has # task} other {{who} has # tasks}}",
	                parameters({{"n", Value::fromInteger(2)}, {"who", Value::fromText("Anna")}})) ==
	      "Anna has 2 tasks");
}

TEST_CASE("i18n: malformed messages are refused, with where") {
	std::vector<std::string> read;
	CHECK(affresco::format::checkMessage("{n, plural, one {x}}", read).contains("needs an 'other' branch"));
	CHECK(affresco::format::checkMessage("{n, colour}", read).contains("isn't number, date, time, plural or select"));
	CHECK(affresco::format::checkMessage("{n, plural, lots {x} other {y}}", read).contains("isn't a plural selector"));
	CHECK_FALSE(affresco::format::checkMessage("Unclosed {name", read).empty());
	// An offset past a double's range is refused, not thrown out of the check
	const std::string huge = "{n, plural, offset:" + std::string(400, '9') + " other {x}}";
	CHECK(affresco::format::checkMessage(huge, read).contains("at most 9 digits"));
	CHECK(affresco::format::checkMessage("{n, plural, offset:1 other {#}}", read).empty());
	read.clear();
	CHECK(affresco::format::checkMessage("{a} and {b, plural, other {#}} and {a}", read).empty());
	CHECK(read == std::vector<std::string>{"a", "b"});
}

TEST_CASE("i18n: a short message can't repeat a long parameter into a huge text") {
	// 100 copies of 1 MiB would be 100 MiB: past the longest text an expression builds, so the message is shown as is
	std::string message;
	for(int i = 0; i < 100; ++i) message += "{p}";
	const Value values = parameters({{"p", Value::fromText(std::string(1u << 20, 'a'))}});
	CHECK(affresco::format::formatMessage(message, values, findLocale("en")) == message);
	CHECK(affresco::format::formatMessage("{p}{p}", values, findLocale("en")).size() == 2u << 20);
}

TEST_CASE("i18n: numbers, percents and currencies in the locale's way") {
	using affresco::format::detail::formatCurrency;
	using affresco::format::detail::formatNumber;
	using affresco::format::detail::formatPercent;
	CHECK(formatNumber(findLocale("en"), 1234.5) == "1,234.5");
	CHECK(formatNumber(findLocale("de"), 1234.5) == "1.234,5");
	CHECK(formatNumber(findLocale("fr"), 1234.5) == "1 234,5");
	CHECK(formatNumber(findLocale("it"), 1234.5) == "1234,5"); // Italian groups from five digits
	CHECK(formatNumber(findLocale("it"), 12345.5) == "12.345,5");
	CHECK(formatNumber(findLocale("en"), -0.125, 2, 2) == "-0.13");
	CHECK(formatNumber(findLocale("en"), 1.005, 2, 2) == "1.01"); // its shortest decimal, rounded half away from zero
	CHECK(formatNumber(findLocale("en"), 2, 2) == "2.00");
	CHECK(formatPercent(findLocale("en"), 0.256, 1) == "25.6%");
	CHECK(formatPercent(findLocale("fr"), 0.25) == "25\u00a0%"); // CLDR's French percent takes a no-break space
	CHECK(formatCurrency(findLocale("en"), 1234.5, "EUR") == "€1,234.50");
	CHECK(formatCurrency(findLocale("de"), 1234.5, "EUR") == "1.234,50 €");
	CHECK(formatCurrency(findLocale("en"), 1234.5, "JPY") == "¥1,235");
	CHECK(formatCurrency(findLocale("en"), -5, "USD") == "-$5.00");
}

TEST_CASE("i18n: dates, times and relative times in the locale's words") {
	using affresco::format::detail::formatDate;
	using affresco::format::detail::formatRelative;
	using affresco::format::detail::formatTime;
	CHECK(formatDate(findLocale("en"), saturdayAfternoon, "medium", true) == "Mar 14, 2026");
	CHECK(formatDate(findLocale("en"), saturdayAfternoon, "full", true) == "Saturday, March 14, 2026");
	CHECK(formatDate(findLocale("it"), saturdayAfternoon, "short", true) == "14/03/26");
	CHECK(formatDate(findLocale("de"), saturdayAfternoon, "medium", true) == "14.03.2026");
	CHECK(formatDate(findLocale("en"), saturdayAfternoon, "y-MM-dd 'at' HH:mm", true) == "2026-03-14 at 15:09");
	CHECK(formatTime(findLocale("en"), saturdayAfternoon, "short", true) == "3:09 PM");
	CHECK(formatTime(findLocale("it"), saturdayAfternoon, "short", true) == "15:09");

	std::string text;
	REQUIRE(formatRelative(findLocale("en"), -1, "day", text));
	CHECK(text == "yesterday");
	REQUIRE(formatRelative(findLocale("en"), 3, "hours", text));
	CHECK(text == "in 3 hours");
	REQUIRE(formatRelative(findLocale("en"), -2, "day", text));
	CHECK(text == "2 days ago");
	REQUIRE(formatRelative(findLocale("it"), 1, "day", text));
	CHECK(text == "domani");
	CHECK_FALSE(formatRelative(findLocale("en"), 1, "fortnight", text));
}

TEST_CASE("i18n: lists are joined, and text is compared letters first, then accents, then case") {
	using affresco::format::detail::compareText;
	using affresco::format::detail::formatList;
	const std::vector<std::string> names{"Anna", "Marco", "Sara"};
	CHECK(formatList(findLocale("en"), names, false) == "Anna, Marco, and Sara");
	CHECK(formatList(findLocale("it"), names, false) == "Anna, Marco e Sara");
	CHECK(formatList(findLocale("en"), std::vector<std::string>{"tea", "coffee"}, true) == "tea or coffee");
	CHECK(formatList(findLocale("en"), std::vector<std::string>{"alone"}, false) == "alone");
	CHECK(formatList(findLocale("en"), std::vector<std::string>{}, false).empty());

	CHECK(compareText("apple", "Banana") < 0);
	CHECK(compareText("e", "é") < 0);
	CHECK(compareText("é", "f") < 0);
	CHECK(compareText("a", "A") < 0);
	CHECK(compareText("same", "same") == 0);
}

TEST_CASE("i18n: the checker reports unknown keys and wrong parameters, not computed ones") {
	const auto document = [](const std::string& text) {
		return json::parse(R"({"version": 2,
			"strings": {"en": {"files": "{n, plural, one {# file} other {# files}}"}, "it": {"hi": "Ciao"}},
			"state": {"count": 2, "key": "hi"},
			"root": {"children": [{"type": "label", "text": ")" +
		                   text + R"("}]}})");
	};
	CHECK(findings(document("{{t('files', {n: state.count})}}")).empty());
	CHECK(findings(document("{{t('hi')}}")).empty()); // in one locale is enough
	CHECK(findings(document("{{t(state.key)}}")).empty());
	CHECK(anyHas(findings(document("{{t('nope')}}")), "unknown key 'nope'"));
	CHECK(anyHas(findings(document("{{t('files')}}")), "reads the parameter 'n', which isn't given"));
	CHECK(anyHas(findings(document("{{t('files', {n: 1, m: 2})}}")), "'files' has no parameter 'm'"));
	CHECK(anyHas(findings(document("{{locales}}")), "locale"));
	CHECK(findings(document("{{locale}}")).empty());
}
