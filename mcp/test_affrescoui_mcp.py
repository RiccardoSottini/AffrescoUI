#!/usr/bin/env python3
"""Unit tests for the MCP server's queries, which need no host: the outline (and its depth), one node (and its depth),
find_nodes, the format guide by topic, and the changes a re-read returns. ctest "mcp", or:

    python3 mcp/test_affrescoui_mcp.py
"""

import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import affrescoui_mcp as mcp  # noqa: E402

SCREEN = {
    "styles": {"caption": {"type": "label", "textColor": "#888888"}, "note": {"style": "caption"}},
    "components": {"chip": {"node": {"id": "label-chip", "type": "label", "text": "{{name}}"}}},
    "root": {"id": "panel-root", "children": [
        {"id": "topBar", "children": [
            {"id": "logo", "type": "label", "text": "Flow"},
            {"id": "label-blocked", "style": "note", "text": "Blocked"},
            {"id": "row-filters", "type": "row", "children": [
                {"id": "chip-all", "use": "chip", "onClick": "showAll"},
                {"id": "label-filter", "type": "label", "text": "{{tag}}",
                 "repeat": {"for": "tag", "in": "state.filters"}, "when": "{{tag != 'all'}}"}]}]},
        {"id": "textarea-new-card", "type": "textarea", "bind": "state.newCard", "placeholder": "New card"}]}}


class Outline(unittest.TestCase):
    def test_every_node_once_with_what_it_shows(self):
        self.assertEqual(mcp.outline(SCREEN), "\n".join([
            "#panel-root",
            " #topBar panel",
            '  #logo label "Flow"',
            '  #label-blocked "Blocked"',  # a label by its style's style; the id says so
            "  #row-filters",
            "   #chip-all use chip onClick=showAll",
            '   #label-filter "{{tag}}" repeat tag in state.filters when',
            ' #textarea-new-card placeholder="New card" bind=state.newCard',
            '#label-chip "{{name}}"']))

    def test_depth_stops_and_counts_what_is_inside(self):
        self.assertEqual(mcp.outline(SCREEN, depth=1), "\n".join([
            "#panel-root",
            " #topBar panel +5",
            ' #textarea-new-card placeholder="New card" bind=state.newCard',
            '#label-chip "{{name}}"']))

    def test_at_starts_at_a_node(self):
        self.assertEqual(mcp.outline(SCREEN, at="#row-filters", depth=0), "#row-filters +2")
        with self.assertRaises(ValueError):
            mcp.outline(SCREEN, at="#nope")

    def test_long_text_is_cut(self):
        node = {"id": "label-long", "type": "label", "text": "A sentence much longer than an outline line needs"}
        self.assertEqual(mcp.summary(node, "#label-long"), '"A sentence much longer t..."')


class Nodes(unittest.TestCase):
    def test_locate_by_id_and_pointer(self):
        self.assertEqual(mcp.locate(SCREEN, "#logo"), ("/root/children/0/children/0", SCREEN["root"]["children"][0]
                                                       ["children"][0]))
        self.assertEqual(mcp.locate(SCREEN, "#logo/text"), ("/root/children/0/children/0/text", "Flow"))
        self.assertEqual(mcp.locate(SCREEN, "/root/id")[1], "panel-root")
        with self.assertRaisesRegex(ValueError, "nothing at #logo/colour"):
            mcp.locate(SCREEN, "#logo/colour")

    def test_depth_turns_deeper_children_into_addresses(self):
        node = mcp.pruned(mcp.locate(SCREEN, "#topBar")[1], "#topBar", 0)
        self.assertEqual(node["children"], ["#logo", "#label-blocked", "#row-filters"])
        node = mcp.pruned(SCREEN["root"], "#panel-root", 1)
        self.assertEqual(node["children"][0]["children"], ["#logo", "#label-blocked", "#row-filters"])
        self.assertEqual(node["children"][1], SCREEN["root"]["children"][1])

    def test_find_by_type_text_and_property(self):
        self.assertEqual(mcp.find_nodes(SCREEN, kind="label").split("\n")[0], "4 found")  # styles count
        self.assertEqual(mcp.find_nodes(SCREEN, text="BLOCK"), '1 found\n#label-blocked "Blocked" in #topBar')
        self.assertEqual(mcp.find_nodes(SCREEN, kind="chip"), "1 found\n#chip-all use chip onClick=showAll in "
                                                               "#row-filters")
        self.assertEqual(mcp.find_nodes(SCREEN, prop="bind", value="state.newCard").split("\n")[1],
                         '#textarea-new-card placeholder="New card" bind=state.newCard in #panel-root')
        self.assertEqual(mcp.find_nodes(SCREEN, prop="bind", value="state.other"), "0 found")
        self.assertTrue(mcp.find_nodes(SCREEN, limit=2).endswith("... and 7 more: narrow the search or raise limit"))


class Guide(unittest.TestCase):
    GUIDE = "# Format\n\nIntro.\n\n## Nodes\nAbout nodes.\n\n### `label`\nText.\n\n```\n## not a heading\n```\n" \
            "\n## Revisions\nEdits."

    def test_contents_by_default(self):
        self.assertEqual(mcp.format_guide(self.GUIDE), "# Format\n\nIntro.\n\nTopics (pass one as `topic`, or \"all\" "
                                                       "for the whole guide): Nodes, label, Revisions.")

    def test_a_topic_and_its_subsections(self):
        self.assertTrue(mcp.format_guide(self.GUIDE, "nodes").startswith("## Nodes\nAbout nodes.\n\n### `label`"))
        self.assertIn("## not a heading", mcp.format_guide(self.GUIDE, "label"))
        self.assertEqual(mcp.format_guide(self.GUIDE, "revis"), "## Revisions\nEdits.")
        self.assertIsNone(mcp.format_guide(self.GUIDE, "nope"))
        self.assertEqual(mcp.format_guide(self.GUIDE, "all"), self.GUIDE)

    def test_the_real_guide_has_its_topics(self):
        with open(mcp.guide_path(), encoding="utf-8") as file:
            text = file.read()
        contents = mcp.format_guide(text)
        for topic in ("Revisions", "Events", "Actions", "The checker"):
            self.assertIn(topic, contents)
            self.assertIn(topic, mcp.format_guide(text, topic).split("\n")[0])
        self.assertLess(len(contents), len(text) / 5)


class Rereads(unittest.TestCase):
    def test_changes_are_addressed_by_id(self):
        after = json.loads(json.dumps(SCREEN))
        after["root"]["children"][0]["children"][0]["text"] = "Board"
        self.assertEqual(mcp.changes_since(SCREEN, after), [{"op": "replace", "path": "#logo/text", "value": "Board"}])


if __name__ == "__main__":
    unittest.main()
