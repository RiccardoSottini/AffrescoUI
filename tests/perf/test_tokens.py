#!/usr/bin/env python3
"""tokens.py's include merge, which must read a screen as the host does (Screen.cpp's mergeInclude), and how it counts
a pick: no tokenizer and no affrescoui tool needed.

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


class StubMcp:
    """The parts of the MCP server pick_read uses, each giving a recognisable text."""

    @staticmethod
    def find_node(document, node_id):
        """The node with an id in the document's root's children, as (parent, node), or None."""
        found = [child for child in document["root"].get("children", []) if child.get("id") == node_id]
        return (document["root"], found[0]) if found else None

    @staticmethod
    def node_type(node, styles):
        """The node's own type."""
        return node.get("type", "element")

    @staticmethod
    def pick_lines(records):
        """One line per record: its node, type and address, and whether it is in a repeat."""
        return [f"pick {r['node']} {r['type']} {r['source'].get('address')}" + (" repeated" if r.get("repeat") else "")
                for r in records]

    @staticmethod
    def compact(value):
        """Minified JSON."""
        return json.dumps(value, separators=(",", ":"))


class Picks(unittest.TestCase):
    """What tokens.py counts for a pick, and how it shows them."""

    def test_a_lisp_screen_is_counted_on_the_json_document_beside_it(self):
        with tempfile.TemporaryDirectory() as root:
            for notation in ("lisp", "json"):
                os.makedirs(os.path.join(root, "examples", "flow", notation))
            screen = os.path.join(root, "examples", "flow", "json", "index.screen.json")
            open(screen, "w", encoding="utf-8").close()
            self.assertEqual(tokens.json_version(root, "examples/flow/lisp/index.screen.lisp"), screen)
            self.assertEqual(tokens.json_version(root, "examples/flow/json/index.screen.json"), screen)
            self.assertIsNone(tokens.json_version(root, "examples/calamaio/program/index.screen.lisp"))
        self.assertEqual(tokens.pick_name("examples/flow/lisp/index.screen.lisp"), "flow-lisp")

    def test_a_pick_reads_its_line_then_the_source_as_get_source_gives_it(self):
        card = {"type": "label", "id": "cardTitle", "repeat": {"for": "card", "in": "state.cards"}}
        document = {"root": {"type": "column", "children": [card]}}
        source = {"address": "/program/columns.lisp/card-title", "kind": "defview", "file": "columns.lisp",
                  "line": 2, "node": "#cardTitle", "pointer": "/root/children/0", "text": "(defview card-title [] ...)"}
        read = tokens.pick_read(StubMcp, document, "#cardTitle", source)
        lines = read.split("\n")
        self.assertEqual(lines[0], "pick #cardTitle label /program/columns.lisp/card-title repeated")
        self.assertEqual(json.loads(lines[1]), {key: value for key, value in source.items() if key != "text"})
        self.assertEqual(lines[2], source["text"])
        self.assertIn(" element ", tokens.pick_read(StubMcp, document, "/root", source).split("\n")[0])
        del card["repeat"]
        self.assertFalse(tokens.pick_read(StubMcp, document, "#cardTitle", source).split("\n")[0].endswith("repeated"))

    def test_the_table_lists_each_pick_and_the_ratios(self):
        results = []
        for name, pick, first in (("flow-lisp/pick/cardTitle", 200, 4000), ("flow-json/pick/cardTitle", 100, 4000),
                                  ("haven-json/pick/alarmCode", 300, 1000)):
            results += [(f"tokens/{name}", pick), (f"tokens/{name}/first-read", first),
                        (f"tokens/{name}/ratio", round(pick / first, 4))]
        table = tokens.pick_table(results + [("tokens/flow/outline", 900)])
        self.assertIn("| flow-lisp | #cardTitle | 200 | 4,000 | 0.050 |", table)
        self.assertEqual(table.count("\n| "), 4)  # the heading and the three picks
        self.assertIn("Ratio over 3 picks: median 0.050, smallest 0.025, largest 0.300", table)
        self.assertIn("the largest pick: 300 tokens (haven-json alarmCode)", table)
        self.assertEqual(tokens.pick_table([("tokens/flow/outline", 900)]), "")


if __name__ == "__main__":
    unittest.main()
