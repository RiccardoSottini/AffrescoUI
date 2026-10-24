# Bottega, an app installed into another

Sinopia's slide sorter, installed into Tempera through a contribution point, an adapter and a proof: the sorter shows
Tempera's top-level layers as pages and reorders them with Tempera's own layer actions. It has no screen of its own:
its program is Tempera's natives with affrescoui's composition commands, and its scenarios play on Tempera's screen.

```sh
./build/affrescoui_bottega                                   # plays bottega.scenarios.json
./build/affrescoui_bottega check --compose SCREEN FEATURE    # affrescoui's check --compose, with Tempera's natives
./build/affrescoui_bottega contributions SCREEN [--json]     # affrescoui's contributions
python3 examples/bottega/e2e_test.py build                   # the whole install, live (ctest bottega_e2e)
```

| File | What it is |
|---|---|
| `bottega.scenarios.json` | Its scenarios: the install proven and submitted, and refused without an adapter or with a broken one (ctest `bottega_scenarios`) |
| `broken-sorter.adapter.json` | An adapter whose proof fails, so nothing is installed |
| `CMakeLists.txt` | The `affrescoui_bottega` program: this folder's `main.cpp`, Tempera's natives and affrescoui's composition commands |
| `e2e_test.py` | End to end: Tempera live, a `studio` peer asks an agent to install the sorter, accepts it, plays it, then undoes it |
| `fake_agent.py` | A deterministic stand-in for the agent: it reads the contributions over MCP, writes the adapter, checks and installs |
| `main.cpp` | The program: the scenarios' install actions, and the `check` and `contributions` commands |
| `tempera-sorter.adapter.json` | The adapter that binds the sorter's needs (pages, the current page, reorder, select, duplicate) to Tempera's exports and actions |

The sorter itself is Sinopia's, in [`../sinopia/features/slide-sorter`](../sinopia/features/slide-sorter).

## How it holds together

- **Invariants:** an install is submitted only when its proof passes; it is a revision like any other, so the host
  screen's permissions decide whether it applies or waits for a reviewer, and undoing it removes it. The manifest and
  the adapters are read as untrusted JSON.
- **Threading and ownership:** each scenario's screen owns its Tempera session; everything runs on the thread that
  plays the scenarios. The e2e test runs Tempera and the agent as separate processes and plays the `studio` peer
  itself, on a socket folder of its own.
- **Extension points:** another feature is another manifest and adapter; another host app is another adapter, bound
  to what that app exports.
- **Assets:** none of its own.
