"""What the server offers an MCP client: its tools (names, descriptions and argument schemas), its prompt and its
instructions. Clients and the website's reference read these as they are, so they change only on purpose."""


def object_schema(properties=None, required=None):
    schema = {"type": "object", "properties": properties or {}}
    if required:
        schema["required"] = required
    return schema


# The notation a tool reads or writes documents in: JSON, or the Lisp notation, which spells the same tree (the host
# converts; the history stays JSON)
NOTATION = {"type": "string", "enum": ["json", "lisp"],
            "description": "json (the default) or lisp: the same document, written as S-expressions"}


TOOLS = [
    {"name": "wait_for_request",
     "description": "Wait for the next change request a person sends from inside the app (its chat) or from "
                    "a client on studio.sock, and return it (request_id, from, text, origin: \"app\" or \"studio\", and options when the "
                    "app's chat sent some, e.g. {\"model\": \"sonnet\"}: the model the person chose). "
                    "Returns right away if some are already waiting; returns {\"request\": null} after the timeout. "
                    "Not needed when requests arrive as <channel> events.",
     "inputSchema": object_schema({"timeout_seconds": {"type": "integer", "description": "Default 120, at most 600"}})},
    {"name": "reply",
     "description": "Answer a request where it was asked (the app's chat or the client): say what you changed, or "
                    "ask a question. Call it once you have submitted the revisions for a request; done=false sends "
                    "a progress note and keeps the request open.",
     "inputSchema": object_schema({"request_id": {"type": "string"}, "text": {"type": "string"},
                                   "done": {"type": "boolean", "description": "false while still working; default "
                                                                              "true"}},
                                  ["request_id", "text"])},
    {"name": "answer_repair",
     "description": "Answer a repair request (a wait_for_request result with kind \"repair\": a change set that no "
                    "longer applies, or whose scenarios fail, on the new template). Give the change set's request "
                    "redone on the new base as one JSON Patch; it always becomes a proposal for a reviewer, and the "
                    "scenarios in repair.proof must pass on it.",
     "inputSchema": object_schema({"request_id": {"type": "string"}, "patch": {"type": "array"},
                                   "message": {"type": "string"}}, ["request_id", "patch", "message"])},
    {"name": "get_screen",
     "description": "The running screen: its document (AffrescoUI Screen Format, with the live state as its \"state\" "
                    "and its permission rules), current revision and head commit, open proposals, the mode, the "
                    "reviewer's \"don't ask again\" grants, and your author name. The first call returns everything; "
                    "later calls return only what changed since your last read, as a JSON Patch (\"changes\"), or "
                    "\"unchanged\". Pass full=true to get everything again (e.g. after your context was cleared), "
                    "at=\"#id\" for one node only, or outline=true for the node tree: one line per node, its "
                    "address (every node has an id: \"#label-blocked\"), type, text, bind, style, onClick, repeat and "
                    "when, without the styling. depth limits both to that many levels below (outline lines then say "
                    "\"+12\" for the nodes inside; a node's deeper children become their addresses). Read the outline "
                    "first, then the nodes you change with at. notation=\"lisp\" writes the document or the node "
                    "in the Lisp notation instead of JSON: the same tree as S-expressions, e.g. (label #title "
                    ":text Hello) for {\"type\": \"label\", \"id\": \"title\", \"text\": \"Hello\"} "
                    "(get_format_guide topic \"Lisp notation\"); what changed since a read stays a JSON Patch.",
     "inputSchema": object_schema({"full": {"type": "boolean", "description": "Everything, not what changed"},
                                   "notation": NOTATION,
                                   "at": {"type": "string", "description": "Only this node: \"#id\" or a JSON Pointer"},
                                   "outline": {"type": "boolean",
                                               "description": "The node tree: one line per node (address, type, text, "
                                                              "bind, style, onClick, repeat, when)"},
                                   "depth": {"type": "integer",
                                             "description": "With at or outline: levels below to include (0: the "
                                                            "node itself)"}})},
    {"name": "find_nodes",
     "description": "Search the screen's nodes (root, screens, templates, components) and get one line per match, as "
                    "in the outline, with its parent: by type (\"label\", or a component's name), by text (in the "
                    "text, placeholder or tooltip, any case), by a property it has (property, optionally with value). "
                    "Criteria combine. Cheaper than reading the whole screen to find where to change something.",
     "inputSchema": object_schema({"type": {"type": "string", "description": "A node type or component name"},
                                   "text": {"type": "string", "description": "Text the node shows (part of it)"},
                                   "property": {"type": "string", "description": "A property the node has"},
                                   "value": {"description": "The property's value (optional)"},
                                   "limit": {"type": "integer", "description": "Most lines (default 20)"}}),
     "annotations": {"readOnlyHint": True}},
    {"name": "get_catalog",
     "description": "What this screen's format offers: node types and their properties, events, steps, expression "
                    "functions, variables, sections, and the app's registered custom types and native functions.",
     "inputSchema": object_schema()},
    {"name": "get_models",
     "description": "The data layer (get_format_guide topic \"Data: models and records\"): each collection's model "
                    "(its version and typed fields) and how many records it holds, and the declared queries. Records "
                    "live in a store apart from the screen and are never sent; a model change is a revision of "
                    "/models, with a migration when records must change (rename, convert, fill, split, merge, drop).",
     "inputSchema": object_schema(),
     "annotations": {"readOnlyHint": True}},
    {"name": "get_contributions",
     "description": "What an app offers to features from other apps, and what it can install: its contribution "
                    "points (slot, the exports each receives), its typed exports (state, lists, actions, models, "
                    "surfaces, images) and the installable parts it ships (each one's needs and provides), one line "
                    "each. file is an app's screen file: another app's (e.g. "
                    "\"examples/sinopia/json/index.screen.json\"), to pick a part to install from it, or the running "
                    "app's with running=true, to read the running document's.",
     "inputSchema": object_schema({"file": {"type": "string", "description": "An app's *.screen.json"},
                                   "running": {"type": "boolean",
                                               "description": "The running document, with file's folder"}},
                                  ["file"]),
     "annotations": {"readOnlyHint": True}},
    {"name": "check_composition",
     "description": "Prove that a feature from another app fits the running screen: binds its needs to the "
                    "screen's exports (by name, or through your adapter), checks kinds, slots, ids and imports, "
                    "refuses widened permissions and capabilities, checks the composed document, then plays the "
                    "feature's and the app's scenarios on it. Returns the proof's verdict and reasons: each unmet need "
                    "and why, each failed check and scenario. An adapter is {\"format\": \"affrescoui-adapter\", "
                    "\"version\": 1, \"feature\", \"host\", \"bind\": {need: {\"export\": name} | {\"value\": "
                    "\"{{expression over app.<export>}}\"} | {\"action\": name, \"with\": {...}} | {\"point\": name}}} "
                    "(SCREEN_FORMAT.md, \"Adapters\").",
     "inputSchema": object_schema({
         "screen": {"type": "string", "description": "The running app's screen file, for its folder and scenarios"},
         "feature": {"type": "string", "description": "The feature's folder or feature.json, in its own app"},
         "adapter": {"type": "object", "description": "The adapter (optional: needs bind by name without one)"},
     }, ["screen", "feature"]),
     "annotations": {"readOnlyHint": True}},
    {"name": "install_feature",
     "description": "Install a feature from another app once check_composition proves it: the same proof, then one "
                    "revision that adds the feature's record (its bindings and proof) to the screen's features. It "
                    "is reviewed like any of your revisions (applied, proposed or rejected), shows in history and "
                    "blame, and undoes like one. A failed proof installs nothing and returns its reasons.",
     "inputSchema": object_schema({
         "screen": {"type": "string", "description": "The running app's screen file, for its folder and scenarios"},
         "feature": {"type": "string", "description": "The feature's folder or feature.json, in its own app"},
         "adapter": {"type": "object", "description": "The adapter (optional)"},
     }, ["screen", "feature"])},
    {"name": "get_format_guide",
     "description": "The AffrescoUI Screen Format specification, a part at a time: without topic, its introduction "
                    "and the list of topics (nodes, layout, values and expressions, events, actions, revisions, ...); "
                    "with topic, that section (a heading or a word of one, e.g. \"Revisions\", \"label\", "
                    "\"Events\"); topic \"all\" gives the whole guide.",
     "inputSchema": object_schema({"topic": {"type": "string",
                                             "description": "A section's heading, a word of it, or \"all\""}})},
    {"name": "submit_revision",
     "description": "Change the running screen (layout, styles, colors, behaviour/actions, or state), like a git "
                    "commit, with compact edits: set (path -> value: replaces, or adds where nothing is), add (path -> "
                    "value: inserts; \"/-\" appends to a list), remove (paths), move (from -> to); or a JSON Patch "
                    "(RFC 6902) in patch. Paths address nodes by id: \"#statusBar/color\", \"#card/children/-\", "
                    "\"#oldBadge\" (the node itself); \"/state/...\" changes live data. Example: {\"message\": \"Dark "
                    "status bar\", \"set\": {\"#statusBar/color\": \"@topBar\"}}. The result is: applied (a commit on "
                    "main); proposed (a branch a reviewer previews and merges or rejects, e.g. on its card in the app's chat: get_events "
                    "reports the decision); or rejected with an error saying why (fix it and retry). To refine an "
                    "open proposal of yours (e.g. after the person asks for more), pass its number as `proposal`: the "
                    "commit is added to that branch instead of opening a new one. To bring content in (a picture, a "
                    "sound, a text), add `assets`: each {name, type, text} (SVG, JSON, text, or a sound description: "
                    "type application/vnd.affrescoui.sound+json, name ending .sound.json), {name, type, data} (base64), "
                    "or {name, attachment} (a file the person attached: its id from the request's attachments); the "
                    "screen then reads it as \"asset:<name>\" (an image's or a sound's file, the image step's open "
                    "with {\"asset\": name}). `run`: steps run once on the live screen when the proposal is accepted "
                    "(e.g. [{\"image\": \"open\", \"with\": {\"asset\": \"moon.svg\"}}]). A revision with assets or "
                    "run is always proposed, and only the person accepts it (get_format_guide topic \"Assets\"). "
                    "With notation=\"lisp\", write the change in `revision` as Lisp text instead: an object with "
                    "the same keys, e.g. {:set {:\"#statusBar/color\" @topBar} :add {:\"#list/children/-\" (label "
                    "#hello :text Hello)}}; it becomes the same JSON before it is committed.",
     "inputSchema": object_schema({
         "message": {"type": "string", "description": "One line, for the reviewer, saying what the change does"},
         "notation": NOTATION,
         "revision": {"type": "string",
                      "description": "With notation \"lisp\": the change as Lisp text, an object with any of set, "
                                     "add, remove, move, patch, parent, proposal, assets and run"},
         "set": {"type": "object", "description": "path -> new value (replace, or add where nothing is)"},
         "add": {"type": "object", "description": "path -> value to insert (\"/-\" appends to a list)"},
         "remove": {"type": "array", "items": {"type": "string"}, "description": "Paths to remove"},
         "move": {"type": "object", "description": "from path -> to path"},
         "patch": {"type": "array", "items": {"type": "object"}, "description": "JSON Patch operations (optional "
                                                                             "when using set/add/remove/move)"},
         "parent": {"type": ["integer", "string"],
                    "description": "The revision or commit id this is based on (optional; if the screen has moved "
                                   "on, it is rejected as a conflict)"},
         "proposal": {"type": "integer", "description": "Push onto this open proposal of yours (optional)"},
         "assets": {"type": "array", "items": {"type": "object"},
                    "description": "Files it brings: {name, type, text}, {name, type, data (base64)} or {name, "
                                   "attachment} (optional; names like \"moon-sea.svg\", lowercase, no folders)"},
         "run": {"type": "array", "items": {"type": "object"},
                 "description": "Steps run once when it is accepted, like an action's (optional)"},
     }, ["message"])},
    {"name": "render_screen",
     "description": "A picture of the running screen (PNG), to check your work before a person reviews it: main as "
                    "it is now, or with an open proposal applied (`proposal`, e.g. the one submit_revision just "
                    "opened). Drawn offscreen: the person's window doesn't change. `scale` 2 draws at twice the "
                    "size, like a Retina display.",
     "inputSchema": object_schema({"proposal": {"type": "integer",
                                                "description": "An open proposal to show applied (optional)"},
                                   "scale": {"type": "integer", "description": "1 (default) or 2"}}),
     "annotations": {"readOnlyHint": True}},
    {"name": "get_evidence",
     "description": "The evidence a reviewer sees for an open proposal, gathered by the host itself (an author can't "
                    "supply it): pictures of main and of the proposal applied, the region of pixels that differ "
                    "({x, y, width, height}), lines saying what it changes in behaviour, the screen's scenarios played "
                    "on it (name, passed, message; those that need a window aren't played), and the texts it changes "
                    "(node, original, modified). Check it before asking for a review.",
     "inputSchema": object_schema({"proposal": {"type": "integer", "description": "The open proposal"},
                                   "scale": {"type": "integer", "description": "1 (default) or 2"}}, ["proposal"]),
     "annotations": {"readOnlyHint": True}},
    {"name": "run_scenarios",
     "description": "Check your work with the screen's own scenarios (scripted clicks, typing, waits and expectations "
                    "of state and nodes, played headless): on main as it runs now, or with an open proposal applied "
                    "(`proposal`, e.g. the one submit_revision just opened). `file` is the scenarios file shipped "
                    "next to the screen (examples/flow/json/index.scenarios.json). Pass `scenarios` (a list of "
                    "{name, steps}) to play your own instead of the file's, `only` for one by name. Answers in a few "
                    "lines: the totals, then \"FAILED <scenario>, step <n>: <what>\" per failure. Nothing changes: "
                    "scenarios play on a copy, without a window. SCREEN_FORMAT.md, \"Scenarios\", lists the steps.",
     "inputSchema": object_schema({
         "file": {"type": "string", "description": "A *.scenarios.json file: absolute, or relative to the repository"},
         "proposal": {"type": "integer", "description": "An open proposal to check, applied to main (optional)"},
         "only": {"type": "string", "description": "Play only the scenario of this name (optional)"},
         "scenarios": {"type": "array", "items": {"type": "object"},
                       "description": "Scenarios to play instead of the file's, on its screen (optional)"},
     }, ["file"]),
     "annotations": {"readOnlyHint": True}},
    {"name": "get_history",
     "description": "main's commits, oldest first: number, short id, author, committer (who approved "
                    "it), message, time, and parents (two for a merged proposal). Revision 0 is the file as loaded.",
     "inputSchema": object_schema({"limit": {"type": "integer", "description": "Only the newest N (optional)"}})},
    {"name": "get_diff",
     "description": "What changed, as a JSON Patch: between two revisions (`from`, `to`: numbers, "
                    "commit ids, \"HEAD\", \"HEAD~1\"; default HEAD~1 to HEAD), or what accepting an open proposal "
                    "would change now (`proposal`), then its impact per layer: view, logic, model (each change "
                    "compatible, transforming or destructive) and data (\"none\", \"migrates N records\", ...). With "
                    "`files`, the change file by file instead: each file with its lines added and removed, and the "
                    "before and after text of `file`, in `notation` (\"json\" or \"lisp\").",
     "inputSchema": object_schema({"from": {"type": ["integer", "string"]}, "to": {"type": ["integer", "string"]},
                                   "proposal": {"type": "integer"}, "files": {"type": "boolean"},
                                   "file": {"type": "string"},
                                   "notation": {"type": "string", "enum": ["json", "lisp"]}})},
    {"name": "get_blame",
     "description": "Who last changed a node or a value, and when: the commit on main that last "
                    "changed `target`, a node (\"#board\", or \"#board/text\" inside it) or a path "
                    "(\"/colors/accent\", \"/state/cards\"). Gives its revision, id, author, committer (who applied "
                    "or approved it), message and time; revision 0 if it is as the screen file has it.",
     "inputSchema": object_schema({"target": {"type": "string"}}, ["target"])},
    {"name": "restore_revision",
     "description": "Bring the whole screen back to how it was at an earlier revision. This "
                    "creates a new revision (the history only grows) and follows the same permissions as any change.",
     "inputSchema": object_schema({"revision": {"type": ["integer", "string"]}}, ["revision"])},
    {"name": "revert_revision",
     "description": "Undo one earlier revision and keep everything after it: a new revision with "
                    "the inverse change. Refused as a conflict if later revisions changed the same parts. Follows "
                    "the same permissions as any change.",
     "inputSchema": object_schema({"revision": {"type": ["integer", "string"]}}, ["revision"])},
    {"name": "get_events",
     "description": "What happened since the last call: reviewers' decisions on your proposals (merged or rejected, "
                    "with their reason), commits by anyone, mode and grant changes, events the screen emitted, "
                    "errors. Returns them and clears the list.",
     "inputSchema": object_schema()},
    {"name": "export_changes",
     "description": "The revisions after `from` as a change set: a portable \"plugin\" file another copy of the "
                    "same screen can import (affrescoui_launcher --plugin).",
     "inputSchema": object_schema({"from": {"type": ["integer", "string"]}, "name": {"type": "string"}})},
]

# Several agents at once (VERSION_CONTROL.md, "Leases"): claim a part of the screen while you work on it, so other
# authors' revisions that touch it are refused; it ends after its seconds, when released, or when you disconnect.
COORDINATION_TOOLS = [
    {"name": "lease_part",
     "description": "Claim a part of the screen (\"#id\" or a JSON Pointer such as /state/columns/0) while you work "
                    "on it: other authors' revisions that touch it are refused until it ends (after `seconds`, 300 "
                    "by default, at most 3600; when you release it; or when you disconnect). Refused if someone "
                    "else holds it or a part inside it, or if your permissions don't let you change it.",
     "inputSchema": object_schema({"target": {"type": "string"}, "seconds": {"type": "integer"}}, ["target"])},
    {"name": "renew_lease",
     "description": "Make a lease you hold last `seconds` (300 by default, at most 3600) from now.",
     "inputSchema": object_schema({"target": {"type": "string"}, "seconds": {"type": "integer"}}, ["target"])},
    {"name": "release_part",
     "description": "End a lease you hold, as soon as you are done with that part.",
     "inputSchema": object_schema({"target": {"type": "string"}}, ["target"])},
    {"name": "get_usage",
     "description": "The screen's usage summaries (VERSION_CONTROL.md, \"Usage\"): per node id and action, how often "
                    "it ran and failed; the screens visited; the controls never used, over the last 30 days. Only "
                    "ids, counts and times. Refused unless the host switched them on with the person's consent and "
                    "your connection may review.",
     "inputSchema": object_schema(),
     "annotations": {"readOnlyHint": True}},
]
TOOLS += COORDINATION_TOOLS


PROMPTS = [
    {"name": "serve_studio",
     "description": "Work through the change requests people send from the AffrescoUI app, one after another."},
]

SERVE_PROMPT = (
    "You are the builder behind a live AffrescoUI app. People ask for changes in the app's chat (or from a "
    "client on studio.sock). Repeat: call "
    "wait_for_request; for each request, find what to change (get_screen with outline=true, or find_nodes; every "
    "node has an id), read just those parts (get_screen at=\"#id\"; after a first full read get_screen returns only "
    "what changed), and look up the format's parts you need (get_format_guide lists its topics), make "
    "the change with submit_revision (fix and retry if it is rejected), then tell the person what you did with "
    "reply. Keep changes focused on what was asked, and keep the existing style (colors, fonts, spacing). If a "
    "follow-up request refines a change that is still waiting for review, push to that proposal instead of opening "
    "a new one.")

INSTRUCTIONS = (
    "This server connects you to a running AffrescoUI app (a native C++/OpenGL UI described by a JSON screen document). "
    "People ask for changes from inside the app (origin \"app\": its chat; your proposals there still wait for the "
    "person's review), or from a client on studio.sock (origin \"studio\"). Their requests arrive as <channel "
    "source=\"affrescoui\" request_id=\"...\" sender=\"...\" origin=\"...\"> events (if this session loaded the "
    "channel), or from the wait_for_request tool. For each request: find what to change with get_screen (outline=true, then "
    "at=\"#id\": every node has an id) or find_nodes (get_format_guide explains the format, one topic at a time), "
    "change it with submit_revision, then answer with reply, passing the request_id. Your changes may "
    "wait for a reviewer's approval (the person accepts or rejects them on their cards in the app's chat); get_events tells you the decision.")
