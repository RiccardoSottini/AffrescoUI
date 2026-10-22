#!/usr/bin/env python3
"""Unit tests for the MCP server's queries, which need no host: the outline (and its depth), one node (and its depth),
find_nodes, the format guide by topic, the changes a re-read returns, a proposal's evidence, composing features from other apps (with a fake affrescoui), and messages it can't use. ctest "mcp", or:

    python3 mcp/test_affrescoui_mcp.py
"""

import contextlib
import importlib.util
import io
import json
import os
import socket
import sys
import tempfile
import unittest
import unittest.mock

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


class Entry(unittest.TestCase):
    def test_the_file_clients_run_offers_the_package(self):
        # tests/perf/tokens.py and benchmarks/agents/run.py load affrescoui_mcp.py by path, outside sys.path
        spec = importlib.util.spec_from_file_location("affrescoui_mcp_entry", os.path.join(HERE, "affrescoui_mcp.py"))
        entry = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(entry)
        for name in mcp.__all__:
            self.assertIs(getattr(entry, name), getattr(mcp, name), name)


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
        for title, part in mcp.guide_sections(text):
            self.assertLessEqual(len(part), mcp.GUIDE_TOPIC, title)

    def test_a_long_topic_names_its_subsections(self):
        guide = "# Format\n\n## Nodes\nAbout nodes.\n\n### label\n" + "x" * mcp.GUIDE_TOPIC + "\n\n### image\nPictures."
        self.assertEqual(mcp.format_guide(guide, "nodes"), "## Nodes\nAbout nodes.\n\nMore in the topics: label, image.")
        self.assertEqual(mcp.format_guide(guide, "image"), "### image\nPictures.")


class Scenarios(unittest.TestCase):
    def test_a_proposals_patch_applies_to_a_copy(self):
        document = {"root": {"children": [{"id": "a"}, {"id": "b"}]}, "colors": {"accent": "#111111"}}
        patched = mcp.apply_patch(document, [
            {"op": "replace", "path": "/colors/accent", "value": "#222222"},
            {"op": "add", "path": "/root/children/1", "value": {"id": "new"}},
            {"op": "add", "path": "/root/children/-", "value": {"id": "last"}},
            {"op": "remove", "path": "/root/children/0"},
            {"op": "move", "from": "/colors/accent", "path": "/colors/main"},
            {"op": "copy", "from": "/colors/main", "path": "/colors/a~1b"},
            {"op": "test", "path": "/colors/main", "value": "#222222"}])
        self.assertEqual(patched, {"root": {"children": [{"id": "new"}, {"id": "b"}, {"id": "last"}]},
                                   "colors": {"main": "#222222", "a/b": "#222222"}})
        self.assertEqual(document["colors"], {"accent": "#111111"})  # the original is untouched
        with self.assertRaises(ValueError):
            mcp.apply_patch(document, [{"op": "test", "path": "/colors/accent", "value": "#000000"}])

    def test_the_report_is_a_few_lines(self):
        report = {"file": "kanban.scenarios.json", "passed": 6, "failed": 1, "failures": [
            {"scenario": "add-a-card", "step": 5, "problem": "state columns[0].cards.length is 4, expected 5"},
            {"scenario": "broken", "step": 0, "problem": "could not start: no such file"}]}
        self.assertEqual(mcp.scenarios_report(report, "proposal 3"), "\n".join([
            "kanban.scenarios.json on proposal 3: 6 passed, 1 failed",
            "FAILED add-a-card, step 5: state columns[0].cards.length is 4, expected 5",
            "FAILED broken: could not start: no such file"]))

    def test_only_a_scenarios_file_is_read(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        with tempfile.TemporaryDirectory() as folder:
            other = os.path.join(folder, "settings.json")  # any other file the agent names, JSON or not
            with open(other, "w", encoding="utf-8") as file:
                json.dump({"token": "not for the agent"}, file)
            self.assertEqual(server.run_scenarios({"file": other}),
                             ("error: file: expected a *.scenarios.json file", True))


class Evidence(unittest.TestCase):
    """get_evidence shows what the host gathered, and nothing it didn't."""

    class FakeHost:
        def __init__(self, reply):
            self.reply, self.asked = reply, []

        def request(self, message):
            self.asked.append(message)
            return self.reply

    def test_both_pictures_then_the_facts(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        picture = {"format": "png", "data": "iVBORw==", "width": 4, "height": 3}
        server.host = self.FakeHost({"type": "evidence", "proposal": 2, "revision": 7, "scale": 1, "evidence": {
            "before": picture, "after": picture, "region": [1, 1, 2, 1], "behaviour": ["adds onClick of #title"],
            "scenarios": [{"name": "opens", "passed": False, "message": "step 1: no #title"}],
            "texts": [{"node": "#title", "original": "A", "modified": "B", "language": "text"}]}})
        blocks, failed = server.call_tool("get_evidence", {"proposal": 2, "scale": 2})
        self.assertFalse(failed)
        self.assertEqual(server.host.asked, [{"type": "getEvidence", "proposal": 2, "scale": 2}])
        self.assertEqual([block["type"] for block in blocks], ["text", "image", "text", "image", "text"])
        self.assertEqual(blocks[0]["text"], "before: 4x3 pixels")
        self.assertEqual(blocks[1], {"type": "image", "data": "iVBORw==", "mimeType": "image/png"})
        facts = json.loads(blocks[4]["text"])
        self.assertEqual(facts["region"], [1, 1, 2, 1])
        self.assertEqual(facts["behaviour"], ["adds onClick of #title"])
        self.assertFalse(facts["scenarios"][0]["passed"])
        self.assertEqual(facts["texts"][0]["modified"], "B")

    def test_an_error_is_an_error(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        server.host = self.FakeHost({"type": "rejected", "error": "no proposal 9"})
        self.assertEqual(server.call_tool("get_evidence", {"proposal": 9}), ("error: no proposal 9", True))


class Composition(unittest.TestCase):
    """check_composition and install_feature return the proof's reasons, never the document, and install only what a
    passed proof made."""

    FAKE_CLI = """#!/usr/bin/env python3
import json, sys
args = sys.argv[1:]
passed = "--adapter" in args
print("slideSorter 1.0.0 into app:tempera: " + ("proven" if passed else "not proven"))
if not passed:
    print("unmet need pages: the host exports no 'pages', and no adapter binds it")
    sys.exit(1)
with open(args[args.index("--revision") + 1], "w") as file:
    json.dump({"author": "features", "message": "Install slideSorter", "set": {"/features": {"slideSorter": {}}}}, file)
"""

    def server_with(self, folder):
        cli = os.path.join(folder, "affrescoui")
        with open(cli, "w", encoding="utf-8") as file:
            file.write(self.FAKE_CLI)
        os.chmod(cli, 0o755)
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        server.host = Evidence.FakeHost({"type": "proposed", "proposal": 3, "commit": "abcdef0123",
                                         "document": {"root": {}}, "head": {"id": "abcdef0123"}})
        return server, cli

    @unittest.skipIf(os.name == "nt", "the fake tool is a script")
    def test_a_failed_proof_installs_nothing_and_says_why(self):
        with tempfile.TemporaryDirectory() as folder:
            server, cli = self.server_with(folder)
            with unittest.mock.patch.dict(os.environ, {"AFFRESCOUI_CLI": cli}):
                text, failed = server.call_tool("install_feature", {"screen": "a.screen.json", "feature": "f"})
        self.assertTrue(failed)
        self.assertIn("unmet need pages", text)
        self.assertEqual([message["type"] for message in server.host.asked], ["getDocument"])

    @unittest.skipIf(os.name == "nt", "the fake tool is a script")
    def test_a_proven_install_is_one_revision_by_this_connection(self):
        with tempfile.TemporaryDirectory() as folder:
            server, cli = self.server_with(folder)
            with unittest.mock.patch.dict(os.environ, {"AFFRESCOUI_CLI": cli}):
                text, failed = server.call_tool("install_feature", {"screen": "a.screen.json", "feature": "f",
                                                                    "adapter": {"bind": {}}})
        self.assertFalse(failed)
        self.assertTrue(text.startswith("slideSorter 1.0.0 into app:tempera: proven\nproposed as proposal 3"), text)
        submitted = server.host.asked[1]
        self.assertEqual(submitted["type"], "revision")
        self.assertNotIn("author", submitted)  # the host names it
        self.assertNotIn("root", text)

    def test_only_a_screen_file_is_a_host(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        self.assertEqual(server.call_tool("check_composition", {"screen": "settings.json", "feature": "f"}),
                         ("error: screen: expected the running app's *.screen.json file", True))


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

    def test_tool_names_that_name_no_tool(self):
        server, _ = self.server()
        for name in ({"x": 1}, ["get_catalog"], None, 7, "no_such_tool"):
            with self.assertRaises(mcp.UnknownTool):
                server.call_tool(name, {})

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
