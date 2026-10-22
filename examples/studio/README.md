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
| `studio.screen.json` | The screen document |
| `studio-features.json`, `studio-repairs.json` | Parts of the document it includes |
| `studio.scenarios.json` | Its scenarios (`affrescoui test`; the rendering check plays them too) |
| `studio.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
