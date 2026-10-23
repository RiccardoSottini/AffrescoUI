# Fonts for the rendering check

Subsets of open fonts, holding only the characters the check's `scripts` screen shows in their script (and, for
`NotoSansArabic-Assistant.ttf`, those of the assistant add-on's Arabic strings), so shaping, fallback by coverage and
colour emoji are tested with a few kilobytes of fonts. `subset.py` makes them again from the originals with HarfBuzz's
`hb-subset`, which keeps the layout tables shaping needs (joining forms, conjuncts, ligatures, mark positions). Variable
fonts are pinned to their regular instance (weight 400, width 100).

| Subset | Original (SHA-256 of the file used) | License |
|---|---|---|
| `NotoSansArabic-Subset.ttf` | `ofl/notosansarabic/NotoSansArabic[wdth,wght].ttf` (`63111b5b…e869e`) | SIL Open Font License 1.1: `LICENSE-NotoSansArabic.txt` |
| `NotoSansArabic-Assistant.ttf` | the same | the same |
| `NotoSansDevanagari-Subset.ttf` | `ofl/notosansdevanagari/NotoSansDevanagari[wdth,wght].ttf` (`14ec4af4…9853e`) | SIL Open Font License 1.1: `LICENSE-NotoSansDevanagari.txt` |
| `NotoSansSC-Subset.ttf` | `ofl/notosanssc/NotoSansSC[wght].ttf` (`a3041811…af0da`) | SIL Open Font License 1.1: `LICENSE-NotoSansCJK.txt` |
| `NotoSansJP-Subset.ttf` | `ofl/notosansjp/NotoSansJP[wght].ttf` (`c2f3b4d4…fc68f`) | SIL Open Font License 1.1: `LICENSE-NotoSansCJK.txt` |
| `NotoSansKR-Subset.ttf` | `ofl/notosanskr/NotoSansKR[wght].ttf` (`194018e6…e252`) | SIL Open Font License 1.1: `LICENSE-NotoSansCJK.txt` |
| `TwemojiMozilla-Subset.ttf` | `Twemoji.Mozilla.ttf`, release v0.7.0 of https://github.com/mozilla/twemoji-colr (`6d90152e…9ee8e`) | Art: CC BY 4.0 (Twitter's Twemoji); font build: Apache License 2.0: `LICENSE-TwemojiMozilla.md` |

The Noto fonts come from https://github.com/google/fonts (the `main` branch on 2026-10-19). The subsets are modified
versions under the Open Font License: they keep the fonts' names (Noto Sans is not a Reserved Font Name; the CJK fonts'
reserved name is "Source", which they don't use), and they are only distributed with this software. The Twemoji art
is © Twitter, Inc. and other contributors, used under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/); the
subset keeps 13 of its emoji, unchanged.

Twemoji Mozilla is a COLR (layered colour) font, which FreeType draws in colour by itself. Bitmap colour fonts (CBDT,
such as Noto Color Emoji, and Apple's sbix) would need libpng, which this build doesn't have.
