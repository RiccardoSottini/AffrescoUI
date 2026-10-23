# The Studio

The person's client for a running host, modelled on a desktop Git client and a code-review site's pull requests: ask
for and direct changes, review the agents' proposals, read the history and set the approval rules. It is a screen
document and nothing else: the stock host runs it with `--client`, which listens on nothing and lets the document's
channels connect in the socket folder.

```sh
./build/affrescoui_host examples/studio/studio.screen.json --client [--socket DIR] [--data DIR]
./build/affrescoui test examples/studio/studio.scenarios.json
```

| File | What it is |
|---|---|
| `studio.screen.json` | The screen document |
| `studio-features.json`, `studio-repairs.json` | Parts of the document it includes |
| `studio-strings.json` | Its texts in English, Italian and Arabic (`strings`, `t()`) |
| `studio.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `studio.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `PARITY.md` | What it has of its counterparts, workflow by workflow |

## What it shows

- **Connections**: a list of hosts and document servers (add, remove, switch), each with its state (connecting,
  connected, reconnecting, failed); the address is a `comboBox`, the token a `secret` field with `maxLength` that stays
  in memory only. Channels, and the `connect` step with `to` and `token` to switch.
- **Review**: proposals with their diff, before/after pictures (the host's PNGs as `data:` image sources, the changed
  region outlined with `highlight`) and scenario evidence; accept, reject, always allow. An
  author never approves their own change: no Accept button, and the shortcut, the palette command and a direct `run` all
  refuse it (scenarios `own-change-*`).
- **History**: revisions with their parents (merges and their proposal), blame, revert and restore.
- **Permissions**: rules, budgets, data rules and roles as JSON drafts (`tabs`, `textarea`); an edit becomes a proposal
  for another reviewer, never applied here.
- **Inspector**: the connected app's outline, one level at a time (`list`, `onOpen`, `keys`), the chosen node's
  properties (`table`) and its blame.
- **Features and repairs**: included documents (`include`), components with parameters, the built-in `dialog`,
  `quickPick` (the palette) and `toast` components.
- **Settings**: light and dark themes (`tokens`, kept with `persist`), English, Italian and Arabic (`strings`, `t()`,
  right to left with `dir`).
- **Commands and keys**: a palette of commands with keybindings (`keys`), full keyboard use (Tab order, arrows, Enter,
  Escape), accessible roles and names, `live` regions for the status.
- **Layouts** from 480 × 640 to 1920 × 1080: below 900 wide the two sides become panes (`split`, `tabs`).
