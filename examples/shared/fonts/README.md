# Fonts

`Roboto-Regular.ttf` from https://github.com/googlefonts/roboto (`src/hinted/`), licensed under the
Apache License 2.0: see `LICENSE-Roboto.txt`.

`JetBrainsMono-Regular.ttf` from https://github.com/JetBrains/JetBrainsMono (`fonts/ttf/`), licensed under the SIL Open
Font License 1.1: see `LICENSE-JetBrainsMono.txt`.

`Lucide.ttf` is `font/lucide.ttf` of the npm package `lucide-static` 1.48.0 (https://lucide.dev, 1,890 icons named
like `search` and `arrow-left`), licensed under the ISC License, with the icons derived from Feather under the MIT
License: see `LICENSE-Lucide.txt`. The build checks its SHA-256,
`eaf30a2a0d2ec0cfbf7ab69ed6dc6e39b7e5e2f0255c17856dcb0b73a15584bb`. Its glyphs are named, so a label's `"icon"` finds
them by name (`SCREEN_FORMAT.md`).

`NotoSansArabic-Subset.ttf` is the Arabic block (U+0600 to U+06FF, U+0750 to U+077F, the presentation forms and the
joining controls) of `ofl/notosansarabic/NotoSansArabic[wdth,wght].ttf` from https://github.com/google/fonts (SHA-256
`63111b5b…e869e`, the same original as `tests/render/fonts/`), pinned to weight 400 and width 100 with HarfBuzz's
`hb-subset`, licensed under the SIL Open Font License 1.1: see `LICENSE-NotoSansArabic.txt`. The templates use it as a
fallback of their text fonts, for their Arabic string tables.
