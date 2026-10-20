#!/usr/bin/env python3
"""Unit tests for the MCP server's queries, which need no host: the outline (and its depth), one node (and its depth),
find_nodes, the format guide by topic, the changes a re-read returns, and messages it can't use. ctest "mcp", or:

    python3 mcp/test_affrescoui_mcp.py
"""

import contextlib
import io
import json
import os
import socket
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

    def test_the_state_types_come_first(self):
        typed = {**SCREEN, "stateTypes": {
            "filter": {"enum": ["all", "bug"]}, "volume": {"type": "number", "maximum": 100},
            "cards": {"type": "array", "items": {"type": "object", "properties": {
                "title": {"type": "string"}, "done": {"type": "boolean"}}}},
            "note": {"type": ["string", "null"]}}}
        self.assertEqual(mcp.outline(typed, depth=0).split("\n")[0],
                         "state: filter: 'all' | 'bug'; volume: number; cards: list of {title: text, done: boolean}; "
                         "note: text | null")
        self.assertEqual(mcp.outline(typed, at="#row-filters", depth=0), "#row-filters +2")  # only the whole outline

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


class Malformed(unittest.TestCase):
    """A message the server can't use costs that message, never the server (each of these stopped it)."""

    def server(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        sent = []
        server.send = sent.append
        return server, sent

    def test_messages_from_the_client(self):
        server, sent = self.server()
        server.handle([{"jsonrpc": "2.0", "id": 1, "method": "ping"}])  # a batch
        server.handle({"jsonrpc": "2.0", "id": 2, "method": "ping", "params": 5})
        server.handle({"jsonrpc": "2.0", "id": 3, "method": "initialize", "params": {"clientInfo": "x"}})
        server.handle({"jsonrpc": "2.0", "id": 4, "method": "ping"})
        self.assertEqual([(m["id"], m.get("error", {}).get("code")) for m in sent],
                         [(None, -32600), (2, -32600), (3, None), (4, None)])
        self.assertEqual(server.client, "agent")

    def test_lines_python_cant_read(self):
        server, sent = self.server()
        server.serve("[" * 200000 + "]" * 200000)  # deeper than Python's recursion limit
        server.serve('{"jsonrpc": "2.0", "id": 5, "method": "ping"}')
        self.assertEqual([(m["id"], m.get("error", {}).get("code")) for m in sent], [(None, -32700), (5, None)])

    @unittest.skipUnless(hasattr(socket, "AF_UNIX"), "the host is reached through AF_UNIX")
    def test_a_launch_command_that_cant_start(self):
        host = mcp.Host(os.path.join(HERE, "no-such-folder"), os.path.join(HERE, "no-such-program"), None)
        with self.assertRaisesRegex(RuntimeError, "couldn't start"):
            host.ensure()

    def test_the_log_escapes_what_a_request_can_hold(self):
        written = io.StringIO()
        with contextlib.redirect_stderr(written):
            mcp.log("request r1: a\nforged line\x1b[2J \\n \u0085 ok é")
        self.assertEqual(written.getvalue(), "[affrescoui-mcp] request r1: a\\nforged line\\x1b[2J \\\\n \\x85 ok é\n")

    @unittest.skipUnless(hasattr(socket, "AF_UNIX"), "the host is reached through AF_UNIX")
    def test_messages_from_the_host(self):
        host = mcp.Host(os.path.join(HERE, "no-such-folder"), None, lambda request: None)
        ours, theirs = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
        host.sock = ours
        theirs.sendall(b'[]\n"text"\n{"type": "review", "mode": "plan", "grants": [1, {"rule": "r"}]}\n'
                       b'{"type": "event", "name": "\xff"}\n{"type": "error", "id": [], "message": "m"}\n')
        theirs.close()
        host.read(ours)
        self.assertEqual(host.take_events(), [{"mode": "plan", "grants": ["r"]},
                                              {"event": "�", "data": None}, {"error": "m"}])
        self.assertIsNone(host.sock)  # the next call connects again


if __name__ == "__main__":
    unittest.main()
