"""A counter driven from Python, headless: a document made in code, a native action, a native call, and the state
read back after simulated clicks: python counter.py [picture.png] (default counter.png)."""

from __future__ import annotations

import sys
import tempfile

from affrescoui import Screen

DOCUMENT = {
    "format": "affrescoui-screen",
    "version": 1,
    "window": {"title": "Counter", "width": 240, "height": 80},
    "state": {"count": 0, "label": ""},
    "root": {"id": "root", "children": [
        {"id": "add", "anchors": {"left": 10, "top": 10}, "width": 100, "height": 30, "color": "#2a6", "onClick": [
            {"increment": "state.count"},
            {"call": "describe", "with": {"n": "{{state.count}}"}, "into": "state.label"}]},
        {"id": "reset", "anchors": {"left": 130, "top": 10}, "width": 100, "height": 30, "color": "#a33",
         "onClick": "reset"},
    ]},
}


def main(picture: str) -> None:
    """Clicks "add" three times and "reset" once, printing the state as it goes.

    @param picture Where to write the picture after the third click.
    """
    with tempfile.TemporaryDirectory() as scratch, Screen.from_string(DOCUMENT, socket_directory=scratch) as screen:
        @screen.on_call("describe")
        def describe(arguments):
            return f"clicked {arguments['n']} times"

        @screen.on_action("reset")
        def reset(sender):
            print("reset from", sender)
            screen.state["count"] = 0

        screen.step(0)
        for _ in range(3):
            screen.click(60, 25)
        screen.step(16)
        print("count:", screen.state["count"], "| label:", screen.state["label"])
        screen.render_png(picture)
        screen.click(180, 25)
        print("count after reset:", screen.state["count"])


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "counter.png")
