#!/usr/bin/env python3
"""Makes the font subsets in this folder from the original fonts (README.md says where each one comes from).

Each subset holds only the characters the screens of the rendering check show in its script, so the check's fonts
stay a few kilobytes. It needs HarfBuzz's hb-subset (2.6 or later; `brew install harfbuzz`, `apt install
libharfbuzz-bin`), which keeps the layout tables shaping needs: joining forms, conjuncts, ligatures, mark positions.

    python3 tests/render/fonts/subset.py <folder with the original fonts>
"""

import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
SCREENS = [HERE.parent / "screens" / "scripts.screen.json"]


def screen_texts(path):
    """Every "text" of a screen's nodes, and its state's strings."""
    texts = []

    def walk(node):
        if isinstance(node, dict):
            for key, value in node.items():
                if key == "text" and isinstance(value, str):
                    texts.append(value)
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    screen = json.loads(path.read_text(encoding="utf-8"))
    walk(screen.get("root", {}))
    texts.extend(value for value in screen.get("state", {}).values() if isinstance(value, str))
    return texts


def locale_texts(path, locale):
    """A document's strings in one locale."""
    document = json.loads(path.read_text(encoding="utf-8"))
    return [value for value in document.get("strings", {}).get(locale, {}).values() if isinstance(value, str)]


def in_ranges(*ranges):
    """A test of whether a character is in one of some ranges of code points."""
    return lambda character: any(first <= ord(character) <= last for first, last in ranges)


CJK = ((0x3000, 0x30FF), (0x4E00, 0x9FFF), (0xFF00, 0xFFEF))

# (subset, original file, axes to pin, the lines it is for (a word they hold, None for all, or a list of texts of
# another document, such as the assistant test screen's Arabic strings), its characters)
SUBSETS = [
    ("NotoSansArabic-Subset.ttf", "NotoSansArabic[wdth,wght].ttf", "wght=400,wdth=100", None,
     in_ranges((0x0600, 0x06FF))),
    ("NotoSansDevanagari-Subset.ttf", "NotoSansDevanagari[wdth,wght].ttf", "wght=400,wdth=100", None,
     in_ranges((0x0900, 0x097F))),
    ("NotoSansSC-Subset.ttf", "NotoSansSC[wght].ttf", "wght=400", ("Chinese", "Wrapped"), in_ranges(*CJK)),
    ("NotoSansJP-Subset.ttf", "NotoSansJP[wght].ttf", "wght=400", ("Japanese",), in_ranges(*CJK)),
    ("NotoSansKR-Subset.ttf", "NotoSansKR[wght].ttf", "wght=400", None, in_ranges((0xAC00, 0xD7AF))),
    ("NotoSansArabic-Assistant.ttf", "NotoSansArabic[wdth,wght].ttf", "wght=400,wdth=100",
     locale_texts(HERE.parent / "screens" / "assistant.screen.json", "ar"), in_ranges((0x0600, 0x06FF))),
    ("TwemojiMozilla-Subset.ttf", "Twemoji.Mozilla.ttf", None, None,
     in_ranges((0x1F000, 0x1FFFF), (0x200D, 0x200D), (0xFE0F, 0xFE0F), (0x2705, 0x2705), (0x2764, 0x2764))),
]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    originals = pathlib.Path(sys.argv[1])
    texts = [text for screen in SCREENS for text in screen_texts(screen)]

    for subset, original, axes, lines, wanted in SUBSETS:
        if isinstance(lines, list):
            chosen = lines
        else:
            chosen = [text for text in texts if lines is None or any(word in text for word in lines)]
        characters = sorted({character for text in chosen for character in text if wanted(character)})
        command = ["hb-subset", str(originals / original), "--unicodes=" + ",".join(f"{ord(c):04X}" for c in characters),
                   "--output-file=" + str(HERE / subset)]
        if axes:
            command.append("--instance=" + axes)
        subprocess.run(command, check=True)
        print(f"{subset}: {len(characters)} characters, {(HERE / subset).stat().st_size} bytes")


if __name__ == "__main__":
    main()
