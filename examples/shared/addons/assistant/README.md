# Assistant

A chat with the agents connected to an app, as one JSON file to `include`. A round launcher at the bottom right opens
a panel where the person asks for a change in plain words; the agents answer, and the proposals they open come back as
cards to preview, accept or reject. It holds no code: it is built from the built-in chat components (`chatBubble`,
`typingIndicator`, `proposalCard`, `chatComposer`, `chatLauncher`), the `ask` step and the `ask:reply` event
([`SCREEN_FORMAT.md`](../../../../SCREEN_FORMAT.md)).

```json
"include": ["../shared/addons/assistant/assistant.json"]
```

The path is relative to the screen file. Everything the add-on adds is named `assistant…`, so it merges into any
screen without meeting the app's own names.

## What the person sees

- **The launcher**, 56 pixels at the bottom right, with a badge for the replies that came while the panel was closed.
  It is always there by default. An app that sets `state.assistant.show` to `false` hides it until an agent connects
  or a conversation is kept. `Ctrl+Shift+A` (`Cmd+Shift+A` on
  macOS) opens and closes the panel from anywhere.
- **The panel**, 380 by up to 560 pixels, which grows in from the launcher. Its header says how many agents are
  connected, or that none is; with none, a note says to set up a provider of intelligence in the AffrescoUI
  Launcher's Settings (the host then runs an agent of its own, `modules/intelligence`, for an app whose `app.json`
  declares the `assistant` capability and that the person allowed it), or to connect an agent over MCP. An empty conversation shows three suggestions
  to ask with a click.
- **The conversation**, as Claude Code and Codex show one: the person's messages in soft bubbles, the replies as plain
  text, each with *Copy* (to the clipboard) and *Retry* (asks the request it answered again), a typing indicator
  while a request is being worked on,
  and a card for every proposal a reply opened, with its first changes ("set /path to value"). Every other proposal
  waiting for review (one an agent opened without a reply, e.g. from a terminal over MCP) has a card too, after the
  conversation, so the app's assistant is where the person reviews every change. A card's buttons show while the
  proposal waits for review; after, it says *Accepted*, *Rejected*, or *Closed* (the proposal went another way, e.g.
  reviewed by another client). On a host that has it, *Review changes* opens the host's own window on the change, file by
  file ([VERSION_CONTROL.md](../../../../VERSION_CONTROL.md), "The changes, file by file").
- **Runs**, as cards: every run of one of the app's commands an agent asked for (the `runs` variable), after the
  conversation, the last 20. A card says what ran (the command's title), for which agent, its first three arguments
  as the agent wrote them (on one line, cut at 60 characters, never evaluated) and how it ended: *Ran* (with *Undo*
  while what it changed can be put back), *Denied* with the reason, or, while it waits, *Run*, *Always allow* (a run
  grant: the agent runs it again without asking) and *Don't run*. A run only the window may allow (a command that
  asks the person each time, or one that reaches a service that writes) has no buttons: the window asks. The
  launcher's badge counts the runs that wait too. The buttons are the person's own steps (`allowRun`, `denyRun`,
  `undoRun`), refused while an agent's reply runs an action, so a reply never decides a run.
- **Files**, as cards with an icon of their kind (a picture, a sound, any other file), their name and their size or
  length: the files attached to the next message inside the composer (each removable), the files sent with a message,
  and a proposal's assets with its run-once steps.
- **The model picker**, in the composer's bar next to *Attach* (`+`): the model the requests use, at first the person's default from the
  Launcher's Settings, and a menu of the provider's models (the default marked). The choice is the app's own (kept
  with `persist`) and goes with every request as the `ask` step's `options` (`{ "model": "opus" }`). It shows only
  when the host's own agent is connected (the peer `agent`) and answers the read-only native `assistant.models`
  (`{ provider, default, models: [{ id, name }] }`, `Intelligence.hpp` in `modules/intelligence`), which reveals nothing
  secret and changes nothing; elsewhere (MCP-only setups, other hosts) it hides.
- **Pointing**: *Point at something* (the arrow after *Attach*, or `Ctrl+Alt+A`, `Cmd+Alt+A` on macOS, from anywhere)
  lets the person show the agent what they mean. The panel steps aside, a hint says to click what they want to change
  (Escape cancels), and the element under the pointer is outlined; the keyboard walks the elements too (Tab, the
  arrows, Enter). The click picks, and doesn't reach the app. The panel comes back with the element as a chip above
  the field (its type and where it is written: a program's definition, or a document's node), up to four, each with a
  remove button and, on a host with a changes window, *Show source*. The chips go with the next message, which shows
  them in the conversation. The record says where the element is written, never what it shows (the `pick` step,
  [SCREEN_FORMAT.md](../../../../SCREEN_FORMAT.md)); a commit since drops it quietly, as where it was may have moved.
- **The composer**, a card with the text above and a bar below: *Attach* (`+`, or drop files on the panel), *Point at
  something* and the model picker at its start, a round send button at its end. While an agent works on a request, the send button is a
  *Stop* button: it cancels the requests in progress (`cancelAsk`), the agents are told, and what they send after is
  dropped. Enter sends, Shift+Enter starts a new line, Cmd+Enter (Ctrl+Enter) sends the draft as a request even when
  it reads as Lisp, Escape closes the panel and gives the focus back to the launcher. The panel is modal: Tab cycles
  inside it.

A chat request is a request, never an approval. The agent's change waits as a proposal, and only the person's
*Accept* on the card (or another reviewing client's) merges it; the reviewer is the person, not the agent.

## The input takes Lisp

In the Lisp version (`assistant.lisp`), the field also evaluates Lisp. A draft counts as Lisp when `describe-form`
says so: it starts with `(`, reads, and every form is a definition or a call of something the program can run, so
`(+ 1 2)` and `(defn greet [] "hi")` are Lisp, `(please make it blue)` and `make the title blue` are requests.
`computed.assistantForm` holds what `describe-form` answered, `nil` for a request.

- **The preview.** While the draft is Lisp, a line over the field says what Enter would do: *Lisp: defines `greet` in
  `evaluated.lisp`* (or *replaces*, for a name the program has), *Lisp: defines 3 definitions*, or *Lisp: runs 1
  form*, with the names and files in the code font. A draft the person didn't type in the field (pasted, or set by a
  step) adds *You will be asked to confirm: this text was not typed here.* An *Ask instead* link sends it as a request.
  The send button says *Evaluate*. With no Lisp there is no line and the panel is as before.
- **Enter** evaluates it (`evaluate`, with `:confirm true`): the draft joins the conversation as a `code` message,
  then its value as a `result` (`=> 3`) and the field empties. An error shows as a `result` in the danger colour and
  keeps the draft, to fix it; a *no* to the window's question is such an error (nothing ran). Definitions land in
  `evaluated.lisp`, or replace the one of the same name where it is.
- **Who the change is by.** The person: a revision by `user`, as a change made in the editor would be. Only what the
  person typed in the field counts without a question; anything else waits for their yes in the window's own
  question, which no peer can answer. An agent's message can never be evaluated from here, and `describe-form` tells
  only the person's code what a text is.
- **The JSON version** (`assistant.json`) is unchanged: its screens have no program, so the input stays requests only.

`state.assistant.codeFont` names the font of `code` and `result` messages and the preview's names: `"default"` until
the app sets it to a monospace font of its own (`:onLoad [{:set state.assistant.codeFont :to mono}]`), since fonts are
the screen's. The strings are the `assistant.lisp.*` keys.

## State and actions

All in `state.assistant`:

| Field | What it is |
|---|---|
| `open` | Whether the panel is open |
| `lift` | Pixels to raise the launcher and the panel by, over a panel of the app's own at the bottom right (`0` by default), e.g. `{ "watch": { "state.findBar": [{ "set": "state.assistant.lift", "to": "{{computed.findHeight}}" }] } }` |
| `show` | Whether the launcher shows when no agent is connected and no conversation is kept (`true` by default) |
| `draft` | The composer's text |
| `codeFont` | The font of evaluated code and its results (`"default"` until the app names a monospace one) |
| `messages` | The conversation: `{ mine, from, text, time, error }` (a reply also has `asked`, the request it answers, for *Retry*; *Stop* adds one with `note: true`), `{ kind: "files", files }` for files sent, `{ kind: "pointed", items }` for the elements pointed at, and `{ kind: "card", id, title, summary, lines, time }` for a proposal, `lines` being its first three `changes`, and, in the Lisp version, `{ kind: "code", mine, text }` for a draft evaluated and `{ kind: "result", text, error }` for its value or error (at most 200, the oldest dropped) |
| `decided` | `proposal id → "accepted" or "rejected"`, as the person decided on the card |
| `unread` | The replies since the panel was last open |
| `pending` | The id of the last request sent |
| `model` | The model the person chose in the picker, or `""` for their default (persisted) |
| `models` | What `assistant.models` answered, or `null` (the picker hides) |
| `modelMenu` | Whether the picker's menu is open |
| `attachments` | The files attached to the next message |
| `pointed` | The elements pointed at for the next message: the `pick` step's records (`id`, `node`, `type`, `source`, `revision`, ...) |
| `pointing` | Whether the person is pointing now |
| `justPointed`, `wasOpen` | The record just picked, and whether the panel was open, while pointing |
| `copied` | The place of the message just copied, for a moment, or `null` |
| `simulate` | For tests only: see below |

`messages`, `decided` and `model` are persisted (`persist`); whether the panel is open is not.

| Action | What it does |
|---|---|
| `assistant.toggle`, `assistant.open`, `assistant.close` | Open and close the panel |
| `assistant.ask` | Sends `text` (with `{ "text": … }`) as a request, as the suggestions do |
| `assistant.start` | Opens the panel and sends `event.text` as a request: what the host's `--ask TEXT` runs once the window is up |
| `assistant.send` | Sends the draft and empties it; in the Lisp version a draft that is Lisp is evaluated instead |
| `assistant.askDraft` | Sends the draft as a request, Lisp or not (*Ask instead*, Cmd+Enter) |
| `assistant.preview`, `assistant.accept`, `assistant.reject` | Review the proposal `id` |
| `assistant.loadModels` | Asks the host for the models (run when the panel shows) |
| `assistant.pickModel` | Uses the model `id` for the next requests (the default is kept as `""`) |
| `assistant.stop` | Cancels the requests in progress and says so in the conversation |
| `assistant.retry` | Asks `text` again (a reply's `asked`) |
| `assistant.copy` | Copies `text` to the clipboard; `at`, its place, shows *Copied* for a moment |
| `assistant.attach`, `assistant.drop`, `assistant.unattach` | Attach files from the OS dialog or a drop, or remove the file `id` |
| `assistant.point` | Lets the person point at an element for the next message (from a click or a key of theirs: a pick starts only from the person's own input) |
| `assistant.unpoint` | Takes the element `id` (a record's) from the next message |
| `assistant.showSource` | Asks the host to show where the element `record` is written (`changes.open`); a host without it does nothing |
| `assistant.clear` | Forgets the conversation |
| `assistant.allowRun`, `assistant.alwaysAllowRun`, `assistant.denyRun` | Run the waiting run `run`, run it and grant its agent that command, or refuse it, from its card |
| `assistant.undoRun` | Puts back what the run `run` changed |

An app can run them from its own menus or buttons, e.g. `{ "run": "assistant.open" }`, or a menu item that points:
`{ "label": "Point at something", "onClick": [{ "run": "assistant.point" }] }`.

## Restyling and strings

The colours are the `assistant.*` tokens, each with a light and a dark value: `accent`, `accentHover`, `onAccent`,
`surface`, `raised`, `border`, `text`, `dim`, `dot`, `success`, `danger`, `warningBackground`. `warningBackground` is
the pale tint behind the note shown when no agent is online, under `text`: map it to a background colour, not
to a bright warning colour. A screen's own `tokens` win over an included file's, so an app restyles the add-on by
redefining them, e.g. `"assistant.accent": "@accent"`.

The strings are the `assistant.*` keys in English, Spanish and French; the panel follows the screen's direction
(`"dir": "auto"`). A screen adds a language, or rewords one, with the same keys in its own `strings`.

## Tests

The rendering check draws the add-on through `tests/render/screens/assistant.screen.json`, whose runner has no agents:
`state.assistant.simulate` stands in for them, with `online`, `pending` (the ids waiting for review), `working`,
`proposals` (as the `proposals` variable lists them) and `models` (as `assistant.models` answers). `runs` stands in for the
`runs` variable (`test.runs` sets four: one ran, one denied, one waiting and one only the window may allow). An app leaves it `null`, and the add-on reads `peers`,
`requests` and `proposals`.

`tests/unit/assistant_addon.cpp` drives the same screen in a headless window: the keys, the focus, the persisted
conversation, a request to an agent connected in the process, whose proposal the person accepts or rejects with
the card's buttons, and pointing (by mouse and by keyboard alone, the chips, the request's `picks`, the limit of four
and *Show source*). `e2e_test.py` runs the whole loop with the stock host and the MCP server as the agent, on
`e2e.screen.json`, where a timer stands in for the person: the offline answer, then a request taken with
`wait_for_request` (`origin: "app"`), the agent's proposal and reply, the card accepted (the change is on main), and a
second card rejected. ctest runs it as `assistant_e2e`.

`tests/unit/assistant_runs.cpp` drives the run cards on both test screens, whose command `test.note` an agent
connected in the process asks to run: it waits on its card under the agent's name, *Run* runs it and *Undo* puts it
back, *Always allow* adds a grant, *Don't run* refuses it, the arguments show as written, and a run only the window
may allow has no buttons.

The Lisp input is drawn through `tests/render/screens/assistant-lisp.screen.lisp` (the preview over a typed
definition, and the conversation after an evaluation), and `tests/unit/assistant_lisp_input.cpp` drives it with real
keys: a typed definition (one revision by `user`), an expression (no revision), words that stay requests, a pasted
and a step-set form (the window's question, *no* then *yes*), Cmd+Enter and *Ask instead*, an error, an empty draft.
