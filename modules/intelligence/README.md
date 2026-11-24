# intelligence: the providers of intelligence a host's assistant uses

`intelligence` answers an app's assistant (the `ask` step, `examples/shared/addons/assistant`) with a provider of
intelligence the person set up once, for every app: the person's own command line (Claude Code, `claude`, or GitHub
Copilot, `copilot`), a local LLM (a model on the person's computer, served by Ollama, LM Studio or llama.cpp's
llama-server), or an API (Anthropic's Messages API, OpenAI's Chat Completions, a Chat Completions server at a base URL;
these are hidden for now, `"shown": false`). Target: `affrescoui::intelligence` (uses the library,
`affrescoui::affrescoui`: the screen, `net` for the APIs, `services` to run a command line).

It is a service a host offers, gated like the others. An app that asks the agents declares the `assistant` capability
in its `app.json` (`affrescoui check` says when one is missing), and the stock host attaches its agent only once the
person allowed it, for that app (`apps/host`). An agent connected over MCP needs no grant: the person started it.

## What it is configured with

| File | Holds |
|---|---|
| `providers.json` (here, built in) | The catalogue: each provider's kind, base URL, whether it needs a key, its default model and the models to fall back on, whether its model is given tools (`tools`), the servers its base URL can be set to (`presets`) and how long one answer may take (`timeout`). Data, not code: a provider of an existing kind is a new entry |
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
| `CommandLine.hpp` | What the command lines share: `findCommandLine`, `keptEnvironment`, `runCommandLine` (one headless run on `services::runProgram`, in an empty temporary folder, input on stdin, output bounded to 4 MiB, a time limit, stopped every 100 ms) |
| `ClaudeCode.hpp` | The claude-code provider: `findClaude`, `claudeArguments`, `claudeEnvironment`, `runClaude` (with no tools), `parseClaudeOutput` |
| `Copilot.hpp` | The copilot provider: `findCopilot`, `copilotArguments`, `copilotEnvironment`, `copilotInput`, `runCopilot` (with no tools), `parseCopilotOutput` |
| `Http.hpp` | `HttpSession`: HTTP to one origin from the agent's worker thread, every byte bounded (`net`) |
| `ScreenQueries.hpp` | The screen as an agent reads it (`outline`, `nodeAt`, `findNodes`) and the format guide by topic (`readFormatGuide(folder)`, `formatGuide`) |

## The command lines

A command-line provider takes no key and no base URL: it is the person's own program, signed in their own way, found
on the PATH (or where its installers put it) and offered only when it is installed. Each request runs it once,
headless, from an argument vector (no shell), in an empty temporary folder of its own (this user's only, removed
after), with an environment cut to what it needs (no API keys or tokens, no `AFFRESCOUI_` variables), its input on
stdin (so the screen never shows in the list of processes), its stdout bounded to 4 MiB, a time limit, and a check
every 100 ms that stops it. With no tools (`"tools"` can only be `false`), it answers with the JSON protocol below
("Undoing an earlier change", "Using the app's commands"): reading rounds, a revision or an undo, command runs, its
answer read as loosely as a local model's.

- **Claude Code** (`claude -p`): `--tools ""` (no tools at all), `--restricted`, `--safe-mode` (no hooks, plugins,
  skills, CLAUDE.md or MCP servers of the person's), `--strict-mcp-config`, `--no-session-persistence`, the system
  prompt as its own, the answer as one JSON object.
- **GitHub Copilot** (`copilot`, npm `@github/copilot`; on Windows the `copilot.exe` of WinGet): the prompt on stdin
  (piped, with no `-p`), `--output-format json` (one JSON event a line: the last `assistant.message` is the answer, a
  `session.error` the failure). It has no flag that turns its tools off, so `--available-tools` names only a tool
  that doesn't exist (the model sees none: no shell, files, web, agents or MCP tools) and `--deny-tool` denies every
  kind of tool besides; `--disable-builtin-mcps`, `--no-custom-instructions`, `--no-ask-user`, `--disallow-temp-dir`,
  `--sync-session=off`, `--no-auto-update`, `--disable-client-telemetry`, `--log-level none`. Its environment keeps
  `COPILOT_HOME` (its sign-in and settings) and drops `GH_TOKEN`, `GITHUB_TOKEN` and `COPILOT_GITHUB_TOKEN` (the
  person's own sign-in is used), `COPILOT_ALLOW_ALL` and the provider, model, instructions and skills variables; it
  sets `COPILOT_AUTO_UPDATE=false`, `COPILOT_DISABLE_LOGIN_SHELL_ENV=1` and the `GITHUB_COPILOT_PROMPT_MODE_*`
  variables to `false`. It expands an `@` followed by a path into that file's or folder's path and size, so every
  `@/`, `@~`, `@.`, `@\` and `@C:` of the input is broken by a word joiner (U+2060). The system prompt goes first in
  the input, since it has no option for one. It runs with the person's own `COPILOT_HOME`, so their `copilot login`
  signs it in (no token from the environment is used: the person runs `copilot login` once), and like the person's
  other Copilot use it keeps its own history of each run on this computer (`$COPILOT_HOME/session-state`,
  `session-store.db`), which no option turns off; `--sync-session=off` keeps it off the GitHub account, and the
  Settings' note says so.

## A local LLM

The `local` provider ("Local LLM") is an `openai-compatible` entry of the catalogue, with no code of its own: a model on
the person's computer, through the Chat Completions API that Ollama (`http://127.0.0.1:11434/v1`, the default), LM
Studio (`http://127.0.0.1:1234/v1`) and llama.cpp's llama-server (`http://127.0.0.1:8080/v1`) all serve. Its `presets`
fill the base URL in one click in the Settings, and any other base URL can be typed (https, or http only on the
loopback, `checkBaseUrl`). A key stays optional (llama-server's `--api-key`, LM Studio's authentication), kept in the
keychain with the origin it was given for, like any compatible server's.

- **The models** come live from the server's `GET /v1/models` (`intelligence.models`, asked with the base URL the form
  shows, before it is saved; the key goes only to the origin it was saved for), without the embedding models LM Studio
  lists. With no fallback list, a server that doesn't answer makes the call fail with why, and the Settings say to
  start Ollama, LM Studio or llama-server; a server that answers with no model says to load or download one.
- **Tools are tried** (`"tools": "auto"`, `Tools::Tried`). Small local models often lack tool calling, so if the
  server refuses tools in its first answer (a 4xx, or llama-server's 500, whose body speaks of tools, e.g. Ollama's
  "does not support tools", llama-server's "tools param requires --jinja flag"), the request goes on with the JSON
  answers claude-code gives (one JSON object a round: reads, then a revision, an undo or runs, and the reply), and the
  agent remembers that server and model for its later requests. A model that calls no tool in its first answer is
  asked again in the same JSON protocol (not remembered). A refusal for any other reason (a refused key, a rate limit,
  a context too small) is the request's error. Any API provider whose server refuses tools falls back the same way.
- **Its answers are read loosely**: inside a code fence, with text around it, or after `<think>` reasoning, the first
  object of the protocol in the answer is taken (the search bounded to 256 KiB and 16 places); an answer that holds a
  `{` but no object that can be read is asked again, within the two retries a rejected revision has, then the
  request ends saying a larger model may do better (never with the broken text as the reply).
- **It is slow to start**: a model reads the whole prompt before its first word, so one answer may take `timeout`
  (300 s, against two minutes for the APIs; measured: a 27B model in LM Studio on an M4 Pro took 122 s for its first
  answer on the Flow example, 109 s of it before the first token) and the request twice that (10 minutes, against the
  agent's 4-minute budget); the person can cancel it from the chat. The Settings' test waits 90 s. Every other bound
  (steps, revisions, runs, tokens, bytes) is the same.
- A model needs a context of about 16k tokens: the first request (the system prompt, the tools and the screen, whole
  when small, else its outline) measured 5.8k tokens on a small screen and 13.1k on the Flow example. Ollama and LM
  Studio often load a model with a 4k context by default; a prompt longer than the context is the server's error,
  said in the chat.

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
`filter` narrows it) and `run_command` (a `runCommand` with the name and the arguments, the state as its evidence). A
model without tools (a command line, or a server that refuses them) asks for the list with `{"commands": true}` and runs
what its answer's `run` names (`[{ "command", "arguments" }]`), beside its reply; a refused run stops the rest, and the
reply says it wasn't run.

A run is the agent's own, under its channel, never the person's, and the screen's run rules decide it
(VERSION_CONTROL.md, "Running commands"). A run that waits for the person's yes shows on its card in the app's chat; the
agent waits for the outcome while a quarter of the request's time is left, then says in its reply that the run still
waits. A refusal comes back to the model as a tool error, and the loop goes on. Each run counts as a step, and a
request runs at most `AgentOptions::maxRuns` commands (12). The agent has no tool to allow, deny or undo a run: those
are the person's, on the card. The commands' names, titles, descriptions and the reasons a run gives are the app's
content: they reach the model in tool results only, on one line each and cut to 200 characters, never in the system
prompt, which only says when to run a command and that running one never approves a change.

## Tests

`tests/unit/intelligence.cpp`, `tests/unit/intelligence_agent.cpp` (a fake network, no real API),
`tests/unit/intelligence_commands.cpp` (the commands, on the same harness, `tests/unit/AgentHarness.hpp`) and
`tests/unit/intelligence_picks.cpp` (picks, the first message's sizes on the examples),
`tests/unit/intelligence_copilot.cpp` (GitHub Copilot: its arguments, environment, input and output, and a stand-in
`copilot`), `tests/unit/intelligence_local.cpp` (the Local LLM: presets, the models list, the fallback when tools are
refused or unused, answers in a fence or after reasoning), the `picks` fuzz target, and `tests/intelligence/e2e_test.py`:
the stock host with the Local LLM pointed at a fake server on the loopback, once with tool calls and once refusing tools
and answering in a fenced JSON object.

## Undoing an earlier change

The agent undoes an earlier revision the way an MCP agent does (`src/Undo.hpp`; SCREEN_FORMAT.md, "Undoing an earlier
commit"), keeping the later changes that don't depend on it. With tools, `revert_revision` takes `revision`,
`dry_run`, `drop` and `keep` (conflict paths, or `"all"`), `then` (a JSON Patch), `head` and `message`, like the MCP
server's. `dry_run` sends `undoPlan` and returns the plan, cut to what the model needs (the commit's number, short id
and message, `outcome`, `blocked`, `head`, `later`, `conflicts`, `state`, `warnings`, and how many more there are when
there are more): nothing changes. Otherwise it sends `revert`, as the agent's provider, with only the revision and the
choices taken from the model, and returns "applied: revision 12", "proposed: proposal 4 waits for the person's review"
or "rejected: ..."; a rejection on a conflict says to read the plan. Arguments that break the tool's schema are
refused before the screen is asked. An undo counts as a revision against `maxRevisions`.

Without tools (a command line, or a server that refuses them), the answer holds `"undo": {"revision", "drop"?, "keep"?,
"then"?, "head"?, "message"?}` in place of `"revision"` (never both), sent as a `revert` (never a dry run), and a
reading round may ask `"plan": 12` (or the same object as `undo`) beside `read`, `find` and `guide`. A refused undo is
given back to fix, within the same two retries as a revision. Both kinds of model are told to undo with the undo, never
with an inverse revision of their own; to read the plan first; and never to drop a later change the person didn't ask to
lose, but to keep it, rebuild it with `then`, or ask. The screen decides the rest: an agent's undo that drops or
rebuilds later changes always waits for the person's review.
