# Intelligence settings

The settings of the provider of intelligence that every app's assistant uses (the assistant add-on in
`../assistant`), as one JSON file to `include`: a list of providers, the chosen one's form (its API key, its base URL
for an OpenAI-compatible server, a searchable model picker), *Save and use* and *Test connection*. The AffrescoUI
Launcher shows it in its Settings:

```json
"include": ["../shared/addons/intelligence/settings.json"],
...
{ "id": "settingsIntelligence", "fill": true, "children": [{ "use": "intelligenceSettings" }] }
```

It holds no code: it calls the host's `intelligence.*` natives (`examples/shared/intelligence/Intelligence.hpp`), which
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

`examples/shared/intelligence/providers.json` lists them, as data: Anthropic (the Messages API), OpenAI (Chat
Completions), an OpenAI-compatible server (Ollama, LM Studio, OpenRouter: a base URL, a key optional; `http://` only on
this computer) and Claude Code (the person's own `claude` command line and sign-in, run with no tools). The models come
live from the provider (`GET /v1/models`) when it can be reached with the saved key, else from the list there.

## State, actions and strings

All in `state.intelligence`: `data` (what `intelligence.providers` answered, or the stand-in), `selected`, `baseUrl`,
`model`, `query`, `models` (the live list), `test` (`{ ok, message }`), `error`, `notice`, and the flags `native`,
`loadingModels`, `testing`, `keyShown`. The actions are `intelligence.load` (run when it is shown), `intelligence.pick`,
`intelligence.refresh`, `intelligence.pickModel`, `intelligence.apply`, `intelligence.check`, `intelligence.removeKey`
and `intelligence.getKey`. The colours are the `intelligence.*` tokens (light and dark), the strings the
`intelligence.*` keys in English, Spanish and French; a screen's own win.

## Tests

The rendering check draws it through `tests/render/screens/intelligence-settings.screen.json` (its scenarios in
`tests/render/scenarios/intelligence.json`). `tests/unit/intelligence*.cpp` test the natives, the providers' requests and
the agent with a fake network and an in-memory keychain.
