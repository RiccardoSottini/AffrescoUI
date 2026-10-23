# connections: servers a screen declares (RFC 0003)

The `connections` section declares servers and their operations, or a WebSocket's messages. `Connections` serves
them to the runtime as a `services::Services` backend: an operation is an HTTP request built from its declaration and
the step's `with`, and a WebSocket's incoming messages run the actions `<connection>:<type>`.

| File | Holds |
|---|---|
| `ConnectionSpec.*` | The section read and checked into plain values |
| `Connections.*` | Requests, WebSocket messages and actions, previews, rate limits |
| `ConnectionAuth.*` | A connection's `auth`: the identity provider, and the bearer header it adds |
| `AuthSteps.cpp` | The `signIn` and `signOut` steps |
 Limits: 64 connections a screen, 256 operations a connection, 50 requests a second each, messages of at most 8 MiB,
timeouts of at most ten minutes. A URL's host comes from the declaration only: the step's values fill `{name}`
segments and the query percent-encoded, and the host must be one the app may connect to. Tokens stay in the identity
provider; the `auth` variable shows each provider's state, never a token. While a proposal is previewed, only a read
with no `mock` reaches the server.

Tests: `tests/unit/connections.cpp`, `auth.cpp`, `services.cpp`.
