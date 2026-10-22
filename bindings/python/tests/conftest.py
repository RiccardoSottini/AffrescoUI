"""Shared pieces of the binding's tests: a small screen document and a headless screen made from it."""

from __future__ import annotations

import os
import shutil
import tempfile

import pytest

from affrescoui import Screen

#: A 200 by 120 screen: a blue button "go" that runs the native action "hostAction", a button "calc" that emits
#: "clicked" and calls the native function "double", and a button "broken" whose call fails.
DOCUMENT = {
    "format": "affrescoui-screen",
    "version": 1,
    "window": {"title": "Python", "width": 200, "height": 120},
    "state": {"count": 1, "result": 0, "failed": False, "items": [{"title": "a/b"}]},
    "root": {"id": "root", "children": [
        {"id": "go", "anchors": {"left": 0, "top": 0}, "color": "#204080", "width": 80, "height": 30,
         "onClick": "hostAction"},
        {"id": "calc", "anchors": {"left": 0, "top": 30}, "width": 80, "height": 30, "onClick": [
            {"emit": "clicked", "data": {"n": 1}},
            {"call": "double", "with": {"n": 21}, "into": "state.result"}]},
        {"id": "broken", "anchors": {"left": 0, "top": 60}, "width": 80, "height": 30, "onClick": [
            {"call": "broken", "onError": [{"set": "state.failed", "to": True}]}]},
    ]},
}


@pytest.fixture
def sockets():
    """A short folder of its own for the screen's sockets (a socket's path is limited to about 100 bytes, which
    pytest's tmp_path can pass).

    @return Its path.
    """
    folder = tempfile.mkdtemp(prefix="afr", dir="/tmp" if os.path.isdir("/tmp") else None)
    yield folder
    shutil.rmtree(folder, ignore_errors=True)


@pytest.fixture
def screen(sockets):
    """A headless screen of DOCUMENT, closed after the test.

    @param sockets The folder for its sockets.
    @return The screen.
    """
    with Screen.from_string(DOCUMENT, socket_directory=sockets) as made:
        yield made
