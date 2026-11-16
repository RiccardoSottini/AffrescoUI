# intelligence: the providers of intelligence a host's assistant uses

`intelligence` answers an app's assistant (the `ask` step, `examples/shared/addons/assistant`) with a provider of
intelligence the person set up once, for every app: an API (Anthropic's Messages API, OpenAI's Chat Completions, a
Chat Completions server at a base URL) or the person's own Claude Code command line. Target:
`affrescoui::intelligence` (uses the library, `affrescoui::affrescoui`: the screen, `net` for the APIs, `services` to
run the command line).

It is a service a host offers, gated like the others. An app that asks the agents declares the `assistant` capability
in its `app.json` (`affrescoui check` says when one is missing), and the stock host attaches its agent only once the
person allowed it, for that app (`apps/host`). An agent connected over MCP needs no grant: the person started it.

## What it is configured with

| File | Holds |
|---|---|
| `providers.json` (here, built in) | The catalogue: each provider's kind, base URL, whether it needs a key, its default model and the models to fall back on. Data, not code: a provider of an existing kind is a new entry |
| `<data folder>/intelligence.json` | The person's choice: the provider, its model and base URL. Never a key |
| The system's keychain (`SecretStore`) | Each provider's key, under `keyName(provider)`. It never reaches a document, the state, the history, a log or a screenshot; a failure's text is `redact`ed |

The agent reads the settings at every request, so a provider set up in the Launcher's Settings reaches the apps already
open with no restart.

## Public headers (`include/affresco/intelligence/`)

| Header | Offers |
|---|---|
| `Intelligence.hpp` | `HostAssistant` (the agent a host attaches to a screen: `attach(screen)`), `registerCalls` (the `intelligence.*` natives of a settings screen, which only the Launcher's host registers), `registerAssistantModels`, `askOnStart` |
| `Agent.hpp` | `ProviderAgent`: a `Channel` connected as author `"agent"`, whose changes wait for the person's review like any agent's; `AgentOptions` bounds its turns, tools and time |
| `Settings.hpp` | `Catalog` (`standard()`, `load`, `fromJson`), `Settings` (`load`, `save`), `endpointOf`, `checkBaseUrl` (https, or http only on the loopback), `readKey` |
| `Providers.hpp` | What the APIs are sent (`chatRequest`, `modelsRequest`) and answer (`parseTurn`, `parseModels`), and `describeFailure` and `redact`, with no secret |
| `ClaudeCode.hpp` | The claude-code provider: `findClaude`, `runClaude` (with no tools, on `services::runProgram`), `parseClaudeOutput` |
| `Http.hpp` | `HttpSession`: HTTP to one origin from the agent's worker thread, every byte bounded (`net`) |
| `ScreenQueries.hpp` | The screen as an agent reads it (`outline`, `nodeAt`, `findNodes`) and the format guide by topic (`readFormatGuide(folder)`, `formatGuide`) |

## One agent answers

When the host's agent and an MCP agent are both connected, a request goes to the last agent connected (the one the
person started most recently) and to every client, never to two agents; a cancel goes to all of them
(`modules/screen`, SCREEN_FORMAT.md "Asking the agents").

## What the person pointed at

A request from the app's chat may carry `picks`: the records the engine made when the person clicked an element
(SCREEN_FORMAT.md, the `pick` step). The agent keeps each record to the contract's keys and types (`src/Picks.hpp`,
`picksOf`): at most 4 records, texts cut to 200 bytes (an address or a pointer to 512), at most 16 bindings, 16
handlers and 4 repeats, and a record without a `node` text is left out. A record says nothing of the app's data
(bindings as written, a repeat's index), and it reaches the model as the app's content, never as instructions.

With picks, the first message no longer holds the screen or its outline. It holds the request, the attachments' lines,
then for each pick its record on one line and what makes it: in a program, the text of the definition at
`source.address` (cut to 8,000 bytes); in a JSON document, the node at `node` to depth 2 (cut to 4,000 bytes); then one
line with the revision, how many nodes and definitions there are, and how to read the rest. Each pick's part is at most
12 KiB, and a definition two picks share is shown once. The system prompt gains a short rule about picks (start from
the element; a repeat's definition builds every item; an element with no address of its own; a built-in kit's
component; a shared add-on), only for a request that has them.

Nothing new is asked of the screen: the definition or node comes from the document the agent's own `getDocument`
returned, so what a limited reader may not read isn't there, and the block says it could not be read. Without picks,
the first message is exactly as before.

## Using the app's commands

When the person asks for something the app can already do (add an entry, send a message), the agent runs one of the
app's commands rather than changing the app (`src/Commands.hpp`). It has two tools: `list_commands` (the screen's
`getCommands`, one line a command: `name(argument: type, ...)`, what it does, and `[ask]`, `[person]` or `[deny]`;
`filter` narrows it) and `run_command` (a `runCommand` with the name and the arguments, the state as its evidence).
claude-code, which has no tools, asks for the list with `{"commands": true}` and runs what its answer's `run` names
(`[{ "command", "arguments" }]`), beside its reply; a refused run stops the rest, and the reply says it wasn't run.

A run is the agent's own, under its channel, never the person's, and the screen's run rules decide it
(VERSION_CONTROL.md, "Running commands"). A run that waits for the person's yes shows on its card in the app's chat; the
agent waits for the outcome while a quarter of the request's time is left, then says in its reply that the run still
waits. A refusal comes back to the model as a tool error, and the loop goes on. Each run counts as a step, and a
request runs at most `AgentOptions::maxRuns` commands (12). The agent has no tool to allow, deny or undo a run: those
are the person's, on the card. The commands' names, titles, descriptions and the reasons a run gives are the app's
content: they reach the model in tool results only, on one line each and cut to 200 characters, never in the system
prompt, which only says when to run a command and that running one never approves a change.

## Tests

`tests/unit/intelligence.cpp`, `tests/unit/intelligence_agent.cpp` (a fake network, no real API), `tests/unit/intelligence_commands.cpp` (the
commands, on the same harness, `tests/unit/AgentHarness.hpp`) and
`tests/unit/intelligence_picks.cpp` (picks, the first message's sizes on the examples), the `picks` fuzz target, and
`tests/intelligence/e2e_test.py`: the stock host with an `openai-compatible` provider pointed at a fake server on the
loopback.
