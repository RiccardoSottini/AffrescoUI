# net: HTTP(S), server-sent events and WebSocket clients

`net` reaches servers for the app. `Client` implements `core`'s `network::Network` (`Network.hpp`): requests with a
timeout, retries, a proxy and cancellation, bodies streamed as server-sent events or one line at a time, and
WebSocket connections. The `connections` section of screens and remote peers are built on it, and host code may use
it directly. The transport is libcurl with the platform's TLS (the system's libcurl on macOS; a pinned curl 8.22.0
built statically elsewhere, over OpenSSL 3 on Linux and Schannel on Windows). WebSocket frames are read and written by
the module itself (RFC 6455) over a connection curl opens. Target: `affrescoui::net` (uses `core` and libcurl;
Winsock on Windows).

## Public headers (`include/affresco/net/`)

| Header | Offers |
|---|---|
| `Client.hpp` | `Client` (the `Network`: `allow`, `check`, `start`, `connect`), `ClientOptions` (proxy, extra certificate authorities, the concurrency limits) and `checkHeaders` |
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
- **Cancelling is final:** after `Transfer::cancel` (or the `Transfer` being destroyed) no handler is called.

## Threads

One worker thread runs every HTTP transfer (curl's multi interface); each WebSocket has a thread of its own that
reads and writes its socket. Neither calls a handler: every outcome is posted to the `Executor`, so `done`, `failed`,
`event`, `opened`, `message` and `closed` run on the main thread. `start`, `connect`, `cancel` and `send` may be called
from the main thread at any time. A `Client` being destroyed cancels its transfers (no handler is called) and joins
its worker; a `Transfer` or `WebSocket` may outlive it, and then does nothing.

## Errors

- A request that can't be sent (a URL the allowlist refuses, a bad header) fails before anything is sent: `check`
  returns the reason, and `start` calls `failed` with `refused`.
- After that, every outcome is a `network::Failure` with a `code`: `network`, `timeout`, `cancelled`, `status` (a
  response outside 200..299, with its `status` and `body`), `refused`, `tooLarge` or `protocol`. Nothing throws.
- A WebSocket ends with `closed`, once, with a `Failure`: `network` for a close or a lost connection (the server's
  close code in the message), `protocol` or `tooLarge` for frames this side refused (it closes them with the RFC 6455
  code: 1002, 1003, 1007 or 1009).

## Extension points

- **Another transport** (a test double, a mock for previews): implement `network::Network`. The format only sees the
  interface: `format::Runtime::setNetwork` takes any, and `Screen::network()` gives a screen's `connections` a
  `Client`.
