"""AffrescoUI from Python: load a screen document, read and change its state, answer its actions and native calls,
and show it in a window or drive it headless, on the C API (affresco/affresco.h) through ctypes.

    with Screen.from_file("app.screen.json") as screen:
        @screen.on_action("save")
        def save(sender):
            print("saved from", sender)

        screen.state["count"] = 3
        screen.click(40, 20)
        screen.render_png("app.png")
"""

from __future__ import annotations

import ctypes
import json
import struct
import zlib
from collections.abc import Callable
from pathlib import Path
from typing import Any

from . import _native

__all__ = ["AffrescoError", "Screen", "State", "version"]


class AffrescoError(Exception):
    """A C API call that failed: its afr_status and the library's message."""

    def __init__(self, status: int, message: str) -> None:
        """Keeps the status and message.

        @param status The afr_status.
        @param message afr_last_error() of the call.
        """
        super().__init__(f"{message} (status {status})")
        self.status = status
        self.message = message


def _check(status: int) -> None:
    """Raises if a call failed.

    @param status Its afr_status.
    @throws AffrescoError If it isn't AFR_OK.
    """
    if status != _native.OK:
        raise AffrescoError(status, _native.library().afr_last_error().decode("utf-8", "replace"))


def _taken(pointer: ctypes.c_void_p) -> str:
    """Reads a string the library returned, and frees it.

    @param pointer It.
    @return Its text.
    """
    if not pointer.value:
        return ""
    try:
        return ctypes.string_at(pointer.value).decode("utf-8")
    finally:
        _native.library().afr_free(pointer)


def _json(value: Any) -> bytes:
    """A value as JSON text.

    @param value It.
    @return Its UTF-8 bytes.
    """
    return json.dumps(value, ensure_ascii=False).encode("utf-8")


def version() -> tuple[int, int, int]:
    """The library's version.

    @return (major, minor, patch).
    """
    packed = _native.library().afr_version()
    return packed >> 16, (packed >> 8) & 0xFF, packed & 0xFF


def _options(mode: str, **values: Any) -> _native.Options:
    """Builds afr_options from keyword arguments.

    @param mode "window", "headless" or "embedded".
    @param values socket_directory, data_directory, history_in_memory, width, height, scale, hidden.
    @return The options.
    @throws ValueError For an unknown mode or option.
    """
    if mode not in _native.MODES:
        raise ValueError(f"mode must be one of {sorted(_native.MODES)}")
    options = _native.Options()
    _check(_native.library().afr_options_init(ctypes.byref(options)))
    options.mode = _native.MODES[mode]
    for name, value in values.items():
        if value is None:
            continue
        if name in ("socket_directory", "data_directory"):
            value = str(value).encode("utf-8")
        elif name not in ("history_in_memory", "width", "height", "scale", "hidden"):
            raise ValueError(f"unknown option {name!r}")
        setattr(options, name, value)
    return options


class State:
    """A screen's state, read and set by path: "items.0.title" (dots), "/items/0/title" (a JSON Pointer), or "" for all
    of it. Setting is a revision by "application", so the document's rules still decide."""

    def __init__(self, screen: Screen) -> None:
        """Keeps the screen.

        @param screen It.
        """
        self._screen = screen

    def __getitem__(self, path: str) -> Any:
        """Reads a value.

        @param path Where.
        @return It, as Python values.
        @throws KeyError If nothing is there.
        """
        out = ctypes.c_void_p()
        status = _native.library().afr_screen_get_state(self._screen._handle(), path.encode("utf-8"), ctypes.byref(out))
        if status == _native.ERROR_NOT_FOUND:
            raise KeyError(path)
        _check(status)
        return json.loads(_taken(out))

    def __setitem__(self, path: str, value: Any) -> None:
        """Sets a value.

        @param path Where; not "".
        @param value What, any JSON value.
        """
        text = _json(value)
        _check(_native.library().afr_screen_set_state(self._screen._handle(), path.encode("utf-8"), text, len(text)))

    def get(self, path: str, default: Any = None) -> Any:
        """Reads a value, or a default if nothing is there.

        @param path Where.
        @param default What to return then.
        @return The value or the default.
        """
        try:
            return self[path]
        except KeyError:
            return default

    def __contains__(self, path: str) -> bool:
        """Whether something is at a path.

        @param path Where.
        @return Whether.
        """
        return self.get(path, _missing) is not _missing

    def all(self) -> Any:
        """The whole state.

        @return It.
        """
        return self[""]


_missing = object()


class Screen:
    """A screen: a document, its state and what shows it. Use it from the thread that created it; close it (or use it as
    a context manager) when done. Window and headless screens need the main thread, one at a time."""

    def __init__(self, handle: ctypes.c_void_p, mode: str) -> None:
        """Wraps a handle; use from_file() or from_string().

        @param handle afr_screen*.
        @param mode Its mode.
        """
        self._pointer = handle
        self.mode = mode
        self.state = State(self)
        self._keep: list[Any] = []  # ctypes callbacks the library holds: alive as long as the screen
        self._event_handlers: list[Callable[[dict], None]] = []

    @classmethod
    def from_file(cls, path: str | Path, mode: str = "headless", **options: Any) -> Screen:
        """Loads a screen from a file.

        @param path The file.
        @param mode "headless" (default), "window" or "embedded".
        @param options socket_directory, data_directory, history_in_memory, width, height, scale, hidden.
        @return The screen.
        """
        built = _options(mode, **options)
        handle = ctypes.c_void_p()
        _check(_native.library().afr_screen_create_from_file(str(path).encode("utf-8"), ctypes.byref(built),
                                                              ctypes.byref(handle)))
        return cls(handle, mode)

    @classmethod
    def from_string(cls, document: str | dict, base_directory: str | Path | None = None, mode: str = "headless",
                    **options: Any) -> Screen:
        """Loads a screen from JSON text or a dict, its history in memory.

        @param document The document.
        @param base_directory Where relative paths in it are resolved, or None for the current directory.
        @param mode As for from_file().
        @param options As for from_file().
        @return The screen.
        """
        text = (document if isinstance(document, str) else json.dumps(document)).encode("utf-8")
        base = str(base_directory).encode("utf-8") if base_directory is not None else None
        built = _options(mode, **options)
        handle = ctypes.c_void_p()
        _check(_native.library().afr_screen_create_from_string(text, len(text), base, ctypes.byref(built),
                                                                ctypes.byref(handle)))
        return cls(handle, mode)

    def _handle(self) -> ctypes.c_void_p:
        """The handle, checked.

        @return It.
        @throws ValueError If the screen is closed.
        """
        if not self._pointer:
            raise ValueError("the screen is closed")
        return self._pointer

    def close(self) -> None:
        """Closes the screen; closing again does nothing."""
        if self._pointer:
            _native.library().afr_screen_destroy(self._pointer)
            self._pointer = None
            self._keep.clear()

    def __enter__(self) -> Screen:
        """@return The screen."""
        return self

    def __exit__(self, *_: Any) -> None:
        """Closes the screen."""
        self.close()

    def __del__(self) -> None:
        """Closes the screen if it wasn't."""
        try:
            self.close()
        except Exception:  # noqa: BLE001 — at interpreter exit the library may be gone
            pass

    def replace_document(self, document: str | dict) -> None:
        """Replaces the whole document, as a revision by "application".

        @param document JSON text or a dict.
        """
        text = (document if isinstance(document, str) else json.dumps(document)).encode("utf-8")
        _check(_native.library().afr_screen_replace_document(self._handle(), text, len(text)))

    def submit_revision(self, revision: dict) -> dict:
        """Submits a revision ({"author", "message", "patch"} or {"set": {...}}, SCREEN_FORMAT.md).

        @param revision It.
        @return {"status": "applied"|"proposed"|"rejected", ...}; rejected is returned, not raised.
        """
        text = _json(revision)
        out = ctypes.c_void_p()
        status = _native.library().afr_screen_submit_revision(self._handle(), text, len(text), ctypes.byref(out))
        result = _taken(out)
        if status not in (_native.OK, _native.ERROR_REFUSED) or not result:
            _check(status)
        return json.loads(result)

    def on_action(self, name: str) -> Callable[[Callable[[str], None]], Callable[[str], None]]:
        """A decorator: the function runs when the document runs the native action of that name; it gets the
        sender's id ("" if none). Exceptions it raises are printed, not passed into the library.

        @param name The action's name.
        @return The decorator.
        """
        def register(function: Callable[[str], None]) -> Callable[[str], None]:
            def callback(_user: Any, _action: bytes, sender: bytes) -> None:
                _guarded(function, sender.decode("utf-8"))
            native = _native.ACTION_CALLBACK(callback)
            _check(_native.library().afr_screen_on_action(self._handle(), name.encode("utf-8"), native, None))
            self._keep.append(native)
            return function
        return register

    def on_event(self, function: Callable[[dict], None]) -> Callable[[dict], None]:
        """A decorator: the function gets every event the screen reports (a revision applied, a proposal, an action
        run), as a dict. Several may be registered.

        @param function It.
        @return It.
        """
        if not self._event_handlers:
            def callback(_user: Any, event: bytes) -> None:
                value = json.loads(event.decode("utf-8"))
                for handler in list(self._event_handlers):
                    _guarded(handler, value)
            native = _native.EVENT_CALLBACK(callback)
            _check(_native.library().afr_screen_on_event(self._handle(), native, None))
            self._keep.append(native)
        self._event_handlers.append(function)
        return function

    def on_call(self, name: str) -> Callable[[Callable[..., Any]], Callable[..., Any]]:
        """A decorator that grants the document a native function: the function gets the call's arguments (a dict)
        and returns the value the document receives; an exception it raises becomes the call's error.

        @param name The function's name.
        @return The decorator.
        """
        def register(function: Callable[..., Any]) -> Callable[..., Any]:
            def callback(_user: Any, arguments: bytes, call: int) -> None:
                library = _native.library()
                try:
                    text = _json(function(json.loads(arguments.decode("utf-8"))))
                except Exception as error:  # noqa: BLE001 — the document sees it as the call's error
                    library.afr_call_fail(call, str(error).encode("utf-8") or b"failed")
                    return
                library.afr_call_return(call, text, len(text))
            native = _native.CALL_CALLBACK(callback)
            _check(_native.library().afr_screen_on_call(self._handle(), name.encode("utf-8"), native, None))
            self._keep.append(native)
            return function
        return register

    def post(self, task: Callable[[], None]) -> None:
        """Queues a task for the screen's thread; it runs in run(), step() or a frame. Safe from any thread.

        @param task It.
        """
        holder: list[Any] = []

        def callback(_user: Any) -> None:
            try:
                _guarded(task)
            finally:
                self._keep.remove(holder[0])
        native = _native.TASK(callback)
        holder.append(native)
        self._keep.append(native)
        _check(_native.library().afr_screen_post(self._handle(), native, None))

    def run(self) -> None:
        """Window: shows the window and runs until it is closed or quit()."""
        _check(_native.library().afr_screen_run(self._handle()))

    def quit(self) -> None:
        """Window: makes run() return."""
        _check(_native.library().afr_screen_quit(self._handle()))

    def step(self, milliseconds: float = 0.0) -> None:
        """Headless: moves the clock, runs posted tasks and timers, and lays out. Embedded: runs posted tasks.

        @param milliseconds How far to move the clock (0 to 1e9).
        """
        _check(_native.library().afr_screen_step(self._handle(), milliseconds))

    def mouse(self, action: str, x: float, y: float, button: int = 0, modifiers: int = 0) -> None:
        """Sends a mouse event, in window units from the top left.

        @param action "move", "press", "release", "enter" or "leave".
        @param x Where, horizontally.
        @param y Where, vertically.
        @param button 0 left, 1 right, 2 middle.
        @param modifiers AFR_MOD_* bits (GLFW's).
        """
        actions = {"move": _native.MOUSE_MOVE, "press": _native.MOUSE_PRESS, "release": _native.MOUSE_RELEASE,
                   "enter": _native.MOUSE_ENTER, "leave": _native.MOUSE_LEAVE}
        if action not in actions:
            raise ValueError(f"action must be one of {sorted(actions)}")
        _check(_native.library().afr_screen_mouse(self._handle(), actions[action], x, y, button, modifiers))

    def click(self, x: float, y: float, button: int = 0) -> None:
        """Presses and releases a mouse button at a point.

        @param x Where, horizontally.
        @param y Where, vertically.
        @param button Which.
        """
        self.mouse("press", x, y, button)
        self.mouse("release", x, y, button)

    def key(self, key: int, action: str = "press", modifiers: int = 0) -> None:
        """Sends a key event.

        @param key A GLFW key code (-1 for an unknown key).
        @param action "press", "release" or "repeat".
        @param modifiers AFR_MOD_* bits.
        """
        actions = {"release": _native.KEY_RELEASE, "press": _native.KEY_PRESS, "repeat": _native.KEY_REPEAT}
        if action not in actions:
            raise ValueError(f"action must be one of {sorted(actions)}")
        _check(_native.library().afr_screen_key(self._handle(), key, actions[action], modifiers))

    def type_text(self, text: str) -> None:
        """Types text, one character event per code point.

        @param text It.
        """
        data = text.encode("utf-8")
        _check(_native.library().afr_screen_text(self._handle(), data, len(data)))

    def scroll(self, x: float, y: float, dx: float, dy: float) -> None:
        """Scrolls at a point.

        @param x Where, horizontally.
        @param y Where, vertically.
        @param dx How far, horizontally.
        @param dy How far, vertically.
        """
        _check(_native.library().afr_screen_scroll(self._handle(), x, y, dx, dy))

    def listen(self, name: str, author: str) -> None:
        """Opens a channel for peers (the MCP server, the Studio), their revisions authored as `author`.

        @param name The socket's name, inside the socket folder.
        @param author Who they are, for the document's rules.
        """
        _check(_native.library().afr_screen_listen(self._handle(), name.encode("utf-8"), author.encode("utf-8")))

    def render_rgba(self) -> tuple[int, int, bytes]:
        """Draws the screen from scratch and reads it back.

        @return (width, height, RGBA bytes, top row first).
        """
        width, height, out = ctypes.c_int(), ctypes.c_int(), ctypes.c_void_p()
        _check(_native.library().afr_screen_render_rgba(self._handle(), ctypes.byref(width), ctypes.byref(height),
                                                        ctypes.byref(out)))
        try:
            return width.value, height.value, ctypes.string_at(out.value, width.value * height.value * 4)
        finally:
            _native.library().afr_free(out)

    def render_png(self, path: str | Path | None = None) -> bytes:
        """Draws the screen as a PNG.

        @param path Where to write it, or None.
        @return The PNG's bytes.
        """
        png = encode_png(*self.render_rgba())
        if path is not None:
            Path(path).write_bytes(png)
        return png


def _guarded(function: Callable[..., Any], *arguments: Any) -> None:
    """Runs a callback; an exception is printed, never raised into the library (ctypes can't pass it through C).

    @param function It.
    @param arguments Its arguments.
    """
    try:
        function(*arguments)
    except Exception:  # noqa: BLE001
        import traceback
        traceback.print_exc()


def encode_png(width: int, height: int, rgba: bytes) -> bytes:
    """Encodes RGBA pixels (top row first) as a PNG, with zlib only.

    @param width In pixels.
    @param height In pixels.
    @param rgba The pixels.
    @return The PNG's bytes.
    """
    stride = width * 4
    raw = b"".join(b"\x00" + rgba[row * stride:(row + 1) * stride] for row in range(height))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
