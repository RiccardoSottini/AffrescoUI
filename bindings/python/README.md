# affrescoui: the Python binding

A ctypes wrapper over the C API (`modules/capi`, `affresco/affresco.h`): load a screen document, read and change its
state, answer its actions and native calls, and drive it headless (scripted input, pictures as RGBA or PNG) or show it
in a window. Not published; build it from this checkout.

```sh
pip install ./bindings/python            # builds libaffresco with CMake (scikit-build-core) into the package
# or, with a library already built (-DAFFRESCOUI_BUILD_CAPI_SHARED=ON):
AFFRESCOUI_LIBRARY=build/modules/capi/libaffresco.dylib PYTHONPATH=bindings/python python3 ...
```

```python
from affrescoui import Screen

with Screen.from_file("app.screen.json") as screen:      # headless by default; mode="window" to show it
    @screen.on_action("save")
    def save(sender):                                       # "onClick": "save" with no document action of that name
        print("save from", sender)

    @screen.on_call("lookup")                               # grants {"call": "lookup"}; an exception is its error
    def lookup(arguments):
        return {"found": arguments["key"]}

    screen.state["count"] = 3                               # a revision by "application": the rules still decide
    screen.click(40, 20)
    screen.step(16)                                         # headless time: tasks, timers, layout
    print(screen.state["items.0.title"])
    screen.render_png("app.png")
```

| Piece | What |
|---|---|
| `Screen.from_file`, `from_string` | `mode` ("headless", "window", "embedded") and the options as keywords: `socket_directory`, `data_directory`, `history_in_memory`, `width`, `height`, `scale`, `hidden` |
| `screen.state[path]` | Read and set by dotted path or JSON Pointer; `get`, `in`, `all()` |
| `replace_document`, `submit_revision` | The whole document; a revision, whose result (`applied`, `proposed`, `rejected`) is returned |
| `on_action`, `on_event`, `on_call` | Decorators; exceptions in them are printed, never passed into the library |
| `post`, `run`, `quit`, `step` | Tasks from any thread; the window's loop; headless time |
| `mouse`, `click`, `key`, `type_text`, `scroll` | Input, in window units |
| `render_rgba`, `render_png` | Pictures, from scratch |
| `listen` | A channel for peers (the MCP server, the Studio), authored as the name given |

Errors raise `AffrescoError` with the C API's `status` and `message`. A screen is used from the thread that created it;
window and headless screens from the main thread, one at a time.

Tests (headless): `AFFRESCOUI_LIBRARY=<the library> python3 -m pytest bindings/python/tests`. Examples:
`examples/render_screen.py` (a document to a PNG) and `examples/counter.py` (a document, an action and a call, in code).
