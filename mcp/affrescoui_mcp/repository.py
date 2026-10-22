"""Where the server finds what it uses besides the host: the format guide and the affrescoui command-line tool."""

import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def guide_path():
    """SCREEN_FORMAT.md: next to this file in the installed package, else at the top of the repository."""
    packaged = os.path.join(os.path.dirname(os.path.abspath(__file__)), "SCREEN_FORMAT.md")
    return packaged if os.path.exists(packaged) else os.path.join(ROOT, "SCREEN_FORMAT.md")


def find_cli():
    """The affrescoui command-line tool: $AFFRESCOUI_CLI, else on the PATH, else in the repository's build folder."""
    candidates = [os.environ.get("AFFRESCOUI_CLI"), shutil.which("affrescoui"),
                  os.path.join(ROOT, "build", "affrescoui.exe" if os.name == "nt" else "affrescoui")]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    raise RuntimeError("the affrescoui tool isn't found: build it (cmake --build build), put it on the PATH, or set "
                       "AFFRESCOUI_CLI")


def repository_path(path):
    """A path as given, or relative to the repository when it doesn't exist as given."""
    return path if os.path.isabs(path) or os.path.exists(path) else os.path.join(ROOT, path)
