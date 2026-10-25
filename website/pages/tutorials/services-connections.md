# Services and connections

A screen reaches the outside world only through what it declares and the host grants. Both are **capabilities**: a
revision can use them, never add or widen them.

## Services

`services` names local services that `invoke` and `subscribe` steps talk to. The host grants what they may reach:

```json
--8<-- "recipes/service-invoke.screen.json"
```

- `files` uses the `fs` provider on the folder the host granted as `workspace` (`screen.services().addRoot("workspace",
  path)` in C++; for an app, its `app.json` declares the folder and `affrescoui_launcher` asks the person the first
  time). A declaration outside the grants is refused.
- `invoke` names the service and its method. `into` receives the result, `loading` is true while it runs, `timeout`
  bounds it, and `onError` runs with `error`.
- The other providers are `jsonrpc` and `process` (programs the host allows with `allowProcess`).
  [RFC 0002](https://github.com/RiccardoSottini/AffrescoUI/blob/master/rfcs/0002-services.md) has the design.

## Connections

`connections` declares the servers that `request` and `subscribe` steps talk to, one operation at a time:

```json
--8<-- "recipes/connection-request.screen.json"
```

- Each request is built from its declared operation: method, `path` with `{name}` segments from `with`, `query`,
  `body`, `headers`, `timeout`.
- Every origin is refused until the host allows it (`screen.network().allow("https://api.example.com")`).
- `mock` is the answer used while a proposal is previewed and by scenarios, so a change can be tried and tested
  without touching the server. `connections.api` is `{online, pending, state, error}`.
- `auth` declares an OAuth 2 sign-in; the host adds the token, and the screen only sees `auth.<name>` as
  `{state, account, expires, error}`, never a token. WebSocket connections (`ws://`, `wss://`) have `messages`.
  [RFC 0003](https://github.com/RiccardoSottini/AffrescoUI/blob/master/rfcs/0003-network.md) has the design.

`affrescoui render` grants nothing: it reports that the host allows no folder and gives the screen no network, and
draws the first frame, which is why the recipes above show their empty states.

## Channels

`channels` connects the screen to other programs on local sockets; each message of type `T` runs the action `name:T`.
The example IDE's agent view talks to a coding agent this way. See “Channels and persistence” in [the screen format](../reference/screen-format.md).

Next: [features as packages](features.md).
