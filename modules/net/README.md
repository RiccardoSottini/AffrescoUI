# net: HTTP(S), server-sent events and WebSocket clients, and remote peers

`net` reaches servers for the app. `Client` implements `core`'s `network::Network` (`Network.hpp`): requests with a
timeout, retries, a proxy and cancellation, bodies streamed as server-sent events, one line at a time or as raw bytes
(`Stream::Bytes`: each chunk of a success as it arrives, at most `maxBodyBytes`, e.g. audio), and
WebSocket connections. The `connections` section of screens and remote peers are built on it, and host code may use
it directly. The transport is libcurl with the platform's TLS (the system's libcurl on macOS; a pinned curl 8.22.0
built statically elsewhere, over OpenSSL 3 on Linux and Schannel on Windows). WebSocket frames are read and written by
the module itself (RFC 6455) over a connection curl opens. Remote peers (`RemotePeers.hpp`) are peers of a screen
over WebSocket: `PeerServer` accepts them with a token, and `connectPeer` connects as one. Target: `affrescoui::net` (uses `core` and libcurl;
Winsock on Windows).

## Public headers (`include/affresco/net/`)

| Header | Offers |
|---|---|
| `Client.hpp` | `Client` (the `Network`: `allow`, `check`, `start`, `connect`), `ClientOptions` (proxy, extra certificate authorities, the concurrency limits) and `checkHeaders` |
| `RemotePeers.hpp` | `TokenVerifier` (checks a token, names the author), `PeerServer` and `PeerServerOptions` (origins, caps, timeouts), `connectPeer` (a `Channel` to a server, with a token) |
| `SignIn.hpp` | `SignIn`, the `identity::Identity` of a host: OAuth 2 / OpenID Connect for public clients (authorization code with PKCE and a loopback redirect, device flow, refresh); `SignInOptions` (the `SecretStore`, how the browser opens), `systemSecretStore` (Keychain, Credential Manager, Secret Service), `openInBrowser` |
| `Url.hpp` | `Url` and `parseUrl` (http, https, ws and wss only; no user info), `isLoopback`, `OriginAllowlist` (exact origins and `*.` subdomains), `encodeSegment` |

The request, response and handler types are `core`'s (`affresco/core/Network.hpp`), so modules that only send
requests depend on the interface, not on this module.

## Invariants

- **Only allowed origins are reached.** Every URL is checked against the host's `OriginAllowlist` before anything is
  sent: https and wss to any allowed origin, http and ws to the loopback only. A redirect is followed only within the
  request's origin, at most 5 times; any other redirect fails with `status`.
- **TLS is always verified:** TLS 1.2 or later, the certificate chain and the host name, from the system's
  authorities (plus `ClientOptions::caFile`). There is no option to turn it off.
- **Headers are checked:** names are HTTP tokens, values have no CR, LF or NUL, at most 100 headers and 64 KiB, and
  none of the headers the transport sets (Host, Content-Length, Transfer-Encoding, Connection, Upgrade, Expect, TE,
  Trailer, `Proxy-*`, `Sec-WebSocket-*`). No cookies, no `.netrc`, no compression (nothing is decompressed).
- **Every byte received is bounded:** a body, a stream event or a WebSocket message at most `Request::maxBodyBytes`
  (8 MiB by default); response headers 64 KiB and 256 lines; a URL 8,192 bytes. A WebSocket message must be UTF-8
  text, and a peer's binary or malformed frame closes the connection with the RFC 6455 code.
- **Concurrency is bounded:** 6 requests in flight per origin (more wait), 256 started and not finished (more fail
  with `refused`), 16 MiB unsent per WebSocket (`send` refuses more), 4,096 events or messages posted to the main
  thread and not yet handled (more fail with `tooLarge`).
- **Retries only read:** a GET is retried, at most 5 times, after a network error, 429, 502, 503 or 504, with
  a doubling wait (a server's `Retry-After` wins, up to an hour).
- **A remote peer is authenticated before it is a peer:** its token (on the upgrade, or a first message within
  `authTimeout`) passes the `TokenVerifier`, its `Origin`, if any, is in `allowedOrigins`, and the server has room
  (`maxPeers`, and the per-address connection rate). Anything else is refused with a reason and logged without the
  token. The server listens on the loopback unless `allowRemote` is set, and has no TLS of its own (a proxy in front
  does it: `apps/server/deploy/Caddyfile`). Limits: `SECURITY.md`.
- **Tokens stay in `SignIn`.** Its `status()` holds a state, an account, an expiry and, during the device flow, the
  code to type; never a token. The refresh token goes to the `SecretStore` (in memory unless the application passes
  `systemSecretStore`), and logs name the provider, never a value. The browser flow sends a PKCE S256 challenge, a
  `state` and a `nonce`, and listens on 127.0.0.1 on a random port for one answer to the exact redirect path; the
  `state` and the `nonce` are compared in constant time. Answers are bounded (64 KiB) and time out. A 401 refreshes
  once; a refused refresh signs out and erases the stored token. No client secret: these are public clients.
- **Cancelling is final:** after `Transfer::cancel` (or the `Transfer` being destroyed) no handler is called.

## Threads

One worker thread runs every HTTP transfer (curl's multi interface); each WebSocket has a thread of its own that
reads and writes its socket. Neither calls a handler: every outcome is posted to the `Executor`, so `done`, `failed`,
`event`, `opened`, `message` and `closed` run on the main thread. `start`, `connect`, `cancel` and `send` may be called
from the main thread at any time. A `Client` being destroyed cancels its transfers (no handler is called) and joins
its worker; a `Transfer` or `WebSocket` may outlive it, and then does nothing.

A `PeerServer` has a thread that accepts connections, and each connection a thread that reads it and one that writes
it. The verifier runs on the reading thread (so it must be thread-safe); the `Connected` callback, `onMessage` and
`onClosed` run on the main thread. Destroying the server stops accepting and closes the connections still opening;
peers already handed over belong to their owner.

Sign-in's requests go through the same `Network` and post to the `Executor`; its loopback listener has a thread
that waits for one connection. `signIn`'s `done` and `onStatus` run on the main thread.

## Errors

- A request that can't be sent (a URL the allowlist refuses, a bad header) fails before anything is sent: `check`
  returns the reason, and `start` calls `failed` with `refused`.
- After that, every outcome is a `network::Failure` with a `code`: `network`, `timeout`, `cancelled`, `status` (a
  response outside 200..299, with its `status` and `body`), `refused`, `tooLarge` or `protocol`. Nothing throws.
- A WebSocket ends with `closed`, once, with a `Failure`: `network` for a close or a lost connection (the server's
  close code in the message), `protocol` or `tooLarge` for frames this side refused (it closes them with the RFC 6455
  code: 1002, 1003, 1007 or 1009).

- A `PeerServer` that can't listen (a malformed address, one off the loopback without `allowRemote`, a port taken,
  no verifier) throws `std::runtime_error`. `connectPeer` returns null for a URL or a token it refuses.

## Extension points

- **How tokens are checked:** derive from `TokenVerifier`. It returns the author for a good token (e.g. `agent/ci`),
  and must compare secrets in constant time.

- **Where secrets are kept:** derive from `SecretStore` (`core`) and pass it in `SignInOptions::store`.

- **Another transport** (a test double, a mock for previews): implement `network::Network`. The format only sees the
  interface: `format::Runtime::setNetwork` takes any, and `Screen::network()` gives a screen's `connections` a
  `Client`.

## Files

```
include/affresco/net/   the public API (above)
src/
  Client.cpp           Client: each request checked on the calling thread, then handed to the worker or a connection
  Url.cpp              URL parsing, the origin allowlist, percent-encoding
  Tcp.hpp, .cpp        plain TCP for the peer server and the tests: every wait bounded, ended by close()
  Random.hpp, .cpp     random bytes from the system's cryptographic generator
  http/                requests over libcurl
    Curl.hpp, .cpp       handle owners, starting libcurl once, the options every connection shares (TLS, proxy)
    Http.hpp, .cpp       status lines, header lines and Retry-After, parsed with bounds
    Streams.hpp, .cpp    server-sent events and line streams, fed in chunks
    Worker.hpp, .cpp     the worker thread: the curl multi loop, retries, redirects within the origin
    WorkerWeb.cpp        the browser build: fetch in place of the curl loop
    Delivery.hpp         handing what the transfer threads find to the main thread
  websocket/           WebSocket connections (RFC 6455)
    WebSocketFrames.hpp, .cpp  the protocol without a transport: keys, frames, a bounded reader
    WebSocketClient.hpp, .cpp  the client: libcurl connects, a thread reads and writes the frames
    WebSocketWeb.cpp     the browser build: the browser's WebSocket, with the same limits
  peers/               remote peers (RFC 0003)
    PeerHandshake.hpp, .cpp    the server side of the opening: upgrade, Origin, the "auth" message
    PeerConnection.hpp, .cpp   one peer's connection on the server side, as a core Channel
    RemotePeers.cpp      PeerServer (its caps) and connectPeer
  oauth/               sign-in (OAuth 2 and OpenID Connect)
    OAuth.hpp, .cpp      the protocol without a transport: PKCE, the authorization URL, the token answers
    OAuthSignIn.hpp, .cpp      SignIn: providers, states, tokens, the browser flow and refresh
    OAuthDevice.cpp      the device flow (RFC 8628)
    LoopbackRedirect.hpp, .cpp the one-time redirect listener on 127.0.0.1
    OAuthTimer.hpp, .cpp tasks on the main thread after a delay (polls, refreshes)
    OAuthBrowser.cpp, OAuthBrowserWeb.cpp  opening the browser, without a shell
  secrets/             systemSecretStore: SecretStoreApple.cpp (Keychain), SecretStoreLinux.cpp (Secret Service,
                       loaded at run time), SecretStoreWindows.cpp (Credential Manager)
  Web/curl/curl.h      the browser build's stand-in for libcurl's header: types only, nothing transfers
```
