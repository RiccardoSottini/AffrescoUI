"""Renders a screen document to a PNG, headless: python render_screen.py app.screen.json app.png [scale]."""

from __future__ import annotations

import sys
import tempfile

from affrescoui import Screen


def main(arguments: list[str]) -> int:
    """Loads the document, lets its timers run for a moment, and writes the picture.

    @param arguments The document's path, the PNG's path, and optionally a whole scale.
    @return The exit status.
    """
    if len(arguments) not in (2, 3):
        print(__doc__, file=sys.stderr)
        return 2
    scale = int(arguments[2]) if len(arguments) == 3 else 1
    with tempfile.TemporaryDirectory() as scratch, \
            Screen.from_file(arguments[0], scale=scale, history_in_memory=1, socket_directory=scratch) as screen:
        screen.step(100)
        screen.render_png(arguments[1])
    print("wrote", arguments[1])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
