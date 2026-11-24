# Intelligence settings

The settings of the provider of intelligence that every app's assistant uses (the assistant add-on in
`../assistant`), as one JSON file to `include` (or `settings.lisp`, the same in Lisp): a list of providers, the chosen
one's form (its server and base URL for a local LLM, its key, a searchable model picker), *Save and use* and *Test
connection*. The AffrescoUI
Launcher shows it in its Settings:

```json
"include": ["../shared/addons/intelligence/settings.json"],
...
{ "id": "settingsIntelligence", "fill": true, "children": [{ "use": "intelligenceSettings" }] }
```

It holds no code: it calls the host's `intelligence.*` natives (`Intelligence.hpp` in `modules/intelligence`), which
`affrescoui_launcher` registers only for the Launcher (started without a screen file), so no other screen, and no revision,
can change the provider, its base URL or its key. Without them (any other host, the rendering check) it shows stand-in
providers, and saving says the settings are changed in the Launcher.

## Where things are kept

- `<data folder>/intelligence.json` (`--data`, else `~/.affrescoui`): the provider in use, its model, and the base URL and
  model chosen per provider. Never a key. Written atomically, readable by its user only.
- The keys: the system's keychain (Keychain, Credential Manager, Secret Service), one entry per provider
  (`intelligence/<provider>` under the service `AffrescoUI`).
- The key field isn't bound to the state: each change goes to the host's memory (`intelligence.stageKey`), *Save and
  use* moves it to the keychain and forgets it, and the field is built again empty. The screen's state, its history,
  its peers and its logs never see it. The form only says whether a key is saved (`hasKey`).
- A new base URL erases the key saved for the old one, so a key never goes to a server it wasn't given for.

## Providers

`modules/intelligence/providers.json` lists them, as data. The settings offer Claude Code (the person's own `claude`
command line and sign-in, run with no tools), GitHub Copilot (the person's own `copilot` command line, its GitHub
sign-in from `copilot login` and subscription, run with no tools: the model is given none, not even an MCP server's,
though the person's own MCP servers, plugins and hooks in `~/.copilot` may still start) and a Local LLM (a model on
a server of the person's: Ollama, LM Studio or llama.cpp's llama-server on this computer, each a button that fills the
base URL, or any base URL typed, `http://` only on this computer; a key optional; with a loopback address nothing
leaves the computer). The API providers (Anthropic, OpenAI, an OpenAI-compatible server such as OpenRouter) are in
the catalogue but hidden (`"shown": false`). The models come live from the provider (`GET /v1/models`, with the base URL
the form shows, even before it is saved) when it can be reached, else from the list there. A local server that doesn't
answer says to start one, with why; one that answers with no model says to load or download one.

Claude Code's models are its command line's aliases, which follow the latest model: Claude Sonnet 5.5 (`sonnet`, the
default: fast enough for a chat and strong at editing screens), Claude Opus 5.5 (`opus`, for harder, longer asks),
Claude Fable 5.1 (`fable`, the most capable) and Claude Haiku 5.5 (`haiku`, the fastest, for small changes). Each has a
line on when to use it in English, Spanish and French (`description`), and the catalogue's default is marked
*Recommended*. Its list is the whole choice (`fixedModels`): saving another model is refused, and a saved one it no
longer lists gives the default. The person's model is the default of every app's assistant, whose picker can choose
another one per app (`../assistant/README.md`).

GitHub Copilot's models are fixed the same way: Claude Sonnet 5.5 (`claude-sonnet-5.5`, the default), Claude Opus 5.5,
GPT-6.2 Sol, Claude Haiku 5.5 and Auto (Copilot picks, among the models the person's plan allows). A command line's
form shows a line on what it can't do (`intelligence.claudeCode`, `intelligence.copilot`) and *Install it* while it
isn't installed. Copilot's line also says that it keeps its own history of each run on this computer, as it does for the
person's other Copilot use.

## State, actions and strings

All in `state.intelligence`: `data` (what `intelligence.providers` answered, or the stand-in), `selected`, `baseUrl`,
`model`, `query`, `models` (the live list), `modelsError` (why the live list couldn't be read, or ""), `test`
(`{ ok, message }`), `error`, `notice`, and the flags `native`, `loadingModels`, `modelsLoaded` (a live list came,
perhaps empty), `testing`, `keyShown`. The actions are `intelligence.load` (run when it is shown), `intelligence.pick`,
`intelligence.refresh`, `intelligence.preset` (`{ url }`: a server's base URL, then its models),
`intelligence.pickModel`, `intelligence.apply`, `intelligence.check`, `intelligence.removeKey` and
`intelligence.getKey`. The colours are the `intelligence.*` tokens (light and dark), the strings the
`intelligence.*` keys in English, Spanish and French; a screen's own win.

## Tests

The rendering check draws it through `tests/render/screens/intelligence-settings.screen.json` (its scenarios in
`tests/render/scenarios/intelligence.json`), and its Lisp version through `intelligence-settings.screen.lisp`
(`tests/render/lisp/intelligence.json`), compared with the same pictures. `tests/unit/intelligence*.cpp` test the
natives, the providers' requests and the agent with a fake network and an in-memory keychain.
