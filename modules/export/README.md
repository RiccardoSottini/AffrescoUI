# export: panel trees as vector documents

`export` writes what a window shows as a vector PDF: any laid-out panel tree becomes a page whose rectangles, rounded
corners, borders, shadows, vector paths and text are paths and real text, not pixels. Text is searchable and can be
copied: its fonts are subset and embedded with a ToUnicode map. An app exports a slide deck, a vector page or a report
by laying out each page in a panel and adding it. Target: `affrescoui::export` (uses `affrescoui::render`).

## Public headers (`include/affresco/export/`)

| Header | Offers |
|---|---|
| `PdfDocument.hpp` | `PdfDocument` (`addPage` of a panel, or a part of it, fitted to a page size in points; `pageCount`; `finish` to bytes; `save` to a file), `PdfLimits` |

## Invariants

- **What the window draws.** A page replays the panel tree as the renderer does: hidden panels and overlays left out,
  each panel clipped to its bounds, transforms (rotation, scale) applied, `paint` then the children then `paintOver`.
  Each `Painter` call has a vector form: rectangles and rounded rectangles as paths (corners as Bézier curves),
  outlines as rings (even-odd), meshes as the outline where their coverage crosses one half, images as RGB with an
  alpha mask, shadows as a few growing layers whose alphas add up to the shadow's colour. A GPU surface draws nothing.
- **Real text.** Glyphs come from the font's own files (`Font::faceSources`, `Font::glyphSource`): each face is
  subset with HarfBuzz keeping glyph indices, embedded as a CID font (TrueType or CFF) with its widths, and mapped back
  to Unicode for search and copy. Synthetic bold and italic look as on screen; colour glyphs (emoji) are images.
- **Valid structure.** PDF 1.7 with a classic cross-reference table whose offsets are exact; every stream has its
  length; one resource dictionary for all pages; the title is UTF-16.
- **Bounded.** `PdfLimits` caps pages, the file, embedded fonts, image pixels and one page's content; past one,
  `std::runtime_error` names it (SECURITY.md, "Limits").

## Decision: our own writer

PDFio and libharu were weighed against a small writer of our own. libharu has no OpenType/CFF subsetting and its own
font handling; PDFio is a good reader and writer but would be a new dependency for what is here a few hundred lines.
HarfBuzz, already built for shaping, has the subsetter (`hb-subset`), so the writer adds no dependency: only the
subsetter's sources of the same pinned HarfBuzz are compiled (`affrescoui_harfbuzz_subset`). Streams are not
compressed (no zlib), which keeps the writer simple at the cost of larger files.

## Threads

Main thread, as the panels it reads. A document is used by one thread at a time.

## Errors

`addPage`, `finish` and `save` throw `std::runtime_error`: past a `PdfLimits` cap (the message names it), for a page
size outside 1 to 14,400 points, after `finish`, or for a file that can't be written.

`save` writes the path the application gives it and replaces a file there: no screen chooses it, so confining it (to a
folder the user picked, say) is the application's.

## Extension points

- **Another vector format:** derive from `render`'s `Painter`, as `PdfPainter` does, and walk the panel tree as
  `PdfDocument::addPage` does.
- **Compression:** streams are written by `PdfObjects`; a filter (`/FlateDecode`) would go there, with its dependency.

## Files

```
include/affresco/export/PdfDocument.hpp   the public API
src/
  PdfDocument.cpp          pages drawn by walking panel trees through a PdfPainter, then the file's structure
  PdfPainter.hpp, .cpp     Painter calls as PDF drawing operators
  PdfShapes.hpp, .cpp      numbers, strings and paths in PDF syntax
  PdfResources.hpp, .cpp   images, transparency states, the shared resource dictionary
  PdfFonts.hpp, .cpp       font files subset with HarfBuzz and embedded as CID fonts, with ToUnicode maps
  PdfObjects.hpp, .cpp     numbered objects written in order, the cross-reference table
```
