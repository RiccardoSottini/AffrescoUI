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
import threading
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
        report = {"file": "index.scenarios.json", "passed": 6, "failed": 1, "failures": [
            {"scenario": "add-a-card", "step": 5, "problem": "state columns[0].cards.length is 4, expected 5"},
            {"scenario": "broken", "step": 0, "problem": "could not start: no such file"}]}
        self.assertEqual(mcp.scenarios_report(report, "proposal 3"), "\n".join([
            "index.scenarios.json on proposal 3: 6 passed, 1 failed",
            "FAILED add-a-card, step 5: state columns[0].cards.length is 4, expected 5",
            "FAILED broken: could not start: no such file"]))

    def test_only_a_scenarios_file_is_read(self):
        with tempfile.TemporaryDirectory() as folder:
            server = mcp.Server(os.path.join(HERE, "no-such-folder"), None, [folder])
            other = os.path.join(folder, "settings.json")  # any other file the agent names, JSON or not
            with open(other, "w", encoding="utf-8") as file:
                json.dump({"token": "not for the agent"}, file)
            self.assertEqual(server.run_scenarios({"file": other}),
                             ("error: file: expected a *.scenarios.json file", True))

    def test_only_files_within_reach_are_read(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        with tempfile.TemporaryDirectory() as folder:
            outside = os.path.join(os.path.realpath(folder), "x.scenarios.json")  # outside every root
            with open(outside, "w", encoding="utf-8") as file:
                json.dump({"token": "not for the agent"}, file)
            refused = (mcp.OUTSIDE.format("file"), True)
            self.assertEqual(server.run_scenarios({"file": outside}), refused)
            self.assertEqual(server.get_contributions({"file": outside.replace("scenarios", "screen")}), refused)
            self.assertEqual(server.compose({"screen": outside, "feature": "f"}, False),
                             (mcp.OUTSIDE.format("screen"), True))
            if hasattr(os, "symlink"):  # a link inside the repository leads out of it: refused, too
                with tempfile.TemporaryDirectory(dir=mcp.ROOT) as inside:
                    link = os.path.join(inside, "y.scenarios.json")
                    os.symlink(outside, link)
                    self.assertEqual(server.run_scenarios({"file": link}), refused)


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


class Models(unittest.TestCase):
    """The data layer: the models in the outline and from get_models, the impact of a diff; never a record."""

    def test_the_models_follow_the_state_types(self):
        modelled = {**SCREEN, "models": {"notes": {"version": 2, "fields": {
            "title": {"type": "string", "required": True}, "done": {"type": "boolean"}}}}}
        self.assertEqual(mcp.outline(modelled, depth=0).split("\n")[0],
                         "models: notes v2: title: string (required), done: boolean")

    def test_get_models_gives_the_outline_of_the_store(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        data = {"collections": {"notes": {"version": 2, "count": 12, "fields": {"title": {"type": "string"}}}},
                "queries": ["open"]}
        server.host = Evidence.FakeHost({"type": "catalog", "catalog": {"steps": {}, "data": data}})
        text, failed = server.call_tool("get_models", {})
        self.assertFalse(failed)
        self.assertEqual(server.host.asked, [{"type": "getCatalog"}])
        self.assertEqual(json.loads(text if isinstance(text, str) else text[0]["text"]), data)

    def test_a_diff_says_its_impact(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        impact = {"view": 1, "logic": 0, "model": [], "data": "none"}
        server.host = Evidence.FakeHost({"type": "diff", "patch": [], "impact": impact})
        text, failed = server.call_tool("get_diff", {})
        self.assertFalse(failed)
        said = text if isinstance(text, str) else text[0]["text"]
        self.assertIn('impact: {"view":1,"logic":0,"model":[],"data":"none"}', said)


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
        host = mcp.Host(os.path.join(HERE, "no-such-folder"), [os.path.join(HERE, "no-such-program")], None)
        with self.assertRaisesRegex(RuntimeError, "couldn't start"):
            host.ensure()

    @unittest.skipIf(os.name == "nt", "Windows keeps the folder private by its ACL")
    def test_a_socket_folder_others_may_enter_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            os.chmod(folder, 0o700)
            mcp.check_socket_dir(folder)
            os.chmod(folder, 0o777)
            with self.assertRaisesRegex(RuntimeError, "open to other users"):
                mcp.check_socket_dir(folder)

    def test_the_launch_program_and_its_arguments_are_never_split(self):
        server_module = sys.modules[mcp.main.__module__]
        argv = ["affrescoui_mcp.py", "--socket", "s", "--launch", "/opt/my host", "--launch-arg", "a b; touch x"]
        with unittest.mock.patch.object(sys, "argv", argv), \
                unittest.mock.patch.object(server_module, "Server") as server, \
                unittest.mock.patch.object(sys, "stdin", io.TextIOWrapper(io.BytesIO())), \
                unittest.mock.patch.object(sys, "stdout", io.TextIOWrapper(io.BytesIO())), \
                contextlib.redirect_stderr(io.StringIO()):
            mcp.main()
        server.assert_called_once_with("s", ["/opt/my host", "a b; touch x"], [])
        with unittest.mock.patch.object(sys, "argv", ["affrescoui_mcp.py", "--launch-arg", "x"]), \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            mcp.main()  # an argument needs a program first

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



class Requests(unittest.TestCase):
    def test_requests_without_an_id_keep_apart(self):
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        with contextlib.redirect_stderr(io.StringIO()):
            server.on_request({"type": "request", "text": "first"})
            server.on_request({"type": "request", "text": "second"})
            server.forget(server.wait_for_request(1)["request_id"])  # the first, answered
            server.on_request({"type": "request", "text": "third"})
        self.assertEqual([request["text"] for request in server.requests.values()], ["second", "third"])
        self.assertEqual(list(server.requests), ["r2", "r3"])

    class SendingHost:
        """A host that keeps what the server sends it."""

        def __init__(self):
            self.sent = []

        def send(self, message):
            self.sent.append(message)

    def serving(self):
        """A server whose host keeps what it is sent."""
        server = mcp.Server(os.path.join(HERE, "no-such-folder"), None)
        server.host = self.SendingHost()
        return server

    def test_only_the_screen_asks_from_inside_the_app(self):
        server = self.serving()
        with contextlib.redirect_stderr(io.StringIO()):
            server.on_request({"type": "request", "id": "ask-1", "from": "user", "text": "a", "source": "screen"})
            server.on_request({"type": "request", "id": "s-1", "from": "studio", "text": "b", "source": "studio"})
            server.on_request({"type": "request", "id": "s-2", "from": "studio", "text": "c"})
            origins = [server.wait_for_request(1)["origin"] for _ in range(3)]
        self.assertEqual(origins, ["app", "studio", "studio"])
        # The app's chat learns the agent has its request; a client on studio.sock shows its own requests' progress
        self.assertEqual(server.host.sent, [{"type": "reply", "request": "ask-1", "done": False}])

    def test_the_channel_says_where_a_request_comes_from(self):
        server = self.serving()
        server.ready = True
        pushed = io.StringIO()
        with contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(pushed):
            server.on_request({"type": "request", "id": "ask-1", "from": "user", "text": "a", "source": "screen"})
            server.wait_for_request(1)  # given out by the channel already: not told again
        meta = json.loads(pushed.getvalue())["params"]["meta"]
        self.assertEqual(meta, {"request_id": "ask-1", "sender": "user", "origin": "app"})
        self.assertEqual(server.host.sent, [{"type": "reply", "request": "ask-1", "done": False}])

    def test_a_reply_not_done_keeps_the_request(self):
        server = self.serving()
        with contextlib.redirect_stderr(io.StringIO()):
            server.on_request({"type": "request", "id": "ask-1", "from": "user", "text": "a", "source": "screen"})
            server.wait_for_request(1)
            server.host.sent.clear()
            note = server.reply({"request_id": "ask-1", "text": "looking", "done": False})
            kept = "ask-1" in server.requests
            answer = server.reply({"request_id": "ask-1", "text": "done"})
        self.assertEqual((note[0], kept, answer[0]), ("sent to the app", True, "sent to the app"))
        self.assertNotIn("ask-1", server.requests)
        self.assertEqual([message["done"] for message in server.host.sent], [False, True])
        self.assertEqual(server.reply({"request_id": "gone", "text": "x"})[0], "sent to the client")

    def test_a_cancelled_request_is_no_longer_given_out_and_the_agent_is_told(self):
        server = self.serving()
        server.ready = True
        pushed = io.StringIO()
        with contextlib.redirect_stderr(io.StringIO()), contextlib.redirect_stdout(pushed):
            server.on_request({"type": "request", "id": "ask-1", "from": "user", "text": "a", "source": "screen"})
            server.on_request({"type": "cancel", "request": "ask-1", "from": "user", "source": "screen"})
            server.on_request({"type": "cancel", "request": 7})  # malformed: ignored
        self.assertNotIn("ask-1", server.requests)
        told = [json.loads(line)["params"] for line in pushed.getvalue().splitlines()]
        self.assertEqual(len(told), 2)  # the request, then its cancel
        self.assertEqual(told[1]["meta"], {"request_id": "ask-1", "sender": "user", "origin": "app",
                                           "cancelled": "true"})


class Events(unittest.TestCase):
    def test_news_nobody_takes_is_bounded(self):
        host = mcp.Host(os.path.join(HERE, "no-such-folder"), None, lambda request: None)
        for n in range(mcp.MAX_EVENTS + 5):
            host.note({"event": "tick", "data": n})
        written = io.StringIO()
        with contextlib.redirect_stderr(written):
            events = host.take_events()
        self.assertEqual([event["data"] for event in events], list(range(5, mcp.MAX_EVENTS + 5)))
        self.assertIn("5 older events were dropped", written.getvalue())
        self.assertEqual(host.take_events(), [])


class Connecting(unittest.TestCase):
    """Calls on several threads at once share one connection to the host."""

    @unittest.skipUnless(hasattr(socket, "AF_UNIX"), "the host is reached through AF_UNIX")
    def test_calls_that_find_no_connection_at_once_make_one(self):
        with tempfile.TemporaryDirectory(dir="/tmp") as folder:  # short: a socket's path has a length limit
            listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            listener.bind(os.path.join(folder, "agent.sock"))
            listener.listen(16)
            host = mcp.Host(folder, None, lambda request: None)
            start = threading.Barrier(8)

            def call():
                start.wait()
                host.ensure()

            with contextlib.redirect_stderr(io.StringIO()):
                callers = [threading.Thread(target=call) for _ in range(8)]
                for caller in callers:
                    caller.start()
                for caller in callers:
                    caller.join()
            listener.settimeout(0.5)
            accepted = []
            with contextlib.suppress(socket.timeout):
                while True:
                    accepted.append(listener.accept()[0])
            for connection in accepted:
                connection.close()
            listener.close()
            self.assertEqual(len(accepted), 1)


if __name__ == "__main__":
    unittest.main()
