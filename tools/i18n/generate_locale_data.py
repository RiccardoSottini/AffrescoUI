#!/usr/bin/env python3
"""Generates modules/format/src/locale/LocaleData.cpp: the compact CLDR subset expression functions format with.

Reads the cldr-json npm packages pinned below, straight from their .tgz files (nothing is extracted), after checking
each one's SHA-256. Writes, for every locale of LOCALES: number symbols and patterns, the cardinal plural rules,
currency symbols, Gregorian month and day names, date, time and date-time patterns, relative-time patterns and list
patterns. The struct it fills is LocaleData (modules/format/src/locale/Locale.hpp): change both together.

    python3 -I tools/i18n/generate_locale_data.py DIR [--download] [-o modules/format/src/locale/LocaleData.cpp]

DIR holds core.tgz, dates-full.tgz, misc-full.tgz and numbers-full.tgz; --download fetches the missing ones from
the npm registry first. The packages are data, never code: they are only parsed as JSON. Only the Python standard
library is needed.
"""

import argparse
import hashlib
import json
import os
import sys
import tarfile
import unicodedata
import urllib.request

CLDR_VERSION = "46.0.0"
PACKAGES = {  # cldr-<name>, and its .tgz's SHA-256
    "core": "c8187b1b9770331bfbc56c16b172ce1172895c0a37daa90ab354576f83593137",
    "dates-full": "f2177c0db4c1d74c7fda25bbacb4434952e980eb1d16e6c75a130616878c77de",
    "misc-full": "be20e3a5c9af5f1389565d22a11598ec9f28f9d010b7334d9b0261508ce86e61",
    "numbers-full": "383015394056d071dcc832e9aa7474ebc03058033cd0f909a461216bc036f184",
}
LOCALES = ["en", "it", "fr", "de", "es", "pt", "ja", "zh", "ar", "he"]
CURRENCIES = ["USD", "EUR", "GBP", "JPY", "CNY", "CHF", "CAD", "AUD", "BRL", "INR", "ILS", "SAR", "EGP", "AED"]
PLURALS = ["zero", "one", "two", "few", "many"]  # "other" is what none of them matches
COUNTS = PLURALS + ["other"]
UNITS = ["year", "month", "week", "day", "hour", "minute", "second"]
WIDTHS = ["full", "long", "medium", "short"]
DAYS = ["sun", "mon", "tue", "wed", "thu", "fri", "sat"]
LIMIT = 64 * 1024 * 1024  # bytes read from one member: the packages' files are far smaller


def read_package(path, digest):
    """Checks a package's SHA-256 and returns its members as {name: bytes}, for the .json ones only."""
    with open(path, "rb") as file:
        data = file.read()
    if hashlib.sha256(data).hexdigest() != digest:
        sys.exit(f"{os.path.basename(path)}: SHA-256 differs from the pinned one")
    members = {}
    with tarfile.open(path, "r:gz") as archive:
        for member in archive:
            if member.isfile() and member.name.endswith(".json") and member.size <= LIMIT:
                members[member.name.removeprefix("package/")] = archive.extractfile(member).read()
    return members


def download(directory):
    """Fetches the packages missing from a directory."""
    for name in PACKAGES:
        path = os.path.join(directory, f"{name}.tgz")
        if not os.path.exists(path):
            url = f"https://registry.npmjs.org/cldr-{name}/-/cldr-{name}-{CLDR_VERSION}.tgz"
            with urllib.request.urlopen(url, timeout=60) as response, open(path, "wb") as file:
                file.write(response.read(LIMIT))


class Cldr:
    """The packages' JSON files, read on demand."""

    def __init__(self, directory):
        self.members = {}
        for name, digest in PACKAGES.items():
            self.members.update(read_package(os.path.join(directory, f"{name}.tgz"), digest))

    def main(self, locale, file):
        """main/<locale>/<file>.json, below its locale's key."""
        return json.loads(self.members[f"main/{locale}/{file}.json"])["main"][locale]

    def supplemental(self, file):
        """supplemental/<file>.json, below "supplemental"."""
        return json.loads(self.members[f"supplemental/{file}.json"])["supplemental"]


def literal(text):
    """A C++ string literal: UTF-8 as is, invisible characters (marks, special spaces) as \\u escapes."""
    out = []
    for character in text:
        if character in '"\\':
            out.append("\\" + character)
        elif character == " " or unicodedata.category(character) not in ("Cf", "Zs", "Cc"):
            out.append(character)
        else:
            out.append(f"\\u{ord(character):04x}")
    return '"' + "".join(out) + '"'


def strings(values):
    """A braced list of literals."""
    return "{" + ", ".join(literal(value) for value in values) + "}"


def numbers(cldr, locale):
    """Symbols and patterns of the locale's default numbering system."""
    data = cldr.main(locale, "numbers")["numbers"]
    system = data["defaultNumberingSystem"]
    symbols = data[f"symbols-numberSystem-{system}"]
    return {
        "decimal": symbols["decimal"],
        "group": symbols["group"],
        "percentSign": symbols["percentSign"],
        "minusSign": symbols["minusSign"],
        "grouping": int(data["minimumGroupingDigits"]),
        "decimalPattern": data[f"decimalFormats-numberSystem-{system}"]["standard"],
        "percentPattern": data[f"percentFormats-numberSystem-{system}"]["standard"],
        "currencyPattern": data[f"currencyFormats-numberSystem-{system}"]["standard"],
    }


def plurals(cldr, locale):
    """The conditions of the cardinal categories zero to many, without their samples; "" for a category not used."""
    rules = cldr.supplemental("plurals")["plurals-type-cardinal"][locale]
    return [rules.get(f"pluralRule-count-{name}", "").split("@")[0].strip() for name in PLURALS]


def currencies(cldr, locale):
    """The symbol of each currency of CURRENCIES."""
    data = cldr.main(locale, "currencies")["numbers"]["currencies"]
    return [data.get(code, {}).get("symbol", code) for code in CURRENCIES]


def calendar(cldr, locale):
    """Month and day names, day periods, and the date, time and date-time patterns."""
    data = cldr.main(locale, "ca-gregorian")["dates"]["calendars"]["gregorian"]
    months, days = data["months"]["format"], data["days"]["format"]
    periods = data["dayPeriods"]["format"]["abbreviated"]
    return {
        "months": [months["wide"][str(i)] for i in range(1, 13)],
        "shortMonths": [months["abbreviated"][str(i)] for i in range(1, 13)],
        "days": [days["wide"][day] for day in DAYS],
        "shortDays": [days["abbreviated"][day] for day in DAYS],
        "periods": [periods["am"], periods["pm"]],
        "dates": [data["dateFormats"][width] for width in WIDTHS],
        "times": [data["timeFormats"][width] for width in WIDTHS],
        "dateTimes": [data["dateTimeFormats"][width] for width in WIDTHS],
    }


def relative(cldr, locale):
    """Per unit: "last", "this" and "next" (or ""), then the future and past patterns per plural category."""
    fields = cldr.main(locale, "dateFields")["dates"]["fields"]
    units = []
    for unit in UNITS:
        field = fields[unit]
        words = [field.get(f"relative-type-{offset}", "") for offset in ("-1", "0", "1")]
        future, past = field["relativeTime-type-future"], field["relativeTime-type-past"]
        pattern = lambda side, count: side.get(f"relativeTimePattern-count-{count}", "")
        units.append((words, [pattern(future, c) for c in COUNTS], [pattern(past, c) for c in COUNTS]))
    return units


def lists(cldr, locale):
    """The "and" and "or" list patterns: start, middle, end, and two items."""
    data = cldr.main(locale, "listPatterns")["listPatterns"]
    parts = ("start", "middle", "end", "2")
    return [[data[f"listPattern-type-{kind}"][part] for part in parts] for kind in ("standard", "or")]


def locale_entry(cldr, locale):
    """One LocaleData initializer."""
    number = numbers(cldr, locale)
    dates = calendar(cldr, locale)
    rtl = cldr.main(locale, "layout")["layout"]["orientation"]["characterOrder"] == "right-to-left"
    units = ",\n         ".join(
        f"RelativeUnit{{{strings(words)}, {strings(future)}, {strings(past)}}}" for words, future, past in relative(cldr, locale))
    and_list, or_list = lists(cldr, locale)
    fields = [
        f".tag = {literal(locale)}",
        f".rightToLeft = {'true' if rtl else 'false'}",
        *(f".{name} = {literal(number[name])}" for name in ("decimal", "group", "percentSign", "minusSign")),
        f".minimumGroupingDigits = {number['grouping']}",
        *(f".{name} = {literal(number[name])}" for name in ("decimalPattern", "percentPattern", "currencyPattern")),
        f".plurals = {{{strings(plurals(cldr, locale))}}}",
        f".currencySymbols = {{{strings(currencies(cldr, locale))}}}",
        *(f".{name} = {{{strings(dates[name])}}}" for name in ("months", "shortMonths", "days", "shortDays")),
        f".am = {literal(dates['periods'][0])}",
        f".pm = {literal(dates['periods'][1])}",
        f".dateFormats = {{{strings(dates['dates'])}}}",
        f".timeFormats = {{{strings(dates['times'])}}}",
        f".dateTimeFormats = {{{strings(dates['dateTimes'])}}}",
        f".relative = {{{{{units}}}}}",
        f".andList = {{{strings(and_list)}}}",
        f".orList = {{{strings(or_list)}}}",
    ]
    return "    {" + ",\n     ".join(fields) + "}"


def generate(cldr):
    """The whole file."""
    fractions = cldr.supplemental("currencyData")["currencyData"]["fractions"]
    default_digits = int(fractions["DEFAULT"]["_digits"])
    digits = [int(fractions.get(code, {}).get("_digits", default_digits)) for code in CURRENCIES]
    entries = ",\n".join(locale_entry(cldr, locale) for locale in LOCALES)
    return f"""/// @file
/// @brief The compact CLDR subset: number, date, plural, relative-time and list data of {len(LOCALES)} locales.
///
/// Generated by tools/i18n/generate_locale_data.py from CLDR {CLDR_VERSION} (cldr-json, Unicode License v3): don't
/// edit by hand, run the generator.

#include "Locale.hpp"

#include <array>

namespace affresco::format::detail {{
namespace {{

// clang-format off
/// Every locale, in the generator's order.
constexpr std::array<LocaleData, {len(LOCALES)}> locales{{{{
{entries}}}}};
// clang-format on

}} // namespace

const std::array<std::string_view, currencyCount> currencyCodes{strings(CURRENCIES)};
const std::array<int, currencyCount> currencyDigits{{{", ".join(map(str, digits))}}};

std::span<const LocaleData> localeData() noexcept {{ return locales; }}

}} // namespace affresco::format::detail
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", help="where core.tgz, dates-full.tgz, misc-full.tgz and numbers-full.tgz are")
    parser.add_argument("--download", action="store_true", help="fetch the missing packages from the npm registry")
    parser.add_argument("-o", "--output", default="modules/format/src/locale/LocaleData.cpp")
    arguments = parser.parse_args()
    if arguments.download:
        download(arguments.directory)
    text = generate(Cldr(arguments.directory))
    with open(arguments.output, "w", encoding="utf-8") as file:
        file.write(text)


if __name__ == "__main__":
    main()
