# tools/i18n: the compact CLDR subset

`generate_locale_data.py` writes `modules/format/src/LocaleData.cpp`, the locale data the expression functions
format with (`formatNumber`, `formatDate`, `formatRelative`, `formatList`, plural rules for `t()`, ...): number
symbols and patterns, cardinal plural rules, currency symbols, Gregorian month and day names, date and time patterns,
relative-time patterns and list patterns of ten locales (en, it, fr, de, es, pt, ja, zh, ar, he).

## Source

CLDR 46.0.0, as the `cldr-json` npm packages, pinned by SHA-256 in the script:

| Package | SHA-256 |
|---|---|
| `cldr-core` | `c8187b1b9770331bfbc56c16b172ce1172895c0a37daa90ab354576f83593137` |
| `cldr-dates-full` | `f2177c0db4c1d74c7fda25bbacb4434952e980eb1d16e6c75a130616878c77de` |
| `cldr-misc-full` | `be20e3a5c9af5f1389565d22a11598ec9f28f9d010b7334d9b0261508ce86e61` |
| `cldr-numbers-full` | `383015394056d071dcc832e9aa7474ebc03058033cd0f909a461216bc036f184` |

The packages are data: the script reads their JSON members straight from the `.tgz` files (nothing is extracted)
after checking each hash, and needs only Python's standard library.

## Running it

```
mkdir -p out/cldr
python3 -I tools/i18n/generate_locale_data.py out/cldr --download
```

`--download` fetches the missing packages from the npm registry into the folder given (keep it outside `tools/`);
`-o` writes elsewhere than `modules/format/src/LocaleData.cpp`. To add a locale or a currency, extend `LOCALES` or
`CURRENCIES`; to add a field, change `LocaleData` (`modules/format/src/Locale.hpp`) and the script together.

## License

CLDR's data is under the Unicode License v3 (`LICENSE-Unicode.txt`, taken from `cldr-core`); the generated file
names it. The script is part of AffrescoUI (Apache License 2.0).
