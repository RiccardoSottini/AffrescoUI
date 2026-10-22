"""The binding, headless: state, revisions, actions, events, native calls, input, pictures and errors."""

from __future__ import annotations

import json
import struct
import threading
import zlib

import pytest

import affrescoui
from affrescoui import AffrescoError, Screen

from conftest import DOCUMENT


def test_version():
    """The library's version is the one the binding follows."""
    major, minor, _ = affrescoui.version()
    assert (major, minor) == (0, 1)


def test_state(screen):
    """State reads by dotted path and JSON Pointer, and sets as revisions."""
    assert screen.state["count"] == 1
    assert screen.state["/items/0/title"] == "a/b"
    assert screen.state["items.0.title"] == "a/b"
    assert screen.state.all()["result"] == 0
    with pytest.raises(KeyError):
        screen.state["missing"]
    assert screen.state.get("missing", 7) == 7
    assert "count" in screen.state and "missing" not in screen.state
    screen.state["count"] = 5
    screen.state["items.0.title"] = "c"
    assert screen.state["count"] == 5 and screen.state["items.0.title"] == "c"


def test_revisions(screen):
    """Revisions are applied or rejected, and replacing the document swaps the state."""
    applied = screen.submit_revision({"author": "agent", "message": "count", "set": {"/state/count": 9}})
    assert applied["status"] == "applied"
    assert screen.state["count"] == 9
    rejected = screen.submit_revision({"author": "agent", "message": "bad", "patch": [{"op": "remove", "path": "/x"}]})
    assert rejected["status"] == "rejected"
    changed = json.loads(json.dumps(DOCUMENT))
    changed["state"]["count"] = 2
    screen.replace_document(changed)
    assert screen.state["count"] == 2


def test_actions_events_calls(screen):
    """Clicks run the host's action, report events, and reach granted native calls."""
    senders, events = [], []

    @screen.on_action("hostAction")
    def host_action(sender):
        senders.append(sender)

    @screen.on_event
    def on_event(event):
        events.append(event)

    @screen.on_call("double")
    def double(arguments):
        return arguments["n"] * 2

    @screen.on_call("broken")
    def broken(_arguments):
        raise RuntimeError("no")

    screen.step(0)
    screen.click(10, 10)
    screen.click(10, 40)
    screen.click(10, 70)
    screen.step(16)
    assert senders == ["go"]
    assert any("clicked" in json.dumps(event) for event in events)
    assert screen.state["result"] == 42
    assert screen.state["failed"] is True


def test_post_from_threads_while_closing(sockets):
    """Posting from other threads while the screen closes either queues the task or raises ValueError, never crashes."""
    screen = Screen.from_string(DOCUMENT, socket_directory=sockets)
    errors = []

    def poster():
        for _ in range(200):
            try:
                screen.post(lambda: None)
            except ValueError:
                return
            except Exception as error:  # noqa: BLE001 — collected and checked below
                errors.append(error)
                return
    threads = [threading.Thread(target=poster) for _ in range(4)]
    for thread in threads:
        thread.start()
    screen.step(1)
    screen.close()
    for thread in threads:
        thread.join()
    assert errors == []
    with pytest.raises(ValueError):
        screen.post(lambda: None)


def test_post_and_input(screen):
    """Posted tasks run on step; keys, text and scrolls are taken; bad input raises."""
    ran = []
    screen.post(lambda: ran.append(True))
    screen.step(16)
    assert ran == [True]
    screen.mouse("move", 5, 5)
    screen.key(65, "press")
    screen.key(65, "release")
    screen.type_text("héllo ✓")
    screen.scroll(10, 10, 0, -3)
    with pytest.raises(ValueError):
        screen.mouse("wiggle", 0, 0)
    with pytest.raises(AffrescoError) as error:
        screen.key(9999)
    assert error.value.status == 1
    with pytest.raises(AffrescoError):
        screen.scroll(float("nan"), 0, 0, 0)
    with pytest.raises(AffrescoError) as error:
        screen.run()  # a window call on a headless screen
    assert error.value.status == 3


def test_pictures(screen, tmp_path):
    """The picture has the window's size, the blue button's color, and encodes as a valid PNG."""
    width, height, rgba = screen.render_rgba()
    assert (width, height) == (200, 120)
    at = (2 * width + 2) * 4
    assert tuple(rgba[at:at + 4]) == (0x20, 0x40, 0x80, 255)
    path = tmp_path / "screen.png"
    png = screen.render_png(path)
    assert path.read_bytes() == png and png.startswith(b"\x89PNG\r\n\x1a\n")
    length = struct.unpack(">I", png[33:37])[0]
    assert png[37:41] == b"IDAT"
    raw = zlib.decompress(png[41:41 + length])
    assert len(raw) == height * (1 + width * 4)


def test_files_and_errors(tmp_path, sockets):
    """Screens load from files at a scale; bad documents and options raise; a closed screen refuses calls."""
    path = tmp_path / "test.screen.json"
    path.write_text(json.dumps(DOCUMENT))
    with Screen.from_file(path, scale=2, history_in_memory=1, socket_directory=sockets,
                          native_actions=["hostAction"]) as screen:
        width, height, _ = screen.render_rgba()
        assert (width, height) == (400, 240)
        screen.listen("py-test.sock", "agent")
        with pytest.raises(AffrescoError):
            screen.listen("../escape", "agent")
    assert screen.mode == "headless"
    with pytest.raises(ValueError):
        screen.step()
    with pytest.raises(AffrescoError) as error:
        Screen.from_string("{nope", socket_directory=sockets)
    assert error.value.status == 2
    with pytest.raises(ValueError):
        Screen.from_string(DOCUMENT, mode="floating")
    with pytest.raises(ValueError):
        Screen.from_string(DOCUMENT, colour="red")
