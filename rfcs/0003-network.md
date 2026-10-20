# RFC 0003: Connections, sign-in and servers

- **Issue:** none yet (written for roadmap item 8.6, wave 3; decides D3)
- **Author:** the `3-rfcs` fleet agent (Claude), for the maintainer
- **Changes:** the screen format; the live protocol (a WebSocket transport for peers); the security model (the network,
  secrets, remote peers); the modules (a new `net` module, a `SecretStore` interface in `core`)
- **Prototype:** [`prototypes/0003-network/`](prototypes/0003-network/README.md): a mock identity provider and API, and
  a reference client whose 23 checks pin down the allowlist and TLS rules, path templates, PKCE with a loopback
  redirect, the device flow, refresh, redirects, sign-out, and that no token reaches what a document can see

## The problem

An application talks to servers: its own backend, a sign-in provider, a live feed, a team's shared copy of the app.
Today an AffrescoUI document can't do any of it. `channels` reach local sockets only; a host can write C++ for the
network and expose it through `onCall`, but then the document has no way to say *which* server and *which* calls,
an agent can't use it (no revision may add a `call`), nothing handles sign-in, and a token has nowhere safe to live
(everything in a document or its state is visible to every peer, kept in history and drawn in screenshots).

What any networked app needs, declared rather than coded:
- **where it connects,** with the headers, timeouts and retries each server wants, and **what it may ask** there
  (named operations, each a method, a path template and a body);
- **steps** to request, to stream, to sign in and out, with loading and error states the UI can show;
- **sign-in** the way native apps should do it (OAuth 2.0 and OpenID Connect with PKCE and the system browser, or the
  device flow), with tokens refreshed and kept in the OS keychain, never in the document;
- **a host that decides** which servers an app may reach, and TLS everywhere but the loopback;
- **agents that can use the backend but never point the app at a new one,** and previews that change nothing on a
  server;
- **peers that aren't on this machine** (an agent in the cloud, a colleague), and **a server** that keeps documents and
  their history for a team.

**Who needs it:** a dashboard over an API, a team's task board, a chat or a feed, an IDE with accounts and settings
sync, a creative tool with cloud storage, a game launcher with sign-in.

**Out of scope here:** collaborative editing algorithms beyond what proposals and leases give (live co-editing of one
text is the editor's concern); GraphQL and gRPC as named protocols (they are operations over HTTP, or a service of
RFC 0002); peer-to-peer networking; the details of presence UI.

## The design

| Part | What it is |
|---|---|
| **`connections`** | A section, a **capability**: each connection is a URL, its auth, headers, timeouts, retries, and its named **operations** (HTTP) or **messages** (WebSocket) |
| **Steps** | `request` (an operation, or a message on a WebSocket), `subscribe` (a stream: server-sent events or a WebSocket's messages), `signIn`, `signOut`; `cancel` from RFC 0002 |
| **Variables** | `auth.<connection>` (who is signed in, never a token) and `connections.<connection>` (online, pending, last error) |
| **Sign-in** | OAuth 2.0 / OpenID Connect for native apps: PKCE with a loopback redirect (RFC 8252), the device flow (RFC 8628), refresh; tokens in a `SecretStore` backed by the OS keychains |
| **The host's allowlist** | Origins an app may reach, set in C++. TLS required except for the loopback; certificates always verified |
| **Remote peers** | The live protocol over WebSocket, authenticated by a pluggable token verifier, behind a TLS proxy |
| **The document server** | A headless host that keeps documents and history and serves them to authenticated peers |

### `connections`

```json
"connections": {
	"api": {
		"url": "https://api.example.com/v1",
		"auth": { "type": "oauth", "issuer": "https://login.example.com", "client": "demo-desktop",
		          "scopes": ["openid", "profile", "projects"], "flow": "browser" },
		"headers": { "Accept": "application/json" },
		"timeout": 15000,
		"retry": { "times": 2, "after": 500 },
		"operations": {
			"listProjects": { "path": "/projects", "query": { "page": "{{page}}" } },
			"getProject": { "path": "/projects/{id}" },
			"saveProject": { "method": "PUT", "path": "/projects/{id}", "body": "{{project}}" },
			"projectEvents": { "path": "/projects/events", "stream": "events" }
		}
	},
	"live": { "url": "wss://api.example.com/v1/live", "auth": "api", "reconnect": { "after": 2000 },
	          "messages": { "join": { "type": "join", "project": "{{project}}" } } }
}
```

**A connection:**

| Key | Type | Default | Meaning |
|---|---|---|---|
| `url` | text | required | `https://` or `wss://` (plain `http://` and `ws://` only for the loopback). A literal: no bindings. Its origin must be on the host's allowlist |
| `auth` | object or connection name | none | How to sign in (below), or another connection's name to share its sign-in (`"auth": "api"`) |
| `headers` | name → text | none | Sent with every request. May not set `Authorization`, `Cookie`, `Host`, `Content-Length` or `Transfer-Encoding` (the error names the header) |
| `timeout` | milliseconds | 30,000 | Per request, from sending to the last byte (1 to 600,000) |
| `retry` | `{ "times", "after" }` | none | For `read` operations only: retry on a network error, 429, 502, 503 or 504, after `after` ms, doubling, honouring `Retry-After`. At most 5 times |
| `operations` | name → operation | | What may be asked over HTTP |
| `messages` | name → message | | What may be sent over a WebSocket (`wss://` connections) |
| `reconnect` | `{ "after" }` or `true` | none | WebSocket: reconnect after a drop (`true`: 2 s), as channels do |

**An operation:**

| Key | Default | Meaning |
|---|---|---|
| `method` | `"GET"` | `GET`, `HEAD`, `POST`, `PUT`, `PATCH` or `DELETE` |
| `path` | required | Appended to the connection's URL. `{name}` placeholders take the step's `with` values, each percent-encoded as one segment, so a value can't add a `/`, a `?` or a `..` step (`"../admin"` becomes `..%2Fadmin`) |
| `query` | none | Name → value, evaluated with the step's `with` as variables, encoded as the query string |
| `body` | none | A value evaluated with `with` as variables, sent as JSON (`"form": true` sends it form-encoded) |
| `headers` | none | Extra headers, the same restrictions as the connection's |
| `effect` | `read` for `GET` and `HEAD`, `write` otherwise | What it does on the server, for the capability boundary (below) |
| `stream` | none | `"events"` (server-sent events) or `"lines"` (one JSON value per line): the operation is used with `subscribe` |
| `result` | the whole body | A JSON Pointer into the response body: `"/data"` |
| `timeout` | the connection's | |
| `mock` | none | `{ "status", "body" }`: the answer used while a proposal is previewed and by scenarios (7.4), instead of the server |

**A message** (WebSocket) is a value evaluated with the step's `with` as variables and sent as one JSON text frame;
its `effect` defaults to `write`. Messages the server sends run the action `"<connection>:<type>"` with the message as
`event` (its `type` key names it), exactly as channels do, plus `"<connection>:open"`, `":closed"` and `":error"`.

Operation definitions read only their step's `with` values (and constants): not the state. So the declaration fixes
*where* (origin, method, path shape) and the step supplies *what* (the arguments).

### Steps

```json
{ "request": "api.saveProject", "with": { "id": "{{state.current.id}}", "project": "{{state.current}}" },
  "into": "state.saved", "loading": "state.saving", "id": "save",
  "onDone": [{ "run": "projects.load" }], "onError": [{ "set": "state.error", "to": "{{error.message}}" }] }
```

- **`request`**: `"connection.operation"` (or `"connection.message"` on a WebSocket). The same keys as RFC 0002's
  `invoke`: `with`, `into`, `loading`, `timeout`, `id` (a newer request with the same `id` cancels the older one),
  `onDone` with `result`, `onError` with `error`. Never computed. `error` is `{ "code", "message", "status", "body" }`
  with `code` one of `network`, `timeout`, `cancelled`, `status` (a 4xx or 5xx, with `status` and the parsed
  `body`), `signedOut`, `refused` (the host's allowlist), `preview`, `tooLarge`.
- **`subscribe`**: a streaming operation (`"api.projectEvents"`), with `do` per event, or `append`/`keep`, or `into`
  (the latest), batched per frame, with the lifetime rules of RFC 0002 (a node's subscription ends with the node). A
  WebSocket's messages run `<connection>:<type>` actions and need no `subscribe`.
- **`signIn`**: `{ "signIn": "api", "onDone": [...], "onError": [...] }` starts the connection's flow: it opens the
  system browser (PKCE) or shows a device code in `auth.api.device`. `onDone` runs when signed in.
- **`signOut`**: `{ "signOut": "api" }` revokes the refresh token where the provider supports revocation, forgets
  every token, and sets `auth.api` to signed out.
- **`cancel`** (RFC 0002) cancels a request, a subscription or a sign-in in progress.

**Loading and error states** are ordinary state, written where the step says (`loading`, `into`, `onError`). The
`connections.<name>` variable adds what a status bar needs: `{ "online", "pending", "state", "error" }`, with
`state` `"idle"`, `"connecting"`, `"open"`, `"closed"` or `"refused"`.

### `auth.<name>`

```json
{ "state": "signedIn", "signedIn": true, "user": { "name": "Ada Lovelace", "email": "ada@example.com" },
  "scopes": ["openid", "profile", "projects"], "device": null, "error": null }
```

`state` is `"signedOut"`, `"signingIn"`, `"signedIn"` or `"expired"` (a refresh failed: sign in again). `user` holds
only the profile claims (`name`, `email`, `picture`, `sub`) the ID token or `userinfo` gives. `device` is `{ "code",
"url", "expires" }` during the device flow. **No token, code or verifier is ever in `auth`, the state, the document,
the history, a log line or a screenshot.**

### Sign-in

**`auth` keys:** `type` (`"oauth"`, or `"key"` for an API key the user types once, kept in the secret store and sent
in a header the declaration names), `issuer` (an `https://` URL on the allowlist; its
`/.well-known/openid-configuration` is read), `client` (the public client id: native apps are public clients, so
there is no client secret, per RFC 8252), `scopes`, `flow` (`"browser"`, the default, or `"device"`), `audience` (for
providers that need it).

**The browser flow** (RFC 8252 with PKCE, RFC 7636):
1. A verifier of 48 random bytes and its S256 challenge; a random `state`; a nonce for the ID token.
2. A listener on `127.0.0.1` at an ephemeral port (`http://127.0.0.1:<port>/callback`, allowed on any port by RFC 8252
   section 7.3), one-shot, closed after the callback or 5 minutes.
3. The system browser opens the authorization URL (never an embedded web view: the app must not see the password).
4. A callback whose `state` doesn't match is answered 400 and ignored, and the listener keeps waiting, so a local
   program can't slip in its own code (the prototype checks it).
5. The code is exchanged with the verifier; the ID token's signature (the issuer's JWKS), `iss`, `aud`, `exp` and
   nonce are verified; then the user's claims are kept for `auth.user`.

**The device flow** (RFC 8628) is for machines without a browser or a person at them (a headless host, a TV, a
kiosk): `auth.api.device` shows the code and the URL, the app polls at the given interval, adds 5 seconds on
`slow_down`, stops on `access_denied` or `expired_token`.

**Refresh.** The access token lives in memory only; the refresh token in the `SecretStore`. A request that gets 401
refreshes once and retries once; a refresh that fails (revoked, expired) clears the tokens and sets `auth` to
`"expired"`, and the request fails with `signedOut`. Rotated refresh tokens are stored as they arrive. Tokens are
refreshed shortly before they expire, so a request rarely waits on a refresh.

**Where a token goes:** only in the `Authorization` header of requests to its own connection's origin, and of the
connections that name it (`"auth": "api"`) when their origin is on the allowlist too. A redirect is followed only
within the same origin (at most 5); a redirect to another origin is an error, so the token is never sent there.

**`SecretStore`**, a `core` interface (so any module can keep a secret without depending on `net`):

```cpp
namespace affresco {
/// Secrets kept by the operating system, never in a document, state, history, log or screenshot.
class SecretStore {
public:
	virtual ~SecretStore() = default;
	[[nodiscard]] virtual std::optional<std::string> read(std::string_view key) = 0;
	[[nodiscard]] virtual std::optional<std::string> write(std::string_view key, std::string_view secret) = 0; ///< An error, or nullopt.
	virtual void remove(std::string_view key) = 0;
};
}
```

Backends: macOS Keychain (Security framework), Windows Credential Manager (`CredWrite`), Linux libsecret (the Secret
Service over D-Bus). Keys are `affrescoui/<app id>/<issuer>/<client>`. Where no keychain is available (a Linux machine
without a Secret Service), the default is memory only (sign in at each start); a plain file is never the default,
and a host that wants one must pass its own `SecretStore`.

### The host's allowlist, and TLS

```cpp
screen.network().allow("https://api.example.com");
screen.network().allow("https://login.example.com");
screen.network().allow("https://*.example-cdn.com");     // any subdomain, not the bare domain
```

- **Origins** (scheme, host, port) only. A connection or an issuer whose origin isn't allowed is refused when the
  screen loads (`connections.api.url: 'api.example.net' isn't a host this app may connect to`), and its state is
  `"refused"`. The prototype checks the tricks: `https://api.example.com.evil.net`, `https://api.example.com@evil.net`,
  another port, a wildcard against the bare domain.
- **TLS** is required for everything except the loopback (`127.0.0.1`, `::1`, `localhost`). Certificates are always
  verified against the OS trust store, with TLS 1.2 or later. No key in any document turns verification off. A host
  may add a certificate authority or pin keys in C++ (an enterprise proxy, a self-hosted server).
- **Proxies** follow the system's settings; a host may set one.
- **Responses are untrusted:** at most 8 MiB into state (more is `tooLarge`; large data is a model, RFC 0002), headers
  at most 64 KiB, decompressed size capped as well as compressed, JSON parsed with `parseUntrusted`; server-sent
  events and WebSocket frames have their own caps (an event or a message at most 8 MiB).
- **Rate:** at most 6 requests at once per origin and 50 requests a second per connection by default (a host may change
  both), so a screen (or a revision that adds an `every`) can't hammer a server.

### What a revision may change

The same line as RFC 0002: **use what was declared, never declare more.**

| A revision may | A revision may not |
|---|---|
| add, change or remove `request`, `subscribe`, `signIn`, `signOut` and `cancel` steps that name declared connections and operations | change anything in `connections` (a capability): no new connection, URL, operation, message, header or auth |
| change `with` arguments, where results go, `loading`, handlers, timeouts on steps | give `request` a computed name |
| read `auth.*` and `connections.*` anywhere | read a token: there is none to read |

**Effects decide review.** Adding or changing a step that runs a `write` operation or message, or `signIn`/`signOut`,
or adding a new way to run an action that holds one (a key, a button), always goes to review, whatever the `allow`
rules say; the reviewer sees `uses api.saveProject (PUT /projects/{id}: write)`. `read` operations follow the
permission rules like any change.

**What remains possible, stated plainly:** a revision that may add a `read` request can send the screen's state to
the app's own declared backend in its arguments (a query parameter, say). It can't send it anywhere else: the origin,
method and path shape are fixed by the declaration and the host. A screen that holds data its own backend must not
see shouldn't let agents add requests.

### Previews have no side effects

While a proposal is previewed (by a reviewer, or drawn by `render`), requests with a `write` effect, messages, and
`signIn`/`signOut` are not sent: they answer with the operation's `mock` if it has one, else with `error.code`
`"preview"`. `read` requests are answered by their `mock` when they have one, and otherwise sent (GET is safe by HTTP's
own rules), unless the host turns that off. Scenarios (7.4) use the same mocks, so an app's behaviour is tested
without a server.

### Remote peers over WebSocket

Today peers connect over local sockets only, and a peer is a program of the same user. Remote peers (8.8a) bring the
same live protocol (`SCREEN_FORMAT.md`, "Peers and the live protocol") over WebSocket, one JSON object per text frame:

```cpp
screen.listenWebSocket(app, "127.0.0.1:7420", MyTokenVerifier{issuer, audience});
```

- **Authenticated.** The upgrade request carries `Authorization: Bearer <token>` (or, for a browser, which can't set
  that header, a first message `{"type": "auth", "token": ...}` within 5 seconds). A pluggable `TokenVerifier`
  (an OIDC verifier against an issuer and audience on the allowlist, or the host's own) returns who the peer is: the
  **author name** its changes are recorded under (`team/ada`), chosen by the host from verified claims, never by the
  peer. A peer that fails is closed with a reason, and the refusal logged.
- **Origins checked.** A browser's `Origin` must be on an allowlist, so a web page can't connect with the user's
  cookies.
- **Limits.** `PeerLimits` (1.8b) apply per peer, plus a cap on connections and on new connections per second per
  address (which also closes, for this transport, the "number of peers isn't bounded" gap in `SECURITY.md`).
- **TLS by a proxy.** The host listens on the loopback by default and a reverse proxy (Caddy, nginx) terminates TLS
  and forwards the WebSocket. There is no TLS server code in the app; documented with a sample Caddy file.
- **Clients:** the Studio and the MCP server connect with a `wss://` URL and a token from a `signIn` (or a token the
  host was given); a screen's own `channels` may name a `wss://` URL with `"auth": "<connection>"`.

### The document server

A headless host (8.8b) that keeps documents and their history on disk (1.6, 4.8a) and serves them to authenticated
remote peers:

- **No window, no steps.** It builds no panels and runs no actions: it applies revisions through version control and
  the format's rules (validation, `checkChange`, the checker, permissions) and relays the live protocol. So no
  document's `services` or `connections` run on the server; each client runs its own.
- **Per-person permissions:** rules name the authors the verifier gives (`team/ada(#sidebar)`, `team/*`), and reviewers
  are people.
- **Presence and leases:** peers broadcast where they are (the node they focus or select) as ephemeral messages, not
  commits; a peer may take a lease on a subtree (`#sidebar`, for N minutes), during which others' changes inside it
  are proposed instead of applied.
- **Settings sync and team templates** are documents on the server: a person's settings document, a team's
  templates; clients pull them at sign-in.
- **Deployment:** one binary (or a Docker image) listening on the loopback, behind Caddy or nginx for TLS; data in a
  folder that is backed up like any other.

### The modules

A new `net` module (8.6a) implements HTTP(S), server-sent events and WebSocket clients, the OAuth flows, the
`SecretStore` backends and the WebSocket peer transport; it uses `core` and `channel`. `format` registers the section
and the steps (`ConnectionSteps.cpp`) and calls `net` only through a `core` interface (as for services), and `screen`
wires them. All network I/O runs on `net`'s own threads and posts to the `Executor`, like the channels.

## The alternatives

**D3: the networking stack.**

| Option | For | Against |
|---|---|---|
| **libcurl** (curl licence, MIT-style) | One mature, heavily fuzzed (OSS-Fuzz) library on every platform: HTTP/1.1 and HTTP/2, proxies, redirects, timeouts, cancellation, server-sent events as a stream, and a WebSocket client (no longer experimental since curl 8.11). Uses the OS trust store where configured. Schannel on Windows is the OS's own TLS | It needs a TLS library on Linux and macOS, a second large dependency to keep patched. curl removed its Secure Transport backend in 8.15.0 (July 2025), so a bundled libcurl on macOS uses OpenSSL (or a fork, or wolfSSL) and doesn't read the macOS trust settings on its own; Apple's system libcurl is built with LibreSSL. A C API to wrap carefully |
| The platforms' own APIs (NSURLSession, WinHTTP, and libcurl or libsoup on Linux anyway) | The OS's TLS, proxies, trust store and updates; nothing to bundle on macOS and Windows | Three implementations to write, test and fuzz; behaviour differs in details (redirects, timeouts, WebSocket support); Linux still needs a library |
| Boost.Beast with Asio and OpenSSL | Full control, client and server, WebSocket included | Boost is large; TLS still means OpenSSL; much more code of our own on the security path |
| cpp-httplib (MIT, header-only) | Very small, easy | Blocking, HTTP/1.1 only, TLS through OpenSSL; not built for many concurrent streams |
| libwebsockets (MIT) | Client and server, HTTP/2, WebSocket, event loop | A large C API with its own event loop to integrate; more than the clients need |

**Recommended (D3):** **libcurl** for every client (HTTP, server-sent events, WebSocket), with Schannel on Windows and
OpenSSL 3 elsewhere (or the system libcurl where the platform ships a maintained one), pinned by hash like the other
dependencies; the OS trust store; **no TLS server in the process** (remote peers and the document server sit behind a
TLS proxy); and a **small WebSocket server framer of our own** for the loopback side (RFC 6455's framing is small),
fuzzed from its first commit. The size of libcurl and OpenSSL in the release binaries is measured in 8.6a before it is
added, as `AIMS.md` asks for every dependency.

**Other choices:**
- *Operations as free URLs in steps* (`{ "request": "https://api.example.com/x" }`): far smaller declarations, but
  any revision could reach any allowed host and path; the declaration is what makes a revision's reach reviewable.
- *Tokens visible to documents* (to send them in a custom way): simpler for odd APIs, but a token in state is a token
  in every peer, history and screenshot. Odd schemes belong in the `key` type or a host-registered auth type.
- *An embedded web view for sign-in:* rejected by RFC 8252 (the app could read the password) and by the providers that
  block it.
- *Doing nothing:* apps keep writing network code in C++ behind `onCall`, which agents can't use and reviewers can't
  read.

## The cost in tokens

Measured with `python3 rfcs/prototypes/tokens.py rfcs/prototypes/0003-network` (tiktoken `o200k_base`, minified, notes
left out). None of these edits is expressible today.

| Agent edit | Today | With this RFC |
|---|---|---|
| Load a list from the backend when a panel appears | not expressible | 43 |
| The same, through an existing command (RFC 0001) | not expressible | 23 |
| A sign-in button that shows who is signed in | not expressible | 53 |
| Save on Cmd+S (a write: always reviewed) | not expressible | 22 |
| A new operation, or another server | not expressible | 22 and 15, refused with a message naming the capability |
| First read of the prototype app (whole document) | | 944 |

The declaration is written once in the screen file; agents only ever write the steps.

## Performance

- **Nothing until used:** a screen without `connections` pays nothing, and connections open on first use.
- **No frame waits on the network:** requests run on `net`'s threads; the main thread validates the step, builds the
  request from the declaration (target under 20 µs) and later applies the result as one batch.
- **Streams cost one update per frame** (server-sent events and WebSocket messages are batched, as RFC 0002's
  streams are).
- **Memory:** responses are read into a buffer up to the limit, parsed once, and moved into state.
- **What 8.6a and 8.6b must measure:** a request's round trip against the local test server (overhead over curl's own
  under 0.2 ms); 1,000 WebSocket messages a second at one update per frame; the `update/400` and `load` benchmarks
  unchanged for screens without connections.

## Security review

This is the largest change to the threat model since peers. The delta, for `SECURITY.md`:

- **New untrusted input: servers.** Anything a server sends (HTTP responses, headers, server-sent events, WebSocket
  frames, JSON, OAuth and OIDC answers) is untrusted, even from the app's own backend. Limits above; new fuzz targets
  for HTTP response handling, server-sent events, WebSocket frames (client and server), the OAuth callback query and
  token answers, and JWT parsing (7.16c lists them for wave 6).
- **A new listening socket: the loopback redirect.** Bound to `127.0.0.1` only, an ephemeral port, one-shot, closed
  after 5 minutes; protected by `state` (a forged callback is ignored) and PKCE (a stolen code is useless without the
  verifier). This is the only port the app opens for sign-in, and it accepts nothing but the callback.
- **A new listening socket: remote peers.** Off by default; when the host turns it on, every peer is authenticated by
  the verifier before any message is read, the origin is checked, limits apply, and TLS comes from a proxy. "A peer
  is a program of the same user" no longer holds for these peers: `SECURITY.md`'s trust model gains "remote peers",
  who can do exactly what their author name's permissions allow and read what every peer reads (the document, the
  state, the history), which is why secrets must never be there.
- **New capabilities: connections.** Granted by the host's allowlist in C++, declared in the screen file, never
  changed by a revision (`checkChange` guards the capability section; 8.6b adds the unit tests). The same `Rules` code
  decides the "always reviewed" effects.
- **Secrets.** Tokens live in the `SecretStore` (refresh) and in memory (access), and are sent only to their origin. A
  test (8.6c) runs a sign-in, requests and a refresh against the mock provider and searches the document, state,
  history, log and a screenshot for every token issued (the prototype does this for the `auth` variable and the log).
  Log lines name the method and path, never headers.
- **Self-approval:** unchanged. `signIn` and `signOut` and `write` operations are always reviewed when a revision adds a
  way to run them; no revision can add an operation, so an agent can't create an endpoint that approves anything.
- **Previews:** `write` operations, messages and sign-in never reach a server during a preview.
- **Exfiltration:** limited to the app's own declared operations (see "What remains possible"); no new destination
  can be added by a revision.
- **Server-side request forgery:** documents can't choose hosts; path arguments are encoded as single segments;
  redirects stay within the origin.
- **Denial of service:** request rates per connection and concurrency per origin are capped; 1.8c's work budget
  bounds the steps that start requests.
- **Supply chain:** libcurl and its TLS library become security-critical dependencies, pinned by hash and updated on
  their advisories; `SECURITY.md` lists them.
- **Callbacks:** every request, subscription and sign-in is owned by the runtime (and its node), cancelled when the
  owner goes away; deliveries check a liveness guard; the loopback listener and the device-flow poller are cancelled by
  `cancel`, sign-out and the screen closing.

## Compatibility

- **Additive within format version 1:** a new section, four new steps, two new variables (`auth`, `connections`). No
  existing screen changes meaning. `auth` and `connections` become reserved names.
- **The live protocol** is unchanged; WebSocket is a new transport for the same messages, and a `type: "auth"` first
  message for browsers.
- **History** stays RFC 6902; tokens never enter it.
- **The schema, `SCREEN_FORMAT.md` and the catalog** change together; the catalog lists each connection's operations
  with their effects, so agents read them instead of guessing.
- **Modules:** a new `net` module; `core` gains `SecretStore.hpp` and the interface `format` calls for requests. The
  public API gains `Screen::network()`, `Screen::listenWebSocket`, and a way to pass a `SecretStore`; nothing changes.

## What it costs

| | |
|---|---|
| **Tokens** | Using the backend costs 22 to 53 tokens an edit; the declaration is written once, by the screen file's author |
| **Speed** | Nothing until used; no frame waits on the network; streams cost one update per frame |
| **Security** | A new capability section (connections), a loopback listener for sign-in, an opt-in authenticated WebSocket listener, a keychain-backed secret store, and two large dependencies (libcurl and a TLS library). Revisions use declared operations, with writes always reviewed and blocked in previews; they can never add a destination |
| **`SCREEN_FORMAT.md`** | A "Connections and sign-in" section (the section, operations, messages, the steps, `auth.*`, `connections.*`, previews and mocks), rows in the steps table; `VERSION_CONTROL.md` gains the effect rule; `SECURITY.md` gains servers, sign-in, secrets and remote peers |

## Open questions

1. **D3, the networking stack.** *Recommendation:* libcurl for all clients with Schannel on Windows and OpenSSL 3
   elsewhere, the OS trust store, no TLS server in the process, and a small fuzzed WebSocket server framer of our own
   behind a TLS proxy. Measure the binary size in 8.6a first. On macOS, decide in 8.6a between Apple's system libcurl
   (LibreSSL, maintained by Apple) and a bundled one that reads the system's certificate authorities itself; the
   alternative is NSURLSession on macOS only, which this RFC doesn't recommend because it adds a second
   implementation.
2. **May a revision add `read` requests without review** (under the permission rules), and `write` ones with review?
   *Recommendation:* yes, as designed; it is the same rule as RFC 0002's services, and the VISION's own line ("use
   declared operations, never add them").
3. **`read` requests during previews: sent, or only mocked?** *Recommendation:* sent unless the operation has a
   `mock` or the host turns it off, because GET is safe by HTTP's rules and previews should show real data; `write`
   requests are never sent.
4. **Where a secret is kept when there is no keychain** (a Linux machine without a Secret Service). *Recommendation:*
   memory only by default; a host may pass its own `SecretStore`, but the library never writes tokens to a plain file
   on its own.
5. **The `key` auth type** (a user-typed API key in the keychain). *Recommendation:* include it in 8.6c: many small
   APIs have no OAuth, and without it people would put keys in state.
6. **A WebSocket server framer of our own, or a library** (libwebsockets, IXWebSocket)? *Recommendation:* our own,
   small, loopback-only and fuzzed, because the TLS and HTTP hardening sit in the proxy.
7. **Leases on subtrees: applied as proposals, or refused?** *Recommendation:* proposals, so nothing is lost and the
   lease holder reviews.
8. **Settings sync as documents on the server** (RFC 0001's settings live in state). *Recommendation:* a person's
   settings are a small document of their own on the server, merged into `state.settings` at sign-in; decided in 8.8b.
