# capi: the C API

`capi` is the library behind one C11 header, `affresco/affresco.h`, for C hosts and for bindings in other languages
(`bindings/python` loads it with ctypes). A host loads a screen from a file or a string, reads and changes its state,
submits revisions, answers its actions, events and native calls, and shows it in one of three ways: in its own window,
headless (time and input driven by the host, pictures as RGBA), or embedded in the host's own OpenGL context. Targets:
`affrescoui::capi` (static, `AFFRESCOUI_BUILD_CAPI`, on by default at the top level) and, with
`AFFRESCOUI_BUILD_CAPI_SHARED`, the shared library `libaffresco` that exports the `afr_` functions only.

## Public header (`include/affresco/`)

| Header | Offers |
|---|---|
| `affresco.h` | `afr_version`, `afr_last_error`, `afr_free`, `afr_options_init`; `afr_screen_create_from_file`, `_from_string`, `_destroy`; `_replace_document`, `_get_state`, `_set_state`, `_submit_revision`; `_on_action`, `_on_event`, `_on_call`, `afr_call_return`, `afr_call_fail`, `_post`; `_run`, `_quit` (window); `_step` (headless, embedded); `_mouse`, `_key`, `_text`, `_scroll`; `_frame`, `_render_texture` (embedded); `_render_rgba`; `_listen` |

## Invariants

- **No exception crosses into C.** Every function runs its body in `capi::guarded`: a C++ exception becomes an
  `afr_status` and a message in `afr_last_error()` (per thread, like errno, cleared by the next call).
- **Every argument checked.** Pointers are checked for NULL, strings for length (NUL-terminated ones are scanned up to a
  limit, never past it), numbers for range and finiteness, text for valid UTF-8, enums for their values. A string the
  host gives with its length is never read past it.
- **Memory is the caller's or `afr_free`'s.** Strings and pixels the library returns are `malloc`ed, freed with
  `afr_free()`; nothing the library keeps is ever handed out, except the embedded texture's GL id, which stays the
  library's.
- **Versioned ABI.** `afr_version()` is `AFR_VERSION` (major, minor, patch in bytes); `afr_options` carries its size,
  so a host built against an older, smaller struct still works and a newer, larger one is refused.
- **One thread per screen.** A screen is used from the thread that created it; `afr_screen_post()` is the one call safe
  from any thread. Window and headless screens use GLFW, so the main thread and one at a time; embedded screens have no
  such limit (the host's context must be current).
- **No way around permissions.** `afr_screen_set_state` and `_replace_document` are revisions by "application", so the
  document's deny rules still apply; `afr_screen_submit_revision` is reviewed like any other; a native call is only
  reached if the host granted it with `afr_screen_on_call`; peers on `afr_screen_listen` are authored as the name the
  host gives, which is required.
- **Native actions are named first.** A screen is checked and built when it is created, before the host can register
  its actions, so `afr_options.native_actions` lists the names it will answer (otherwise the check reports them as
  unknown); `afr_screen_on_action()` then gives each its callback.
- **Embedding saves the host's GL state.** `afr_screen_frame` and `_render_texture` save and restore the state they
  touch (the list is in `affresco.h`) and set their own defaults while drawing.

## Threads and ownership

- A screen belongs to the thread that created it; every call but `afr_screen_post()` is made on that thread. Callbacks
  (`_on_action`, `_on_event`, `_on_call`, a posted task) run on it too, inside `_run`, `_step` or a frame.
- `afr_screen_create_*` gives the host a screen it owns until `afr_screen_destroy()` (embedded: with its context
  current).
- Strings the host passes are read during the call and copied if kept; `user_data` pointers are kept and handed back
  as given, never read or freed.
- What the library returns (`_get_state`, `_render_rgba`) is the host's, freed with `afr_free()`. A picture is at
  most 16384 pixels a side, in every mode.

## Extension points

- **Native actions and calls:** `afr_screen_on_action()` and `afr_screen_on_call()`; a call answers later with
  `afr_call_return()` or `afr_call_fail()`.
- **Another language:** a binding loads the shared library and declares the `afr_` functions, as `bindings/python`
  does with ctypes; `afr_options` carries its size, so a binding built against an older header keeps working.

## Files

```
include/affresco/affresco.h   the C API: every function, its arguments, limits and errors
src/
  Handle.hpp     what an afr_screen holds, the last error, the argument checks, the guard for exceptions
  Common.cpp     errors, checks, memory, the headless task queue, and the functions that take no screen
  Screen.cpp     creating and showing screens, their document and state, the host's callbacks, peers
  Input.cpp      the window's loop, the headless step, the host's input, frames, textures and RGBA pixels
```

## Tests

`tests/capi` calls every function from C11 with strict warnings: headless, in a hidden window, embedded in a context of
its own, and the argument checks. `ctest -R capi` runs it. `examples/embed` is a host using it in its own OpenGL.
