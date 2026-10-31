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
- **Files**, as cards with an icon of their kind (a picture, a sound, any other file), their name and their size or
  length: the files attached to the next message inside the composer (each removable), the files sent with a message,
  and a proposal's assets with its run-once steps.
- **The model picker**, in the composer's bar next to *Attach* (`+`): the model the requests use, at first the person's default from the
  Launcher's Settings, and a menu of the provider's models (the default marked). The choice is the app's own (kept
  with `persist`) and goes with every request as the `ask` step's `options` (`{ "model": "opus" }`). It shows only
  when the host's own agent is connected (the peer `agent`) and answers the read-only native `assistant.models`
  (`{ provider, default, models: [{ id, name }] }`, `Intelligence.hpp` in `modules/intelligence`), which reveals nothing
  secret and changes nothing; elsewhere (MCP-only setups, other hosts) it hides.
- **The composer**, a card with the text above and a bar below: *Attach* (`+`, or drop files on the panel) and the
  model picker at its start, a round send button at its end. While an agent works on a request, the send button is a
  *Stop* button: it cancels the requests in progress (`cancelAsk`), the agents are told, and what they send after is
  dropped. Enter sends, Shift+Enter starts a new line, Escape closes the panel and gives the focus back to the
  launcher. The panel is modal: Tab cycles inside it.

A chat request is a request, never an approval. The agent's change waits as a proposal, and only the person's
*Accept* on the card (or another reviewing client's) merges it; the reviewer is the person, not the agent.

## State and actions

All in `state.assistant`:

| Field | What it is |
|---|---|
| `open` | Whether the panel is open |
| `lift` | Pixels to raise the launcher and the panel by, over a panel of the app's own at the bottom right (`0` by default), e.g. `{ "watch": { "state.findBar": [{ "set": "state.assistant.lift", "to": "{{computed.findHeight}}" }] } }` |
| `show` | Whether the launcher shows when no agent is connected and no conversation is kept (`true` by default) |
| `draft` | The composer's text |
| `messages` | The conversation: `{ mine, from, text, time, error }` (a reply also has `asked`, the request it answers, for *Retry*; *Stop* adds one with `note: true`), `{ kind: "files", files }` for files sent, and `{ kind: "card", id, title, summary, lines, time }` for a proposal, `lines` being its first three `changes` (at most 200, the oldest dropped) |
| `decided` | `proposal id → "accepted" or "rejected"`, as the person decided on the card |
| `unread` | The replies since the panel was last open |
| `pending` | The id of the last request sent |
| `model` | The model the person chose in the picker, or `""` for their default (persisted) |
| `models` | What `assistant.models` answered, or `null` (the picker hides) |
| `modelMenu` | Whether the picker's menu is open |
| `attachments` | The files attached to the next message |
| `copied` | The place of the message just copied, for a moment, or `null` |
| `simulate` | For tests only: see below |

`messages`, `decided` and `model` are persisted (`persist`); whether the panel is open is not.

| Action | What it does |
|---|---|
| `assistant.toggle`, `assistant.open`, `assistant.close` | Open and close the panel |
| `assistant.ask` | Sends `text` (with `{ "text": … }`) as a request, as the suggestions do |
| `assistant.start` | Opens the panel and sends `event.text` as a request: what the host's `--ask TEXT` runs once the window is up |
| `assistant.send` | Sends the draft and empties it |
| `assistant.preview`, `assistant.accept`, `assistant.reject` | Review the proposal `id` |
| `assistant.loadModels` | Asks the host for the models (run when the panel shows) |
| `assistant.pickModel` | Uses the model `id` for the next requests (the default is kept as `""`) |
| `assistant.stop` | Cancels the requests in progress and says so in the conversation |
| `assistant.retry` | Asks `text` again (a reply's `asked`) |
| `assistant.copy` | Copies `text` to the clipboard; `at`, its place, shows *Copied* for a moment |
| `assistant.attach`, `assistant.drop`, `assistant.unattach` | Attach files from the OS dialog or a drop, or remove the file `id` |
| `assistant.clear` | Forgets the conversation |

An app can run them from its own menus or buttons, e.g. `{ "run": "assistant.open" }`.

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
`proposals` (as the `proposals` variable lists them) and `models` (as `assistant.models` answers). An app leaves it `null`, and the add-on reads `peers`,
`requests` and `proposals`.

`tests/unit/assistant_addon.cpp` drives the same screen in a headless window: the keys, the focus, the persisted
conversation, and a request to an agent connected in the process, whose proposal the person accepts or rejects with
the card's buttons. `e2e_test.py` runs the whole loop with the stock host and the MCP server as the agent, on
`e2e.screen.json`, where a timer stands in for the person: the offline answer, then a request taken with
`wait_for_request` (`origin: "app"`), the agent's proposal and reply, the card accepted (the change is on main), and a
second card rejected. ctest runs it as `assistant_e2e`.
