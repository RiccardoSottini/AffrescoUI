# The Studio

`affrescoui_studio`, the person's client for a running host: ask for and direct changes, review the agents' proposals,
read the history and set the approval rules. Its whole user interface is the `studio` template, a screen document; the
C++ only connects it.

```sh
./build/affrescoui_studio [--socket DIR] [--data DIR]
./build/affrescoui test examples/studio/studio.scenarios.json
```

| File | What it is |
|---|---|
| `studio.cpp` | The program: loads the screen and connects it to the host's socket |
| `EvidencePicture.hpp` | A proposal's evidence pictures: decoded, and the changed region outlined, clipped to the picture whatever region the peer sends (`tests/unit/studio_evidence.cpp`) |
| `studio.screen.json` | The screen document |
| `studio-features.json`, `studio-repairs.json` | Parts of the document it includes |
| `studio-strings.json` | Its texts in English, Spanish and French (`strings`, `t()`) |
| `studio.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `studio.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |

## What it shows

- **Connections**: a list of hosts and document servers (add, remove, switch), each with its state (connecting,
  connected, reconnecting, failed); the address is a `comboBox`, the token a `secret` field with `maxLength` that stays
  in memory only. Channels, `onError`, and a native `call` (`connectTo` in `studio.cpp`).
- **Review**: proposals with their diff, before/after pictures and scenario evidence; accept, reject, always allow. An
  author never approves their own change: no Accept button, and the shortcut, the palette command and a direct `run` all
  refuse it (scenarios `own-change-*`).
- **History**: revisions with their parents (merges and their proposal), blame, revert and restore.
- **Permissions**: rules, budgets, data rules and roles as JSON drafts (`tabs`, `textarea`); an edit becomes a proposal
  for another reviewer, never applied here.
- **Inspector**: the connected app's outline, one level at a time (`list`, `onOpen`, `keys`), the chosen node's
  properties (`table`) and its blame.
- **Features and repairs**: included documents (`include`), components with parameters, the built-in `dialog`,
  `quickPick` (the palette) and `toast` components.
- **Settings**: light and dark themes (`tokens`, kept with `persist`), English, Spanish and French (`strings`, `t()`).
- **Commands and keys**: a palette of commands with keybindings (`keys`), full keyboard use (Tab order, arrows, Enter,
  Escape), accessible roles and names, `live` regions for the status.
- **Layouts** from 480 × 640 to 1920 × 1080: below 900 wide the two sides become panes (`split`, `tabs`).
