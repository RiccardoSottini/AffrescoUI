"""The C API (modules/capi/include/affresco/affresco.h) as ctypes declarations, and the library that provides it.

The library is looked for, in order: the path in the environment variable AFFRESCOUI_LIBRARY; the package's own folder,
where a wheel puts it; the system's library path (ctypes.util.find_library).
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import sys
from pathlib import Path

#: The C API's version these declarations follow: AFR_VERSION_MAJOR and AFR_VERSION_MINOR.
VERSION_MAJOR = 0
VERSION_MINOR = 1

# afr_status
OK = 0
ERROR_ARGUMENT = 1
ERROR_DOCUMENT = 2
ERROR_STATE = 3
ERROR_NOT_FOUND = 4
ERROR_REFUSED = 5
ERROR_INTERNAL = 6

#: afr_mode, by name.
MODES = {"window": 0, "headless": 1, "embedded": 2}

# afr_mouse_action
MOUSE_MOVE, MOUSE_PRESS, MOUSE_RELEASE, MOUSE_ENTER, MOUSE_LEAVE = range(5)

# afr_key_action
KEY_RELEASE, KEY_PRESS, KEY_REPEAT = range(3)

GL_LOADER = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_char_p)
ACTION_CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p)
EVENT_CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_char_p)
CALL_CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p)
TASK = ctypes.CFUNCTYPE(None, ctypes.c_void_p)


class Options(ctypes.Structure):
    """afr_options: how a screen is created."""

    _fields_ = [
        ("size", ctypes.c_size_t),
        ("mode", ctypes.c_int),
        ("socket_directory", ctypes.c_char_p),
        ("data_directory", ctypes.c_char_p),
        ("history_in_memory", ctypes.c_int),
        ("width", ctypes.c_int),
        ("height", ctypes.c_int),
        ("scale", ctypes.c_float),
        ("hidden", ctypes.c_int),
        ("gl_loader", GL_LOADER),
        ("native_actions", ctypes.POINTER(ctypes.c_char_p)),
    ]


def _library_names() -> list[str]:
    """The shared library's file names on this system, the versioned ones last.

    @return The names.
    """
    if sys.platform == "win32":
        return ["affresco.dll"]
    if sys.platform == "darwin":
        return ["libaffresco.dylib"]
    return ["libaffresco.so"]


def _find_library() -> str:
    """Finds the shared library.

    @return Its path.
    @throws OSError If it isn't anywhere it is looked for.
    """
    explicit = os.environ.get("AFFRESCOUI_LIBRARY")
    if explicit:
        return explicit
    here = Path(__file__).resolve().parent
    for name in _library_names():
        candidate = here / name
        if candidate.exists():
            return str(candidate)
    found = ctypes.util.find_library("affresco")
    if found:
        return found
    raise OSError("libaffresco not found: build it with -DAFFRESCOUI_BUILD_CAPI_SHARED=ON and set AFFRESCOUI_LIBRARY "
                  "to its path, or install the wheel")


def _declare(library: ctypes.CDLL) -> None:
    """Declares the argument and result types of every function, so ctypes converts and checks them.

    @param library The library.
    """
    screen, call = ctypes.c_void_p, ctypes.c_void_p
    text, size, status = ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int
    out_text = ctypes.POINTER(ctypes.c_void_p)
    signatures = {
        "afr_version": (ctypes.c_uint32, []),
        "afr_last_error": (ctypes.c_char_p, []),
        "afr_free": (None, [ctypes.c_void_p]),
        "afr_options_init": (status, [ctypes.POINTER(Options)]),
        "afr_screen_create_from_file": (status, [text, ctypes.POINTER(Options), ctypes.POINTER(screen)]),
        "afr_screen_create_from_string": (status, [text, size, text, ctypes.POINTER(Options), ctypes.POINTER(screen)]),
        "afr_screen_destroy": (None, [screen]),
        "afr_screen_replace_document": (status, [screen, text, size]),
        "afr_screen_get_state": (status, [screen, text, out_text]),
        "afr_screen_set_state": (status, [screen, text, text, size]),
        "afr_screen_submit_revision": (status, [screen, text, size, out_text]),
        "afr_screen_on_action": (status, [screen, text, ACTION_CALLBACK, ctypes.c_void_p]),
        "afr_screen_on_event": (status, [screen, EVENT_CALLBACK, ctypes.c_void_p]),
        "afr_screen_on_call": (status, [screen, text, CALL_CALLBACK, ctypes.c_void_p]),
        "afr_call_return": (status, [call, text, size]),
        "afr_call_fail": (status, [call, text]),
        "afr_screen_post": (status, [screen, TASK, ctypes.c_void_p]),
        "afr_screen_run": (status, [screen]),
        "afr_screen_quit": (status, [screen]),
        "afr_screen_step": (status, [screen, ctypes.c_double]),
        "afr_screen_mouse": (status, [screen, ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_int,
                                      ctypes.c_int]),
        "afr_screen_key": (status, [screen, ctypes.c_int, ctypes.c_int, ctypes.c_int]),
        "afr_screen_text": (status, [screen, text, size]),
        "afr_screen_scroll": (status, [screen, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]),
        "afr_screen_frame": (status, [screen, ctypes.c_int, ctypes.c_int, ctypes.c_float, ctypes.c_uint32]),
        "afr_screen_render_texture": (status, [screen, ctypes.c_int, ctypes.c_int, ctypes.c_float,
                                               ctypes.POINTER(ctypes.c_uint32)]),
        "afr_screen_render_rgba": (status, [screen, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
                                            out_text]),
        "afr_screen_listen": (status, [screen, text, text]),
    }
    for name, (result, arguments) in signatures.items():
        function = getattr(library, name)
        function.restype = result
        function.argtypes = arguments


def load() -> ctypes.CDLL:
    """Loads the library and checks that its version is the one these declarations follow.

    @return The library.
    @throws OSError If it can't be found or loaded, or its version differs.
    """
    library = ctypes.CDLL(_find_library())
    _declare(library)
    version = library.afr_version()
    if (version >> 16) != VERSION_MAJOR or ((version >> 8) & 0xFF) != VERSION_MINOR:
        raise OSError(f"libaffresco {version >> 16}.{(version >> 8) & 0xFF} doesn't match the binding "
                      f"({VERSION_MAJOR}.{VERSION_MINOR})")
    return library


#: The loaded library, on first use (see library()).
_library: ctypes.CDLL | None = None


def library() -> ctypes.CDLL:
    """The library, loaded once.

    @return It.
    """
    global _library
    if _library is None:
        _library = load()
    return _library
