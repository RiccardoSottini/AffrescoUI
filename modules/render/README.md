# render: the rendering engine

`render` draws a tree of panels in a window and routes input to them. It owns the windowing system (GLFW, behind an
internal platform layer), the OpenGL 4.1 core or OpenGL ES 3.0 context ("Backends") and the renderer (behind an
internal interface), and knows nothing about documents: you build the tree in C++, or `format` builds
it from a screen document. It keeps the ideas of 2017: nested panels, pixel and percentage sizes, anchors to edges,
minimum sizes, hex colours and signal-to-slot events. It adds rows, columns and grids, scrolling, overlays, rounded
shapes with borders and shadows, shaped text with fallback fonts and colour emoji, images and icons, and a multi-line editor. No public
header exposes OpenGL, GLFW, FreeType, HarfBuzz or stb_image. Target: `affrescoui::render` (uses `core`; vendors glad from
`external/`; links FreeType and HarfBuzz, pinned in `cmake/AffrescoUIText.cmake`).

## Public headers (`include/affresco/render/`)

| Header | Offers |
|---|---|
| `Application.hpp` | `Application`: the windowing system, every `Window`, the event loop (`run`, `quit`), second windows sharing a window's context and font (`createWindow(..., sharing)`), their clock (a `SteadyClock`); the `Executor` (`post`); `ApplicationOptions` (another clock, hidden windows: for automation) |
| `Window.hpp` | `Window`: the root panel, focus, hover, mouse capture, clipboard, tooltips; the desktop: files dropped from the OS (`onDrop`), the display's scale, the system's colour scheme and its reduced-motion setting (`contentScale`, `colorScheme`, `reducedMotion`, `onDisplay`), a native menu bar (`setMenuBar`, `MenuItem`); a focus ring drawn around the focused panel only after the keyboard moved the focus (`setFocusRing`, `FocusRing`, `focusVisible`; on by default in screens); the OS's file dialogs (`chooseFiles`, `FileDialog`, `FileFilter`; `setFileDialogHandler` answers them instead, for tests and automation); the `route...` input methods; `clock`, `defer`, `runDeferredTasks`, `setTimer`, `cancelTimer`; `render`, `saveScreenshot`, `renderToImage` (a panel's subtree as a bitmap: thumbnails, mirrors); for the inspector, `frameStatistics` (`FrameStatistics.hpp`: frames drawn, the last one's time and draw calls) and `setInspection`, a layer drawn above everything and never hit |
| `Accessibility.hpp` | What a panel is to a person or a screen reader: `accessibleRole` (the panel's own, or one from its kind: a button, a checkbox, a textbox, text), `accessibleName` (its label, else its text or the text inside it, else its tooltip), `accessibilityTree` (`AccessibleNode`: shown panels with a role, in tree order), `findByRole`; each element's value (text, a slider's `AccessibleRange`, a checkbox's state, a field's selection), states (`AccessibleState`: expanded, selected, disabled, a live region), the actions it allows, bounds and an id that lasts (`accessibleId`, `findByAccessibleId`), and `performAccessibleAction`. The roles and names are set on a `Panel` (`setAccessibleRole`, `setAccessibleLabel`, `setAccessibleDescription`, `setAccessibleState`); each window hands the tree to the platform after the frames that change it (`ACCESSIBILITY.md`) |
| `Clock.hpp` | `Clock`, what a window's timers, tooltip delay and double clicks read, and the animations of what is built into it; `SteadyClock` |
| `Panel.hpp` | `Panel`: the owned tree, `Length` positions and sizes, anchors (also to another panel's edge), minimum sizes, `grow`, scrolling, overlays, shapes, cursors, tooltips, event signals and hooks; `setCached` (the subtree drawn from a texture); `setGradient` (a background over its colour) |
| `Stack.hpp` | `Stack` (a row or a column, like a CSS flex container: justify, alignItems, gaps, grow, shrink, basis, wrap) and `Grid` (column and row tracks, cells and spans) |
| `Layout.hpp` | The values of flexible and grid layout: `Justify`, `CrossAlign`, `Track` (pixels, percent, fractions, content, minmax), `GridCell` |
| `Label.hpp` | `Label`: text with padding, alignment, wrap, and fit to text; an icon before it (`setIcon`, a glyph named in its font: `Font::codepointNamed`); rich text (`setRuns`), links (`onLink`) and selection with copy (`setSelectable`) |
| `RichText.hpp` | `TextRun` (a part of a text with its own font, colour, link or inline icon) and `TextLayout` (runs broken into lines across styles, hit testing, carets) |
| `TextArea.hpp` | `TextArea`: a single-line input |
| `TextEditor.hpp` | `TextEditor`: multi-line editing, undo, clipboard, a gutter; a colouring hook for subclasses; its text is a `TextModel` (`model()`); an `EditorHighlighter` (`setHighlighter`) colours lines, folds and pairs brackets as it follows the model's edits. Word wrap (`setWrap`), folding (`fold`, `unfold`, `toggleFold`, `setFolds`, `offerFolds`), decorations (`EditorDecoration`: background, squiggle, gutter icon, inline text), extra cursors and column selection (`addCursor`, Cmd/Ctrl+D, Cmd+Alt+Up/Down, Alt+drag), find and replace (`setFind`, `findNext`, `replaceNext`, `replaceAll`, `matchCount`; Cmd/Ctrl+G), bracket matching, indentation guides and an IME composition (`setComposition`); `setLineLabels` numbers rows its own way |
| `DiffView.hpp` | `DiffView`: two texts compared in read-only editors, side by side (rows aligned, scrolled together) or inline (`setMode`), each text's line numbers, changed lines and characters marked (`DiffColors`); an `EditorFactory` gives syntax-coloured editors |
| `TextModel.hpp` | `TextModel`: UTF-8 text of any size as a piece tree, with a line index (`line`, `offsetOf`, `positionOf`), O(log n) edits, undo as operations (`beginStep`, `endStep`, `undo`) and `onChanged` (`TextChange`); `TextLines`, its lines decoded and kept for reading again |
| `Controls.hpp` | `Toggle` (a switch or a checkbox) and `Slider` |
| `ColorSpaces.hpp` | `color::` conversions between sRGB, linear sRGB, HSV, HSL, OKLab and OKLCH, and gamut mapping into sRGB |
| `ColorPicker.hpp` | `ColorPicker`: a saturation-value square and hue strip, or a wheel and value strip; an alpha strip; hex, RGB, HSL, HSV and OKLCH fields; swatches and recent colours (at most 32 and 8); an eyedropper limited to the window; arrows act in the part Tab is on |
| `Gradient.hpp` | `Gradient`: a linear (at an angle) or radial gradient of at most 32 `GradientStop`s; `normalizeGradient`, `sampleGradient`, `gradientPosition` |
| `GradientEditor.hpp` | `GradientEditor`: a `Gradient` edited on a bar, each stop's colour in a `ColorPicker` popover |
| `CurvesEditor.hpp` | `CurvesEditor`: tone curves for RGB, R, G and B (at most 32 points each) over an optional histogram; `evaluateCurve` (monotone, linear or Bézier) |
| `Split.hpp` | `Split`: panes along a row or a column with sashes between them; sizes within minimums, the last pane taking the rest; dragged, reset by a double click or Home, moved by the arrow keys (PageUp and PageDown pick a sash); `onSizesChanged` |
| `Tabs.hpp` | `Tabs`: a strip of `Tab`s, one active; it scrolls when they overflow, closes (a button or Delete), reorders by dragging (`onMoved`), shows where a tab dragged from another strip would go (`insertionAt`, `showDropIndicator`), and moves with the arrow keys, Home and End |
| `RowWindow.hpp` | `RowWindow`: which rows of a long list are in view and where each starts, for one shared virtualisation; fixed heights in O(1), measured ones in O(log n) |
| `ListView.hpp` | `ListView`: rows of a `ItemModel` (`ItemList`, or an application's own model), drawing only those in view, each with an optional icon (`ListItem::icon`, `iconColor`); single or multiple selection kept by id across new items, arrow keys, pages, type-to-find, `onOpened`, a drop line or outline (`dropTargetAt`); once scrolled, the top row stays in place when items are added above it |
| `TreeView.hpp` | `TreeView`: a `ListView` over a pre-order tree, showing only the open nodes' rows, with indent guides and chevrons; Right and Left open, close and walk; `onExpandedChanged`; the open folders of the top row pinned as it scrolls (`setStickyRows`) |
| `TableView.hpp` | `TableView`: a `ListView` of rows of cells under a header; a header click sorts (numbers before text, stable, again to reverse: `onSortChanged`), a header edge drag resizes a column (`onColumnResized`); the order and widths given replace the user's only when they change |
| `GraphView.hpp` | `GraphView`: a node editor over a `GraphModel` (nodes with typed ports, links); the link rules (`checkLink`: kinds, `links` per port, duplicates, cycles), a small camera (`GraphCamera`, zoom and pan, to move onto Camera2D), Bézier links coloured by kind, a minimap, off-screen nodes culled; drags, a marquee and the keyboard emit `GraphEvent`s (`onEdit`) and change nothing themselves |
| `TimelineView.hpp` | `TimelineView`: tracks of keyframes and clips under a time ruler over a `TimelineModel` (nested groups, mute and lock, markers, a play range); ticks that follow the zoom in frames, seconds or timecode (`rulerTicks`, `formatTime`), snapping (`snapTime`), step, linear and Bézier easing (`keyframeValueAt`); only the rows in view laid out and drawn; scrubbing, keyframe drags, clip moves and trims, a marquee and the keyboard emit `TimelineEvent`s (`onEdit`) |
| `Font.hpp` | `Font`: a font and its fallbacks at one size, in a style (`FontStyle`: bold, italic, synthesised from the outlines); `shape` (glyphs, advances, carets), `measure`, `wrap`; glyphs rasterised on demand into coverage and colour atlas pages; `rasterAt` (its glyphs rasterised for a HiDPI framebuffer, laid out the same) |
| `FontCache.hpp` | `FontCache`: fonts by file, size and fallbacks, within a bound (32 fonts, 128 MiB), the least recently used that nothing holds dropped first; the system's fallback fonts when the host asks |
| `Painter.hpp` | `Painter`: what `paint` draws with (rectangles, rounded rectangles, borders, shadows, text, images, meshes of anti-aliased quads with `fillMesh`, gradients with `fillGradient`); `CornerRadii`, `Shadow`, `MeshVertex` |
| `Bitmap.hpp` | `Bitmap`: an image's premultiplied RGBA pixels, immutable and shared; `load` and `decode` (PNG and JPEG with stb_image, SVG icons rasterised at twice their size) and `fromPixels`, within `Bitmap::Limits` |
| `ImageCache.hpp` | `ImageCache`: files decoded on a worker thread and delivered on the main one (through an `Executor`), only from allowed folders, kept within a bound; images the host names; `Request`s that cancel when destroyed |
| `ImageView.hpp` | `ImageView`: a panel showing a bitmap, a file or a named image, with `ImageFit` (`contain`, `cover`, `fill`) |
| `VectorPath.hpp` | `VectorPath`: SVG path data parsed within `VectorPath::Limits` (or built with `moveTo`, `lineTo`, `cubicTo`, `close`) and flattened; `appendFill` (`FillRule`) and `appendStroke` (`StrokeStyle`: width, `LineJoin`, `LineCap`, dashes), the meshes that draw it, within `meshQuadLimit` and `meshWorkLimit` |
| `PathView.hpp` | `PathView`: a panel drawing a path, filled and stroked, fitted to it through a view box with `ImageFit`; its meshes are built again only when the path, the style or the size changes |
| `Camera2D.hpp` | `Camera2D`: a pure value (x, y, zoom within 0.01 to 256) mapping world units to screen units: `toScreen`, `toWorld`, `visible`, `zoomedAt` (the point under the pointer stays), `pannedBy`, `fitted` |
| `CanvasView.hpp` | `CanvasView`: an infinite canvas. Children placed in world units (all in `placeChildren()`, to become a transform); items from a `CanvasItemModel` (up to 1,000,000) culled through a spatial index; levels of detail; pan, zoom at the pointer, `fit`; selection by click, Shift-click and marquee, handles to move, resize and rotate (`onTransform`, the document applies it); snapping to the grid, items and guides, with smart guides; rulers whose ticks follow the zoom, guides dragged from them; `onViewChanged` |
| `Color.hpp`, `Geometry.hpp`, `Length.hpp` | `Color` (hex in and out), `Vec2`, `Rect`, `Length` with the `_px` and `_pct` literals |
| `Events.hpp`, `Input.hpp` | The typed events (`KeyEvent`, `MouseButtonEvent`, ...) and the key and button names (GLFW's values) |

## Invariants

- **A parent owns its children** (`std::unique_ptr`); `parent()` and `window()` never own. Destroying a panel
  clears the window's focus, hover and capture if they point at it.
- **Setters only mark what changed.** A setter stores a value only when it differs (`Panel::setProperty`), and then
  marks the panel dirty and its ancestors as having a dirty descendant. Layout runs once, before the next frame, then
  the tree is drawn. Changes made during the layout pass itself (`prepareLayout`, `arrange`) don't schedule another
  one.
- **Layout is incremental.** Each panel keeps what its last pass gave it (origin, clip, the parent's size, the size its
  container arranged, whether its parent was shown). A pass skips a subtree whose inputs are the same and where
  nothing changed; subtrees with an anchor to another panel are always laid out, as that panel may move. Measured
  sizes (`measuredSize`) are kept until the panel changes. `Window::invalidate()` lays out and measures everything
  again. **A widget whose size or arrangement depends on anything else must call `invalidate()` when it changes.**
- **Flex and grid are pure functions** (`src/layout/Flex.*`, `src/layout/Grid.*`) of measured sizes, tested with
  tables of rectangles; `Stack` and `Grid` measure their children, call them, and turn the result into anchors and
  arranged sizes. Their arithmetic keeps the 2017 rows, columns and grids on exactly the same pixels. A grid has at
  most `maxGridTracks` (1,000) tracks a side, and placing a child looks at each of the grid's cells about once at most.
- **A window draws only when something changed,** and the event loop sleeps while nothing is due: idle costs no CPU.
- **A frame draws only what changed.** Each panel keeps what it drew (`src/paint/RetainedGeometry.hpp`: its clipped quads
  and the batch steps between them, recorded by `PaintRecorder`, the Painter panels draw with). Before a frame,
  `GeometryCache` visits only the panels whose drawing changed (`invalidatePaint()`, or `invalidate()`) and those a
  layout pass laid out since the last frame (their pass stamp), records again those that changed or whose frame,
  bounds, clip or scroll did, and gathers the damage: where each was and is, and where a hidden or destroyed panel
  was. The OpenGL renderer draws into canvases that keep their pixels (one for the window, copied to it each frame;
  one for screenshots), each only within its damage (scissor), or whole when it is new, the window was resized or
  invalidated, the frame needs other shaders, or most of it changed. Replaying the records gives the batches and
  vertices drawing directly gave, and which shaders a frame needs is known for the whole tree, so a frame has the
  pixels a full frame would. The rendering check proves it: every capture is also drawn from scratch
  (`detail::drawFromScratch`) and must be the same. **A widget whose drawing depends on anything else must call
  `invalidatePaint()` (or `invalidate()`) when it changes.** One renderer per window: it clears the panels' paint bits.
- **Writing direction is inherited** (6.9). `Panel::setWritingDirection` (`dir` in screens) sets a subtree right to
  left, left to right, or as the window's locale (`Window::setLocaleRightToLeft`); right to left mirrors rows, grid
  columns, anchors, positions and text alignment, and a label marked mirror swaps its icon's left and right
  (`tests/unit/rtl.cpp`).
- **Layout per axis:** anchored edges first, then the position, then the size within its minimum and maximum (the
  minimum wins), then the aspect ratio; percentages refer to the
  parent; a panel is clipped to its parent, and a panel clipped to nothing is not shown. Overlays are laid out, drawn
  and hit-tested after the whole tree, unclipped.
- **Every OpenGL call is in `src/gl/`, and every GLFW call in `src/platform/GlfwPlatform.cpp`** (and what GLFW
  doesn't do, the colour scheme, reduced motion and native menu bars, in `src/platform/Desktop*`: Cocoa on macOS,
  Win32 on Windows; elsewhere the scheme is light, motion is full and there is no native bar, so `setMenuBar` returns
  false). `Window.cpp` and
  `Application.cpp` call neither: the OS window is a `platform::NativeWindow` (`src/platform/Platform.hpp`), and
  drawing a `detail::Renderer` (`src/paint/Renderer.hpp`), whose OpenGL implementation (`gl/OpenGlRenderer.cpp`) uses
  `gl::QuadRenderer`: batched quads replayed from the panels' records, one draw call per texture switch, clipping on
  the CPU.
- **A window without a platform window or renderer is headless.** It routes input, keeps the focus, hover and
  capture, runs timers and deferred tasks and lays out, but never draws; its clipboard is its own.
  `detail::createHeadlessWindow` (`src/Headless.hpp`, internal for now) creates one for tests.
- **One clock per window.** Timers, the tooltip delay and double clicks read `Window::clock()`; an `Application`'s
  windows share a `SteadyClock` (or the clock its `ApplicationOptions` give), and tests and automation use
  `detail::ManualClock` to move time by hand.
- **One `Application` at a time.** A second one throws.
- **Text is shaped, then drawn.** `Font::shape` splits text into runs of one face and one script, each character
  from the first file of the font and its fallbacks that has it (fallbacks at the font's em size), and shapes each run
  with HarfBuzz; FreeType rasterises the glyphs, unhinted. A right-to-left run is laid out right to left, and a line's
  runs are put in visual order by their bidirectional levels (UAX #9, simplified: the paragraph's direction is its
  first strong character's, and numbers stay left to right). Shaped text is cached per font by line and by word, so a
  frame that draws what the last one drew shapes nothing, and measuring a new line of known words allocates nothing.
- **Plain words skip shaping, with HarfBuzz's results.** A word of the first face's plain Latin-1 characters
  (`text/PlainText`) is laid out from a table of advances with kerning, one read per character, without HarfBuzz or
  the caches. Which characters are plain comes from the face's GSUB and GPOS lookups (`text/LookupSpans` reads how
  many neighbours each one reads); what the lookups that read at most two do is taken from HarfBuzz itself, shaping
  each character and each pair alone the first time it is met. Everything else (ligatures such as Roboto's `fi`,
  contextual forms, marks, other scripts, fallback faces) is shaped as before. `tests/unit/plain_text.cpp` checks
  that both give the same glyphs, advances, carets and widths, to the last bit.
- **Text is sharp on any display, and lays out the same on all of them.** Sizes, shaping, measuring, wrapping,
  line heights, carets and hit testing are in window units. A frame with more framebuffer pixels per unit (a Retina
  display: 2; also `saveScreenshot` and `renderToImage` at a scale) draws its text from `Font::rasterAt(scale)`: the
  font's faces again, rasterising the same glyphs that many times denser into atlas pages of their own, on demand,
  each glyph's quad its bitmap's rectangle divided by the scale, the baseline on a pixel row. Scales round to a
  quarter from 1 to 4; at 1 (and below, for thumbnails) the font's own glyphs are drawn, as before. A font keeps one
  raster per scale it was drawn at, for as long as it lives, so windows on displays of different scales share it and
  moving a window back and forth rasterises nothing twice. A frame at another scale than the last records every panel
  again (`PaintKey::scale`). A raster's pages have a font's 64 MiB bound and count in its `memoryBytes()`; a font too
  large to rasterise at the scale (past 4,096 pixels) draws unscaled.
- **Colour glyphs share the text's batch.** Colour emoji (COLR fonts) go to RGBA atlas pages, which the renderer binds
  on a second texture unit next to the coverage page. A frame without colour glyphs (or without shapes) uses a shader
  without that code, so its pixels don't depend on it. A font's GPU pages are freed the first frame after it is gone.
- **Images are decoded off the main thread and drawn with the text.** An `ImageCache` decodes files on its worker
  thread and posts them to the main thread; files must be inside a folder it allows (resolved, links followed). On the
  GPU (`gl/ImageTextures`), images of up to 256 a side are packed into shared 1,024 × 1,024 RGBA pages with a border
  that repeats their edges, larger ones get a texture of their own, and both are freed the first frame after their
  `Bitmap` is gone (a page once no image on it is alive). An image is a colour quad bound where a font's colour page
  would be, next to the batch's coverage page, so text, icons and the small images of one page draw in one call.
- **A cached subtree is one quad.** A panel with `setCached(true)` has its subtree drawn into a texture of its own
  (`gl/SubtreeTextures`, a framebuffer object per subtree, premultiplied RGBA, within a budget of 64 Mi texels), then
  drawn as one colour quad of it every frame. It is drawn into it again only when a panel in it was recorded again or
  marked, or when its area changes; a subtree over the budget is drawn directly. Textures not used in a full frame
  are freed. Screenshots use the cache too; the frame the rendering check draws from scratch doesn't. Where the
  subtree is opaque the two are the same pixel for pixel; where its own edge is translucent (its rounded corners over
  what is behind) a pixel may be one level of 255 off, as the texture rounds it once more.
- **Icons are glyphs.** `Font::codepointNamed` finds a glyph by its PostScript name in the font and its fallbacks;
  with an icon font (Lucide) as a fallback of the text font, a label's icon comes from the text's own atlas.
- **Rich text is runs.** A label's `TextRun`s each name their font (a bold, italic or larger variant), colour, link
  or icon; `TextLayout` breaks them into lines at spaces across runs, each line as tall as its tallest font, and is
  made again only when the runs, the font or the width change. A press on a link emits `onLink`; a selectable label
  selects with the mouse and Cmd/Ctrl+A and copies with Cmd/Ctrl+C.
- **Limits:** a glyph atlas page is at most 4,096 × 4,096, and a font's pages (and each of its rasters') hold at most
  64 MiB in all (colour pages count four bytes a texel), so text naming many characters can't exhaust memory; past
  that, characters draw as `?`.
  A font file is a regular file of at most 256 MiB, and a font's size at most 4,096 pixels. `FontCache` keeps at most
  32 fonts and 128 MiB that nothing uses. A `TextModel` (a `TextEditor`'s text) keeps 500 undo steps, and at most
  256 MiB of removed text in them; invalid UTF-8 becomes U+FFFD as it comes in.
- **Vector bounds:** a path flattens to at most 65,536 points, and a fill or stroke makes at most 65,536 quads and
  16 Mi edge visits (`VectorPath.hpp`); a dashed stroke ends after 1 Mi dashes and gaps. An SVG icon is at most 1 MiB,
  4,096 elements nested 32 deep and 2,048 pixels a side, and its fills together visit at most 64 Mi edges. Past a
  bound a path is drawn in part, never slowly.
- **Large texts:** a `TextModel` copies a file once into buffers that are only appended to, and keeps pieces of them
  in a treap with the bytes and line feeds under each node, so an edit or finding a line costs O(log n). A 50 MB
  text opens in under a second, an edit takes under 50 µs, and memory stays under 1.5 times the text
  (`tests/unit/text_model.cpp`). The editor decodes and colours only the visible lines (the colouring state of each
  line above them is kept), and past 4,096 lines estimates its width from the longest line. Wrapped or folded, it
  keeps a row index (a line's first row, by binary search), so scrolling a million lines costs the same as ten
  thousand (`tests/perf/editor.cpp`: typing in 100,000 lines and scrolling 1,000,000, both well within 8 ms).
- **Editor bounds:** at most 10,000 cursors, 10,000 decorations (8 starting on a line, inline text 256 code points),
  10,000 folds, 10,000 matches counted or replaced and 10,000 highlighted, 1,000 on a line; bracket matching reads at
  most 100,000 code points and Cmd+D 100,000 lines. Find runs line by line, under `TextPattern`'s limits.
- **Diff bounds:** lines are diffed within core's `DiffLimits`; characters only on lines up to 2,000 code points, for
  at most 2,000 changed line pairs, 50,000 steps each.

## Backends

The renderer draws with one of two OpenGL backends, chosen when building (`cmake/AffrescoUIAngle.cmake`):

| `AFFRESCOUI_GL_BACKEND` | Context | Where |
|---|---|---|
| `gl41` (default) | OpenGL 4.1 core, the platform's driver | macOS, Linux, Windows |
| `gles3` | OpenGL ES 3.0 through EGL | ANGLE on macOS (Metal) and Windows (Direct3D 11), Mesa on Linux, WebGL2 in a browser |

The code in `src/gl/` calls only what both share, and every shader body is written once, in the common subset of GLSL
4.10 core and GLSL ES 3.00 (no implicit int-to-float conversions, `layout(location)` only on vertex inputs and
fragment outputs). `gl::shaderPrelude(stage)` (`src/gl/Shader.hpp`) adds what differs: the `#version` line and, for
OpenGL ES fragment shaders, `precision highp float` and `int`. `gl::linkProgram` takes whole sources and adds nothing.
The one call that had to change is reading the frame back: OpenGL ES reads only RGBA, so screenshots read RGBA and
drop the alpha.

**ANGLE on macOS and Windows.** ANGLE needs Chromium's tools to build, so the build doesn't fetch it. Build it
([ANGLE's dev setup](https://chromium.googlesource.com/angle/angle/+/HEAD/doc/DevSetup.md): `gn gen out/Release
--args='is_debug=false'`, then `autoninja -C out/Release libEGL libGLESv2`), or take the two libraries from a
Chromium-based browser of the same architecture. Then point the build at the folder that holds them:

```
cmake -S . -B build-angle -G Ninja -DAFFRESCOUI_GL_BACKEND=gles3 -DAFFRESCOUI_ANGLE_DIR=/path/to/angle/out/Release
cmake --build build-angle
AFFRESCOUI_SCREENSHOT=out.ppm ./build-angle/affrescoui_launcher
```

macOS needs `libEGL.dylib` and `libGLESv2.dylib`; Windows needs `libEGL.dll` and `libGLESv2.dll`. GLFW opens them by
path at run time, and asks ANGLE for Metal on macOS and Direct3D 11 on Windows (`src/platform/GlfwPlatform.cpp`).
If `libEGL` can't find `libGLESv2` on Windows, copy both DLLs next to the executables. Without
`AFFRESCOUI_ANGLE_DIR`, GLFW loads the system's EGL, which is Mesa's on Linux.

**Golden images.** Each backend has its own pixel-exact golden images: `python3 tests/render/check.py --backend gles3
--build build-angle` compares with `tests/render/golden-gles3/`. A frame is compared only within its backend, because
two drivers' rasterizers round differently (anti-aliased edges, gradients, text coverage). `.github/workflows/gles.yml`
checks every template on gles3 (Mesa EGL, llvmpipe), and its summary lists the frames that differ from gl41's.

## Threads

Everything here runs on the main thread, inside `Application::run`. There are two exceptions. `Application::post` may
be called from any thread: the task runs on the main thread, and the event loop wakes for it. This is how channels
deliver messages. Each `ImageCache` has a worker thread that only decodes files (`Bitmap::load`) and posts the
results that way; it is stopped and joined when its cache is destroyed, and what it posted then does nothing. Timers (`Window::setTimer`) and deferred tasks (`Window::defer`) run on the main thread too; an owner that
captures `this` in one must cancel its timers in its destructor.

Every OpenGL object (textures, framebuffers, buffers, programs) is owned by a `gl::` RAII name (`gl/GlObjects.hpp`)
held by the renderer of the window whose context made it, and is deleted with that context current. What panels share
with the renderer (a font's pages, an image's `Bitmap`, a surface's state) the renderer holds weakly, and frees on the
GPU the first frame after the owner is gone. A texture larger than the driver's `GL_MAX_TEXTURE_SIZE` fails in the
driver, not here: images are bounded by `Bitmap::Limits` and passes by `SurfaceFrame::maxPassSide`.

## Errors

- Failures of the platform or of OpenGL throw `std::runtime_error` (`Application`, `createWindow`). A second
  `Application` throws `std::logic_error`. A child position out of range throws `std::out_of_range`.
- Drawing and input never throw for bad data: a character no font has shows as `?`, and a failed screenshot returns
  false.
- Messages (GLFW errors, shader logs) go through the `core` log.

## Extension points

- **A widget is a `Panel` subclass:** draw in `paint` (and `paintOver`, drawn over the children), size yourself in
  `prepareLayout`, place your children in `arrange`, and react in the event hooks (`keyEvent`, `mouseButtonEvent`, ...)
  or through the signals (`onKey`, `onMouseButton`, ...).
- **Text editors:** `TextEditor`'s protected editing API and its `colorLine` hook let an application add a language
  without changing the library (`examples/shared/CodeEditor.*` adds syntax colours that way).
- **Highlighters:** an `EditorHighlighter` (`TextEditor::setHighlighter`) is told of every edit and colours each
  line; it may answer folds and bracket pairs too (the format's `SyntaxHighlighter` does, on tree-sitter).
- **Another platform or renderer:** implement `platform::Platform` and `NativeWindow`, or `detail::Renderer`
  (internal interfaces, kept small: 7.8 builds damage tracking on the renderer).
- **File dialogs:** `Window::setFileDialogHandler` answers `chooseFiles` with an application's own dialog, or a
  test's fixed answer; without one, nativefiledialog-extended shows the OS's (`src/platform/FileDialogs.cpp`).
- **Fonts:** a screen's fonts come from a `FontCache` (`format` owns one); an application can make its own, with its
  own limits and system fallbacks (`FontCache::systemFonts()`, or `AFFRESCOUI_SYSTEM_FONTS=1` for the default one).

## Files

```
include/affresco/render/   the public headers above
src/
  window/Application.cpp   the platform, the windows, the event loop, posted tasks
  window/Window.cpp        focus, the focus ring, the clipboard, timers, deferred tasks, frames
  window/WindowInput.cpp   input routing (each event checked first), hover, capture, tooltips, the cursor
  window/Accessibility.cpp roles, accessible names, the tree, values, states, actions, finding by role or id
  Headless.hpp / .cpp      a headless window and a manual clock, for tests (internal)
  platform/                Platform.hpp (the internal interface), GlfwPlatform.cpp (every GLFW call), Desktop.hpp
                           and DesktopMac.mm, DesktopWin32.cpp, DesktopOther.cpp (colour scheme, menu bar per OS),
                           FileDialogs.cpp (the OS's file dialogs, through nativefiledialog-extended),
                           Accessibility.hpp (the adapter a window owns) with AccessKitTree (the tree as AccessKit
                           updates, only the changed elements) and AccessibilityMac.mm, AccessibilityWindows.cpp,
                           AccessibilityLinux.cpp, AccessibilityNone.cpp (AFFRESCOUI_ACCESSIBILITY=OFF),
                           AccessKitText.hpp (text made safe for AccessKit: valid UTF-8, no NUL),
                           EmbeddedPlatformGl.cpp (the OpenGL state an embedding saves and restores)
  paint/                   no OpenGL: Renderer.hpp (detail::Renderer, what a window draws with),
                           RetainedGeometry.hpp (what a panel drew: quads, batch steps, what the drawing depended on),
                           PaintRecorder (the Painter panels draw with: clipped quads and batch steps, recorded),
                           GeometryCache (before a frame: the panels that changed or moved recorded again, the damage,
                           the shaders), WindowImage.cpp (a subtree drawn from scratch into an image)
  gl/                      everything OpenGL: Context (state, read-back, canvases, scissored regions), GlObjects
                           (RAII names), Shader, QuadShaders.hpp (the programs' GLSL), QuadRenderer (replays records
                           into batches and draws them), QuadRendererCache.cpp (cached subtrees), Stencil (clips that
                           are not axis-aligned), SubtreeTextures (cached subtrees' textures and framebuffers),
                           ImageTextures (image pages and textures), OpenGlRenderer (the Renderer: canvases with their
                           damage, a full frame as the fallback)
  gl/surfaces/             GPU surfaces: SurfaceView.cpp (the panel), SurfaceState.hpp (what it shares with the
                           renderer), Surfaces (their framebuffers and passes, and drawing into them), SurfaceGuard
                           (the GL state saved and restored around a surface), SurfacePrelude.cpp (the GLSL prelude)
  font/                    Font.cpp (loading, Latin-1 at load, glyphs, measuring and wrapping, the atlas, rasters at
                           a display's scale),
                           FontCache.cpp (the bounded cache, the system's fallback fonts), FontFace (one file at one
                           size, in FreeType and HarfBuzz), FontData.hpp (what a Font holds), GlyphAtlas (coverage and
                           colour pages), LookupSpans (how far GSUB and GPOS lookups reach, read with every offset
                           checked), FontOutlines.cpp (the files and glyphs behind a font, for vector output),
                           FontInspector.hpp (what tests see of a font)
  text/                    Shaping.cpp (runs, HarfBuzz, clusters into advances and carets), ShapedTextCache (LRU by
                           text), PlainText (words laid out without shaping), RichText.cpp (TextLayout: runs into
                           lines, hit testing, carets), PieceTree (the text model's treap of pieces over append-only
                           buffers, with line feeds counted)
  vector/                  VectorPath.cpp (parsing, flattening), PathFill (scanline trapezoids by the fill rule, and
                           the fringe), PathStroke.cpp (segments, joins, caps, dashes), VectorMath.hpp,
                           MeshRasterizer (meshes into premultiplied pixels, on the CPU), Svg (the icon subset:
                           SvgXml reads the tags, SvgStyle the presentation attributes, Svg.cpp draws them)
  layout/                  Flex (lines, grow and shrink within bounds, justification, alignment) and Grid
                           (placement in cells, track sizing): pure functions
  image/                   Bitmap.cpp (decoding PNG and JPEG, stb_image compiled here, within limits; SVG goes to
                           vector/), ImageCache.cpp (the worker, the deliveries, the allowed folders, the bound),
                           Color.cpp (hex colours), ColorSpaces.cpp (the colour-space maths), CreativePixels.hpp (the
                           creative widgets' CPU bitmaps, each a cached slot of at most 256 px a side)
  spatial/                 SpatialIndex (a uniform grid, stored flat, rebuilt only when the items change), Snapping
                           (the nearest edge or centre within a reach)
  detail/                  Utf8.hpp (UTF-8 decoding), Ppm.hpp (PPM screenshots), ShelfPacker.hpp (shelf packing,
                           shared by the glyph atlas and the image pages)
  panel/Panel.cpp          the tree, scrolling, overlays, painting the panel's own shape
  panel/PanelLayout.cpp    measuring, anchors, a layout pass's frames and clips; PanelEdges.hpp the edge arithmetic
  panel/Stack.cpp          rows, columns and grids: measured, laid out by layout/, turned into anchors
  widgets/Label.cpp, TextArea.cpp
                           text (and its icon, runs, links and selection) and a single-line input
  widgets/ImageView.cpp    showing an image, fitted
  widgets/PathView.cpp     showing a vector path, fitted
  canvas/CanvasView.cpp    the canvas: content, culling, children placed, painting (items, levels of detail, the
                           selection, guides); CanvasViewInput.cpp its pointer, wheel and keys;
                           spatial/ (its index and snapping), canvas/Rulers.cpp (steps and ticks),
                           canvas/CanvasViewInternals.hpp (what both sources share)
  graph/                   GraphView.cpp (layout, culling, hit tests, events), GraphModel.cpp (the link rules,
                           cycles, the camera), GraphInput.cpp (pointer, wheel, keys), GraphPaint.cpp (the drawing),
                           GraphShared.hpp (what they share)
  timeline/                TimelineView.cpp (rows, hit tests, snapping's targets, accessibility), TimelineMath.cpp
                           (time mapping, ruler steps, formats, easing), TimelineInput.cpp (pointer, wheel, editing),
                           TimelineKeys.cpp, TimelinePaint.cpp (the drawing), TimelineShared.hpp (what they share)
  editor/TextEditor.cpp    the multi-line editor: editing, selection, scrolling, undo through its model
  editor/TextEditorInput.cpp
                           its keys, typed characters and pointer; TextEditorShared.hpp (the word rules and the
                           modifiers), TextEditorRows.hpp (rows per line, found in O(log n)), TextEditorMinimap.cpp
  editor/TextEditorPaint.cpp
                           its drawing: visible lines in runs of one colour, selection, gutter, caret
  editor/TextEditorView.cpp
                           wrapping and its row index, folds, scrolling, and how edits move folds and decorations
  editor/TextEditorCursors.cpp
                           extra cursors, column selection, Cmd+D, decorations, the view's keys
  editor/TextEditorFind.cpp
                           find and replace line by line, bracket matching
  editor/TextEditorPaintView.cpp
                           what is drawn under and over the text: decorations, matches, brackets, guides, the
                           extra carets, the composition
  editor/DiffView.cpp      a diff's rows, numbers and marks in one or two editors, scrolled together
  editor/TextModel.cpp     positions over the piece tree, UTF-8 checked as it comes in, undo as operations
  widgets/Controls.cpp     Toggle and Slider
  widgets/Calendar.cpp, CivilDate.cpp
                           the month view, and its calendar arithmetic and ISO 8601 dates and times
  widgets/Carousel.cpp, CodeInput.cpp
                           pages turned by arrows, dots, keys and trackpad; a code typed one character per cell
  creative/ColorPicker*.cpp
                           the picker: input, fields and swatches; its drawing (ColorPickerPaint.cpp)
  creative/GradientEditor.cpp
                           the gradient maths and editor; CurvesEditor.cpp the curve maths and editor
  widgets/Split.cpp, Tabs.cpp
                           panes with sashes, and a strip of tabs
  lists/RowWindow.cpp      the rows in view: fixed heights, or measured ones in a Fenwick tree
  lists/ListView.cpp, TreeView.cpp
                           virtualised rows: selection, keys, type-to-find, drops, anchoring; a tree's open
                           nodes and its pinned folders
  lists/TableView.cpp      a table's header, sorting and column widths
CMakeLists.txt             the affrescoui_render target and its sources
```

## Recipes

**Add a widget.**

```cpp
class Badge final : public affresco::Panel {
public:
	void paint(affresco::Painter& painter) const override {
		painter.fillRoundedRect(bounds(), affresco::CornerRadii::all(8.0f), color());
	}
};
window.add<Badge>().setColor(affresco::Color::hex(0x2563eb));
```

Give it its own signals for what it means (`Toggle::onToggled`). When its look changes, call `invalidatePaint()`
(or store the value with `setLook(member, value)`); when its size or arrangement may change too, `invalidate()`
(or `setProperty`). The next frame draws it again, and only where it is.

**Add a property to `Panel`.** A getter and a setter that stores the value with `setProperty` (it changes the layout)
or `setLook` (only the drawing); the layout or `paint` reads it. Document its default. If screen documents should set it, the `format` README says how.

**Test a window without a display.**

```cpp
affresco::detail::ManualClock clock;                                   // src/Headless.hpp
auto window = affresco::detail::createHeadlessWindow(400, 300, clock);
window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {10.0f, 10.0f}});
clock.advance(600.0);                                                   // the tooltip delay has passed
window->render();                                                       // runs what is due, lays out
```

**Run work later.** `window.defer(task)` runs after the current event has been fully delivered (a handler may then
rebuild even its own panel). `window.setTimer(ms, task)` runs it later and returns an id for `cancelTimer`. From
another thread, `application.post(task)`.

**Draw inside the host's OpenGL.** `affresco::Embedding` (`Embedding.hpp`) is a Window with no window or event loop of
its own, in a context the host owns and keeps current: `frame()` draws it over the host's framebuffer, `renderTexture()`
into a texture of the library's; each saves and restores the OpenGL state it changes. Its platform is
`src/platform/EmbeddedPlatform.*` (cursors left to the host, a clipboard of its own, the clock it is given).
