# render: the rendering engine

`render` draws a tree of panels in a window and routes input to them. It owns the windowing system (GLFW), the OpenGL
4.1 core context and the renderer, and knows nothing about documents: you build the tree in C++, or `format` builds
it from a screen document. It keeps the ideas of 2017: nested panels, pixel and percentage sizes, anchors to edges,
minimum sizes, hex colours and signal-to-slot events. It adds rows, columns and grids, scrolling, overlays, rounded
shapes with borders and shadows, text with fallback fonts, and a multi-line editor. No public header exposes OpenGL
or GLFW. Target: `affrescoui::render` (uses `core`; vendors glad and stb_truetype from `external/`).

## Public headers (`include/affresco/render/`)

| Header | Offers |
|---|---|
| `Application.hpp` | `Application`: GLFW, every `Window`, the event loop (`run`, `quit`); the `Executor` (`post`) |
| `Window.hpp` | `Window`: the root panel, focus, hover, mouse capture, clipboard, tooltips; `defer`, `setTimer`, `cancelTimer`; `render`, `saveScreenshot` |
| `Panel.hpp` | `Panel`: the owned tree, `Length` positions and sizes, anchors (also to another panel's edge), minimum sizes, `grow`, scrolling, overlays, shapes, cursors, tooltips, event signals and hooks |
| `Stack.hpp` | `Stack` (a row or a column, with a gap and growing children) and `Grid` (equal columns) |
| `Label.hpp` | `Label`: text with padding, alignment, wrap, and fit to text |
| `TextArea.hpp` | `TextArea`: a single-line input |
| `TextEditor.hpp` | `TextEditor`: multi-line editing, undo, clipboard, a gutter; a colouring hook for subclasses |
| `Controls.hpp` | `Toggle` (a switch or a checkbox) and `Slider` |
| `Font.hpp` | `Font`: a TrueType font, glyphs baked on demand into atlas pages, fallback fonts, measuring and wrapping |
| `Painter.hpp` | `Painter`: what `paint` draws with (rectangles, rounded rectangles, borders, shadows, text); `CornerRadii`, `Shadow` |
| `Color.hpp`, `Geometry.hpp`, `Length.hpp` | `Color` (hex in and out), `Vec2`, `Rect`, `Length` with the `_px` and `_pct` literals |
| `Events.hpp`, `Input.hpp` | The typed events (`KeyEvent`, `MouseButtonEvent`, ...) and the key and button names (GLFW's values) |

## Invariants

- **A parent owns its children** (`std::unique_ptr`); `parent()` and `window()` never own. Destroying a panel
  clears the window's focus, hover and capture if they point at it.
- **Setters only mark the window dirty.** Layout runs once, before the next frame, then the tree is drawn. Changes
  made during the layout pass itself (`prepareLayout`, `arrange`) don't schedule another one.
- **A window draws only when something changed,** and the event loop sleeps while nothing is due: idle costs no CPU.
- **Layout per axis:** anchored edges first, then the position, then `max(size, minimum)`; percentages refer to the
  parent; a panel is clipped to its parent, and a panel clipped to nothing is not shown. Overlays are laid out, drawn
  and hit-tested after the whole tree, unclipped.
- **Every OpenGL call is in `src/gl/`.** `gl::QuadRenderer` draws the tree as batched quads: one draw call per
  texture switch, clipping on the CPU.
- **One `Application` at a time.** A second one throws.
- **Limits:** a glyph atlas page is at most 4,096 × 4,096; a font's pages are bounded too, so text naming many
  characters can't exhaust memory.

## Threads

Everything here runs on the main thread, inside `Application::run`. The one exception is `Application::post`, which
any thread may call: the task runs on the main thread, and the event loop wakes for it. This is how channels deliver
messages. Timers (`Window::setTimer`) and deferred tasks (`Window::defer`) run on the main thread too; an owner that
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
- **Coming in wave 2:** a platform layer behind an internal interface, a headless window and one clock (from 7.3);
  text shaping with HarfBuzz and FreeType (from 2.3b). They aren't there yet: `Window.cpp` still calls GLFW and
  `Font` uses stb_truetype.

## Files

```
include/affresco/render/   the public headers above
src/
  Application.cpp          GLFW start-up, the event loop, posted tasks
  Window.cpp               input routing, focus, hover, capture, tooltips, timers, deferred tasks, frames
  Panel.cpp                the tree, layout, scrolling, overlays, painting the panel's own shape
  Stack.cpp                rows, columns and grids
  Label.cpp, TextArea.cpp  text and a single-line input
  TextEditor.cpp           the multi-line editor
  Controls.cpp             Toggle and Slider
  Font.cpp                 the font, its atlas pages, fallbacks, measuring and wrapping
  Color.cpp                hex colours
  detail/Utf8.hpp          UTF-8 decoding, shared by the text code
  detail/Ppm.hpp           PPM screenshots
  gl/                      everything OpenGL: Context (state, read-back), GlObjects (RAII names), Shader,
                           QuadRenderer (the batched renderer and its Painter)
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

Give it its own signals for what it means (`Toggle::onToggled`), and call `invalidate()` when its look changes.

**Add a property to `Panel`.** A getter and a setter that stores the value and calls `invalidate()`; the layout or
`paint` reads it. Document its default. If screen documents should set it, the `format` README says how.

**Run work later.** `window.defer(task)` runs after the current event has been fully delivered (a handler may then
rebuild even its own panel). `window.setTimer(ms, task)` runs it later and returns an id for `cancelTimer`. From
another thread, `application.post(task)`.
