# Other languages

Besides C++, AffrescoUI has a C API and a Python binding built on it.

## The C API

`affresco/affresco.h` (`modules/capi`) is one C11 header for C hosts and for bindings in other languages. A host loads
a screen from a file or a string, reads and changes its state, submits revisions, answers its actions, events and
native calls, and shows it in one of three ways: in its own window, headless (time and input driven by the host,
pictures as RGBA), or embedded in the host's own OpenGL context.

- Targets: `affrescoui::capi` (static, on by default) and, with `AFFRESCOUI_BUILD_CAPI_SHARED`, the shared library
  `libaffresco`, which exports the `afr_` functions only.
- No exception crosses into C: an error is an `afr_status` and a message in `afr_last_error()`. Strings and pixels
  the library returns are freed with `afr_free()`.
- The permission rules still apply: `afr_screen_set_state` is a revision by `application`, a submitted revision is
  reviewed like any other, and a native call is reached only if the host registered it.
- `modules/capi/example` (`affrescoui_embed`) shows a screen inside a program's own OpenGL scene.

Every function, its arguments, limits and errors are in the header; the rules it keeps are in
[`modules/capi/README.md`](https://github.com/RiccardoSottini/AffrescoUI/blob/master/modules/capi/README.md).

## Python

`bindings/python` wraps the C API with ctypes. It is not published; install it from a checkout:

```sh
pip install ./bindings/python            # builds libaffresco with CMake into the package
```

```python
from affrescoui import Screen

with Screen.from_file("app.screen.json", native_actions=["save"]) as screen:  # headless; mode="window" shows it
    @screen.on_action("save")
    def save(sender):
        print("save from", sender)

    screen.state["count"] = 3      # a revision by "application": the rules still decide
    screen.click(40, 20)
    screen.step(16)                # headless time: tasks, timers, layout
    screen.render_png("app.png")
```

A screen has `state`, `submit_revision`, `on_action`, `on_event` and `on_call`, input (`click`, `key`, `type_text`,
`scroll`), pictures (`render_rgba`, `render_png`) and `listen` for peers. Errors raise `AffrescoError`. Details:
[`bindings/python/README.md`](https://github.com/RiccardoSottini/AffrescoUI/blob/master/bindings/python/README.md).

## Without a binding

Programs in any language can also work with AffrescoUI directly:

- **Files**: screens, scenarios and change sets are JSON, checked by `affrescoui check` and `affrescoui test`, with
  [JSON Schemas](../reference/schemas.md) for editors.
- **Live**: a program connects to a running app's socket and sends revisions over the live protocol, as the MCP
  server does. The MCP server ([its tools](../reference/mcp.md)) is a Python program using only the standard library.
- **Channels**: a screen's `channels` talk to programs on local sockets, in both directions.
