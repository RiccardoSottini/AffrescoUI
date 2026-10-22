# render: the rendering engine

`render` draws a tree of panels in a window and routes input to them. It owns the windowing system (GLFW, behind an
internal platform layer), the OpenGL 4.1 core context and the renderer (behind an internal interface), and knows
nothing about documents: you build the tree in C++, or `format` builds
it from a screen document. It keeps the ideas of 2017: nested panels, pixel and percentage sizes, anchors to edges,
minimum sizes, hex colours and signal-to-slot events. It adds rows, columns and grids, scrolling, overlays, rounded
shapes with borders and shadows, shaped text with fallback fonts and colour emoji, images and icons, and a multi-line editor. No public
header exposes OpenGL, GLFW, FreeType, HarfBuzz or stb_image. Target: `affrescoui::render` (uses `core`; vendors glad from
`external/`; links FreeType and HarfBuzz, pinned in `cmake/AffrescoUIText.cmake`).

## Public headers (`include/affresco/render/`)

| Header | Offers |
|---|---|
| `Application.hpp` | `Application`: the windowing system, every `Window`, the event loop (`run`, `quit`), their clock (a `SteadyClock`); the `Executor` (`post`); `ApplicationOptions` (another clock, hidden windows: for automation) |
| `Window.hpp` | `Window`: the root panel, focus, hover, mouse capture, clipboard, tooltips; the desktop: files dropped from the OS (`onDrop`), the display's scale, the system's colour scheme and its reduced-motion setting (`contentScale`, `colorScheme`, `reducedMotion`, `onDisplay`), a native menu bar (`setMenuBar`, `MenuItem`); a focus ring drawn around the focused panel only after the keyboard moved the focus (`setFocusRing`, `FocusRing`, `focusVisible`; on by default in screens); the OS's file dialogs (`chooseFiles`, `FileDialog`, `FileFilter`; `setFileDialogHandler` answers them instead, for tests and automation); the `route...` input methods; `clock`, `defer`, `runDeferredTasks`, `setTimer`, `cancelTimer`; `render`, `saveScreenshot`; for the inspector, `frameStatistics` (`FrameStatistics.hpp`: frames drawn, the last one's time and draw calls) and `setInspection`, a layer drawn above everything and never hit |
| `Accessibility.hpp` | What a panel is to a person or a screen reader: `accessibleRole` (the panel's own, or one from its kind: a button, a checkbox, a textbox, text), `accessibleName` (its label, else its text or the text inside it, else its tooltip), `accessibilityTree` (`AccessibleNode`: shown panels with a role, in tree order), `findByRole`. The roles and names are set on a `Panel` (`setAccessibleRole`, `setAccessibleLabel`, `setAccessibleDescription`); the OS backend comes later |
| `Clock.hpp` | `Clock`, what a window's timers, tooltip delay and double clicks read, and the animations of what is built into it; `SteadyClock` |
| `Panel.hpp` | `Panel`: the owned tree, `Length` positions and sizes, anchors (also to another panel's edge), minimum sizes, `grow`, scrolling, overlays, shapes, cursors, tooltips, event signals and hooks; `setCached` (the subtree drawn from a texture) |
| `Stack.hpp` | `Stack` (a row or a column, like a CSS flex container: justify, alignItems, gaps, grow, shrink, basis, wrap) and `Grid` (column and row tracks, cells and spans) |
| `Layout.hpp` | The values of flexible and grid layout: `Justify`, `CrossAlign`, `Track` (pixels, percent, fractions, content, minmax), `GridCell` |
| `Label.hpp` | `Label`: text with padding, alignment, wrap, and fit to text; an icon before it (`setIcon`, a glyph named in its font: `Font::codepointNamed`); rich text (`setRuns`), links (`onLink`) and selection with copy (`setSelectable`) |
| `RichText.hpp` | `TextRun` (a part of a text with its own font, colour, link or inline icon) and `TextLayout` (runs broken into lines across styles, hit testing, carets) |
| `TextArea.hpp` | `TextArea`: a single-line input |
| `TextEditor.hpp` | `TextEditor`: multi-line editing, undo, clipboard, a gutter; a colouring hook for subclasses; its text is a `TextModel` (`model()`) |
| `TextModel.hpp` | `TextModel`: UTF-8 text of any size as a piece tree, with a line index (`line`, `offsetOf`, `positionOf`), O(log n) edits, undo as operations (`beginStep`, `endStep`, `undo`) and `onChanged` (`TextChange`); `TextLines`, its lines decoded and kept for reading again |
| `Controls.hpp` | `Toggle` (a switch or a checkbox) and `Slider` |
| `Split.hpp` | `Split`: panes along a row or a column with sashes between them; sizes within minimums, the last pane taking the rest; dragged, reset by a double click or Home, moved by the arrow keys (PageUp and PageDown pick a sash); `onSizesChanged` |
| `Tabs.hpp` | `Tabs`: a strip of `Tab`s, one active; it scrolls when they overflow, closes (a button or Delete), reorders by dragging (`onMoved`), shows where a tab dragged from another strip would go (`insertionAt`, `showDropIndicator`), and moves with the arrow keys, Home and End |
| `RowWindow.hpp` | `RowWindow`: which rows of a long list are in view and where each starts, for one shared virtualisation; fixed heights in O(1), measured ones in O(log n) |
| `ListView.hpp` | `ListView`: rows of a `ItemModel` (`ItemList`, or an application's own model), drawing only those in view, each with an optional icon (`ListItem::icon`, `iconColor`); single or multiple selection kept by id across new items, arrow keys, pages, type-to-find, `onOpened`, a drop line or outline (`dropTargetAt`); once scrolled, the top row stays in place when items are added above it |
| `TreeView.hpp` | `TreeView`: a `ListView` over a pre-order tree, showing only the open nodes' rows, with indent guides and chevrons; Right and Left open, close and walk; `onExpandedChanged`; the open folders of the top row pinned as it scrolls (`setStickyRows`) |
| `TableView.hpp` | `TableView`: a `ListView` of rows of cells under a header; a header click sorts (numbers before text, stable, again to reverse: `onSortChanged`), a header edge drag resizes a column (`onColumnResized`); the order and widths given replace the user's only when they change |
| `GraphView.hpp` | `GraphView`: a node editor over a `GraphModel` (nodes with typed ports, links); the link rules (`checkLink`: kinds, `links` per port, duplicates, cycles), a small camera (`GraphCamera`, zoom and pan, to move onto Camera2D), Bézier links coloured by kind, a minimap, off-screen nodes culled; drags, a marquee and the keyboard emit `GraphEvent`s (`onEdit`) and change nothing themselves |
| `TimelineView.hpp` | `TimelineView`: tracks of keyframes and clips under a time ruler over a `TimelineModel` (nested groups, mute and lock, markers, a play range); ticks that follow the zoom in frames, seconds or timecode (`rulerTicks`, `formatTime`), snapping (`snapTime`), step, linear and Bézier easing (`keyframeValueAt`); only the rows in view laid out and drawn; scrubbing, keyframe drags, clip moves and trims, a marquee and the keyboard emit `TimelineEvent`s (`onEdit`) |
| `Font.hpp` | `Font`: a font and its fallbacks at one size, in a style (`FontStyle`: bold, italic, synthesised from the outlines); `shape` (glyphs, advances, carets), `measure`, `wrap`; glyphs rasterised on demand into coverage and colour atlas pages |
| `FontCache.hpp` | `FontCache`: fonts by file, size and fallbacks, within a bound (32 fonts, 128 MiB), the least recently used that nothing holds dropped first; the system's fallback fonts when the host asks |
| `Painter.hpp` | `Painter`: what `paint` draws with (rectangles, rounded rectangles, borders, shadows, text, images, meshes of anti-aliased quads with `fillMesh`); `CornerRadii`, `Shadow`, `MeshVertex` |
| `Bitmap.hpp` | `Bitmap`: an image's premultiplied RGBA pixels, immutable and shared; `load` and `decode` (PNG and JPEG with stb_image, SVG icons rasterised at twice their size) and `fromPixels`, within `Bitmap::Limits` |
| `ImageCache.hpp` | `ImageCache`: files decoded on a worker thread and delivered on the main one (through an `Executor`), only from allowed folders, kept within a bound; images the host names; `Request`s that cancel when destroyed |
| `ImageView.hpp` | `ImageView`: a panel showing a bitmap, a file or a named image, with `ImageFit` (`contain`, `cover`, `fill`) |
| `VectorPath.hpp` | `VectorPath`: SVG path data parsed within `VectorPath::Limits` (or built with `moveTo`, `lineTo`, `cubicTo`, `close`) and flattened; `appendFill` (`FillRule`) and `appendStroke` (`StrokeStyle`: width, `LineJoin`, `LineCap`, dashes), the meshes that draw it, within `meshQuadLimit` and `meshWorkLimit` |
| `PathView.hpp` | `PathView`: a panel drawing a path, filled and stroked, fitted to it through a view box with `ImageFit`; its meshes are built again only when the path, the style or the size changes |
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
- **A frame draws only what changed.** Each panel keeps what it drew (`src/RetainedGeometry.hpp`: its clipped quads
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
- **Layout per axis:** anchored edges first, then the position, then the size within its minimum and maximum (the
  minimum wins), then the aspect ratio; percentages refer to the
  parent; a panel is clipped to its parent, and a panel clipped to nothing is not shown. Overlays are laid out, drawn
  and hit-tested after the whole tree, unclipped.
- **Every OpenGL call is in `src/gl/`, and every GLFW call in `src/platform/GlfwPlatform.cpp`** (and what GLFW
  doesn't do, the colour scheme, reduced motion and native menu bars, in `src/platform/Desktop*`: Cocoa on macOS,
  Win32 on Windows; elsewhere the scheme is light, motion is full and there is no native bar, so `setMenuBar` returns
  false). `Window.cpp` and
  `Application.cpp` call neither: the OS window is a `platform::NativeWindow` (`src/platform/Platform.hpp`), and
  drawing a `detail::Renderer` (`src/Renderer.hpp`), whose OpenGL implementation (`gl/OpenGlRenderer.cpp`) uses
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
  runs left to right (no bidirectional reordering yet). Shaped text is cached per font by line and by word, so a
  frame that draws what the last one drew shapes nothing, and measuring a new line of known words allocates nothing.
- **Plain words skip shaping, with HarfBuzz's results.** A word of the first face's plain Latin-1 characters
  (`text/PlainText`) is laid out from a table of advances with kerning, one read per character, without HarfBuzz or
  the caches. Which characters are plain comes from the face's GSUB and GPOS lookups (`text/LookupSpans` reads how
  many neighbours each one reads); what the lookups that read at most two do is taken from HarfBuzz itself, shaping
  each character and each pair alone the first time it is met. Everything else (ligatures such as Roboto's `fi`,
  contextual forms, marks, other scripts, fallback faces) is shaped as before. `tests/unit/plain_text.cpp` checks
  that both give the same glyphs, advances, carets and widths, to the last bit.
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
- **Limits:** a glyph atlas page is at most 4,096 × 4,096, and a font's pages hold at most 64 MiB in all (colour pages
  count four bytes a texel), so text naming many characters can't exhaust memory; past that, characters draw as `?`.
  A font file is a regular file of at most 256 MiB, and a font's size at most 4,096 pixels. `FontCache` keeps at most
  32 fonts and 128 MiB that nothing uses. A `TextModel` (a `TextEditor`'s text) keeps 500 undo steps, and at most
  256 MiB of removed text in them; invalid UTF-8 becomes U+FFFD as it comes in.
- **Large texts:** a `TextModel` copies a file once into buffers that are only appended to, and keeps pieces of them
  in a treap with the bytes and line feeds under each node, so an edit or finding a line costs O(log n). A 50 MB
  text opens in under a second, an edit takes under 50 µs, and memory stays under 1.5 times the text
  (`tests/unit/text_model.cpp`). The editor decodes and colours only the visible lines (the colouring state of each
  line above them is kept), and past 4,096 lines estimates its width from the longest line.

## Threads

Everything here runs on the main thread, inside `Application::run`. There are two exceptions. `Application::post` may
be called from any thread: the task runs on the main thread, and the event loop wakes for it. This is how channels
deliver messages. Each `ImageCache` has a worker thread that only decodes files (`Bitmap::load`) and posts the
results that way; it is stopped and joined when its cache is destroyed, and what it posted then does nothing. Timers (`Window::setTimer`) and deferred tasks (`Window::defer`) run on the main thread too; an owner that
captures `this` in one must cancel its timers in its destructor.

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
  without changing the library (`examples/CodeEditor.*` adds syntax colours that way).
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
  Application.cpp          the platform, the windows, the event loop, posted tasks
  Window.cpp               input routing, focus, the focus ring, hover, capture, tooltips, timers, deferred tasks,
                           frames
  Accessibility.cpp        roles, accessible names, the tree, finding by role
  Headless.hpp / .cpp      a headless window and a manual clock, for tests (internal)
  Renderer.hpp             detail::Renderer, what a window draws with
  platform/                Platform.hpp (the internal interface), GlfwPlatform.cpp (every GLFW call), Desktop.hpp
                           and DesktopMac.mm, DesktopWin32.cpp, DesktopOther.cpp (colour scheme, menu bar per OS),
                           FileDialogs.cpp (the OS's file dialogs, through nativefiledialog-extended)
  Panel.cpp                the tree, layout, scrolling, overlays, painting the panel's own shape
  Stack.cpp                rows, columns and grids: measured, laid out by layout/, turned into anchors
  layout/                  Flex (lines, grow and shrink within bounds, justification, alignment) and Grid
                           (placement in cells, track sizing): pure functions
  Label.cpp, TextArea.cpp  text (and its icon, runs, links and selection) and a single-line input
  RichText.cpp             TextLayout: runs into lines, hit testing, carets
  Bitmap.cpp               decoding PNG and JPEG (stb_image compiled here), within limits; SVG goes to vector/
  ImageCache.cpp           the worker, the deliveries, the allowed folders, the bound
  ImageView.cpp            showing an image, fitted
  PathView.cpp             showing a vector path, fitted
  vector/                  VectorPath.cpp (parsing, flattening), PathFill.cpp (scanline trapezoids by the fill
                           rule, and the fringe), PathStroke.cpp (segments, joins, caps, dashes), VectorMath.hpp,
                           MeshRasterizer (meshes into premultiplied pixels, on the CPU), Svg (the icon subset:
                           SvgXml reads the tags, SvgStyle the presentation attributes, Svg.cpp draws them)
  TextEditor.cpp           the multi-line editor: input, selection, scrolling, undo through its model
  TextEditorPaint.cpp      its drawing: visible lines in runs of one colour, selection, gutter, caret
  TextModel.cpp            positions over the piece tree, UTF-8 checked as it comes in, undo as operations
  Controls.cpp             Toggle and Slider
  Split.cpp, Tabs.cpp      panes with sashes, and a strip of tabs
  RowWindow.cpp            the rows in view: fixed heights, or measured ones in a Fenwick tree
  ListView.cpp, TreeView.cpp  virtualised rows: selection, keys, type-to-find, drops, anchoring; a tree's open
                           nodes and its pinned folders
  TableView.cpp            a table's header, sorting and column widths
  Font.cpp                 the font: loading, Latin-1 at load, glyphs, measuring and wrapping, the atlas
  FontCache.cpp            the bounded font cache, and the system's fallback fonts
  text/                    FontFace (one file at one size, in FreeType and HarfBuzz), GlyphAtlas (coverage and
                           colour pages), ShapedTextCache (LRU by text), FontData.hpp (what a Font holds),
                           Shaping.cpp (runs, HarfBuzz, clusters into advances and carets), PlainText (words laid
                           out without shaping), LookupSpans (how far GSUB and GPOS lookups reach, read with every
                           offset checked), FontInspector.hpp (what tests see of a font), PieceTree (the text model's
                           treap of pieces over append-only buffers, with line feeds counted)
  Color.cpp                hex colours
  detail/Utf8.hpp          UTF-8 decoding, shared by the text code
  detail/Ppm.hpp           PPM screenshots
  RetainedGeometry.hpp     what a panel drew: quads, batch steps, what the drawing depended on (no OpenGL)
  PaintRecorder.*          the Painter panels draw with: clipped quads and batch steps, recorded
  GeometryCache.*          before a frame: the panels that changed or moved recorded again, the damage, the shaders
  detail/ShelfPacker.hpp   shelf packing, shared by the glyph atlas and the image pages
  gl/                      everything OpenGL: Context (state, read-back, canvases, scissored regions), GlObjects
                           (RAII names), Shader, QuadRenderer (replays records into batches and draws them),
                           QuadRendererCache (cached subtrees), SubtreeTextures (their textures and
                           framebuffers), ImageTextures (image pages and textures), OpenGlRenderer (the Renderer: canvases with
                           their damage, a full frame as the fallback)
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
