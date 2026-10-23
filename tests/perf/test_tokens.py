#!/usr/bin/env python3
"""tokens.py's include merge, which must read a screen as the host does (Screen.cpp's mergeInclude): no tokenizer
needed.

    python3 tests/perf/test_tokens.py
"""

import importlib.util
import json
import os
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPEC = importlib.util.spec_from_file_location("tokens", os.path.join(HERE, "tokens.py"))
tokens = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tokens)


class WithIncludes(unittest.TestCase):
    """A screen with an include, merged."""

    def merged(self, screen, part, nested=None):
        """The screen with `part` (in parts/part.json, itself including `nested` if given) merged in."""
        with tempfile.TemporaryDirectory() as folder:
            os.makedirs(os.path.join(folder, "parts"))
            if nested is not None:
                part = {**part, "include": ["nested.json"]}
                with open(os.path.join(folder, "parts", "nested.json"), "w", encoding="utf-8") as file:
                    json.dump(nested, file)
            with open(os.path.join(folder, "parts", "part.json"), "w", encoding="utf-8") as file:
                json.dump(part, file)
            return tokens.with_includes({**screen, "include": ["parts/part.json"]}, folder)

    def test_strings_merge_locale_by_locale_and_the_screen_wins(self):
        screen = {"strings": {"en": {"title": "Haven", "save": "Save"}, "it": {"title": "Haven"}}}
        part = {"strings": {"en": {"ask": "Ask", "save": "Keep"}, "it": {"ask": "Chiedi"}, "ar": {"ask": "اسأل"}}}
        strings = self.merged(screen, part)["strings"]
        self.assertEqual(strings["en"], {"ask": "Ask", "save": "Save", "title": "Haven"})
        self.assertEqual(strings["it"], {"ask": "Chiedi", "title": "Haven"})
        self.assertEqual(strings["ar"], {"ask": "اسأل"})

    def test_menus_merge_menu_by_menu(self):
        screen = {"menus": {"file": {"open": {"title": "Open"}}}}
        part = {"menus": {"file": {"ask": {"title": "Ask"}}, "help": {"about": {"title": "About"}}}}
        menus = self.merged(screen, part)["menus"]
        self.assertEqual(set(menus["file"]), {"open", "ask"})
        self.assertEqual(set(menus["help"]), {"about"})

    def test_other_sections_persist_and_the_screens_own_keys(self):
        screen = {"version": 2, "root": {"id": "root"}, "actions": {"a": [], "b": ["mine"]}, "persist": ["state.x"]}
        part = {"version": 1, "$schema": "x", "root": {"id": "other"}, "window": {"title": "W"},
                "actions": {"b": ["theirs"], "c": []}, "persist": ["state.y", "state.x"], "styles": {"s": {}}}
        merged = self.merged(screen, part)
        self.assertEqual(merged["actions"], {"a": [], "b": ["mine"], "c": []})
        self.assertEqual(merged["persist"], ["state.y", "state.x"])
        self.assertEqual(merged["root"], {"id": "root"})
        self.assertEqual(merged["version"], 2)
        self.assertEqual(merged["window"], {"title": "W"})
        self.assertEqual(merged["styles"], {"s": {}})
        self.assertNotIn("$schema", merged)
        self.assertNotIn("include", merged)

    def test_nested_includes_merge_too(self):
        merged = self.merged({"strings": {"en": {"a": "A"}}}, {"strings": {"en": {"b": "B"}}},
                             nested={"strings": {"en": {"c": "C", "b": "nested"}}})
        self.assertEqual(merged["strings"]["en"], {"a": "A", "b": "B", "c": "C"})


if __name__ == "__main__":
    unittest.main()
