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

## Tests

`tests/unit/intelligence.cpp` and `tests/unit/intelligence_agent.cpp` (a fake network, no real API), and
`tests/intelligence/e2e_test.py`: the stock host with an `openai-compatible` provider pointed at a fake server on the
loopback.
