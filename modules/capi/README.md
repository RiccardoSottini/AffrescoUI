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
- **Embedding saves the host's GL state.** `afr_screen_frame` and `_render_texture` save and restore the state they
  touch (the list is in `affresco.h`) and set their own defaults while drawing.

## Tests

`tests/capi` calls every function from C11 with strict warnings: headless, in a hidden window, embedded in a context of
its own, and the argument checks. `ctest -R capi` runs it.
