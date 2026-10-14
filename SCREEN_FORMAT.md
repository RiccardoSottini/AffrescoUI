# sGLGUI Screen Format, version 1

A JSON format for describing a user interface that sGLGUI builds and shows at runtime. The file describes **what is
on screen**; the application's code adds **behaviour** (it handles named actions, finds elements by `id`, and builds
templates on demand).

- File extension: `.screen.json`
- JSON Schema (editor autocompletion and validation): [`schema/sglgui-screen.schema.json`](schema/sglgui-screen.schema.json).
  Point a file at it with `"$schema"`.
- Try any file without writing code: `./build/screen_viewer path/to/file.screen.json`

```json
{
	"$schema": "../schema/sglgui-screen.schema.json",
	"format": "sglgui-screen",
	"version": 1,
	"window": { "title": "Hello", "width": 600, "height": 400 },
	"fonts": { "default": { "file": "Roboto-Regular.ttf", "size": 16 } },
	"colors": { "accent": "#6366f1" },
	"root": {
		"color": "#f8fafc",
		"children": [
			{ "type": "label", "text": "Click me", "anchors": { "left": 24, "top": 24 }, "width": 160, "height": 44,
			  "align": "center", "color": "@accent", "textColor": "#ffffff", "onClick": "hello" }
		]
	}
}
```

## Top level

| Key | Required | Meaning |
|---|---|---|
| `version` | yes | Must be `1`. |
| `root` | yes | Node for the window's root panel. It always fills the window, so layout properties aren't allowed on it. |
| `format` | | `"sglgui-screen"`, for tools. |
| `window` | | `title`, `width`, `height`, `minWidth`, `minHeight` (integers). |
| `fonts` | | `name → { "file", "size" }`. Paths are relative to the screen file. `default` is used by text without a `font`. |
| `colors` | | `name → "#rrggbb"`. Referenced anywhere a color is expected as `"@name"`. |
| `styles` | | `name → properties`, applied to a node with `"style": "name"` or `["a", "b"]`. Later styles win, and the node's own properties win over all. |
| `templates` | | `name → node`. Built by code on demand, any number of times (`Screen::instantiate`), e.g. one per chat message. |

## Nodes

A node is a panel: a rectangle positioned relative to its parent and clipped to it. Children are drawn on top of their
parent, and later siblings on top of earlier ones.

### Values
- **Length:** a number of pixels (`120`, `"120px"`) or a percentage of the parent's matching dimension (`"50%"`).
- **Color:** `"#rgb"`, `"#rrggbb"`, `"#rrggbbaa"`, `"@name"` (from `colors`) or `"transparent"`.
- **Align:** `"start"`, `"center"` or `"end"`.

### Every type
| Key | Meaning |
|---|---|
| `type` | `panel` (default), `label`, `textarea`, `stack`, or a type the application registers. |
| `id` | Name for code to find the node (`screen.get<sgl::Label>("title")`). |
| `style` | Style name or list of names. |
| `children` | Array of nodes. |
| `x`, `y` | Position of the top-left corner inside the parent (used when the matching edge isn't anchored). |
| `width`, `height`, `minWidth`, `minHeight` | Size. The minimum wins over a smaller size. |
| `anchors` | `{ "top", "bottom", "left", "right" }`: keep that edge this far from the parent's same edge. Anchoring two opposite edges stretches the node. |
| `fill` | Shorthand: anchor all four edges this far in (`0` or `true` to cover the parent). |
| `color` | Background. Default: light grey for panels, transparent for labels and stacks. |
| `visible` | `false` hides the node and its children. |
| `cursor` | `arrow`, `ibeam`, `hand`, `resize-horizontal`, `resize-vertical`. |
| `hover`, `pressed`, `focused` | `{ "color" }` shown in that state (pressed wins over focused, focused over hover). |
| `onClick` | Action name, fired when the left button is released over the node. |

### `label`
| Key | Meaning |
|---|---|
| `text` | The text. |
| `textColor` | Default black. |
| `font` | Font name. Default: `default`. |
| `padding` | Pixels between the edges and the text. |
| `align` | One value for both axes, or `{ "horizontal", "vertical" }`. |
| `wrap` | Break lines that don't fit the width. |
| `fitText` | `true` or `{ "maxWidth" }`: the label sizes itself to its text (plus padding), wrapping at `maxWidth`. |

### `textarea`
Everything `label` has, plus `placeholder` and `placeholderColor`. It's a single-line input: click it to type.

### `stack`
Places its children one below the other, in order. Children keep their horizontal anchors and width; the stack manages
their vertical position.

| Key | Meaning |
|---|---|
| `gap` | Pixels between children. |
| `align` | `start` (from the top) or `end` (the block sits at the bottom, like a chat; overflow is clipped at the top). |

## Errors

Loading stops at the first problem and names its place in the file:

```
sGLGUI screen: root.children[0].anchors: unknown edge 'lefft' (expected top, bottom, left or right)
sGLGUI screen: root.children[0].width: expected a length: a number of pixels, "12px" or "50%"
sGLGUI screen: root.children[0].text: unknown property 'text' for this type
```

## From code

```cpp
sgl::Application app;
sgl::Screen screen = sgl::Screen::fromFile("chat.screen.json");
screen.registerType("resizeHandle", [] { return std::make_unique<ResizeHandle>(); }); // before createWindow
sgl::Window& window = screen.createWindow(app);

screen.onAction("send", [&](sgl::Panel&) { /* ... */ });
auto& input = screen.get<sgl::TextArea>("input");
auto& bubble = screen.instantiate("message", screen.get("messages"));
app.run();
```

See `examples/chatbot_demo.cpp` with `examples/assets/screens/chat.screen.json`, and
`examples/assets/screens/sign-in.screen.json` (no code at all: open it with `screen_viewer`).
