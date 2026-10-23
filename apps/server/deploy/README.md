# Deploying behind TLS

AffrescoUI's listeners speak plain WebSocket on the loopback; a reverse proxy in front of them terminates TLS. Two
examples, for the document server ([`apps/server`](../README.md)) or any host that calls
`listenWebSocket`:

| File | Proxy |
|---|---|
| [`Caddyfile`](Caddyfile) | Caddy: obtains and renews the certificate itself |
| [`nginx.conf`](nginx.conf) | nginx: your certificate, and the WebSocket upgrade passed through |

Peers connect to `wss://<your host>/` with their token. The proxy adds no authentication of its own: the server checks
every token, and never accepts a peer without one (SECURITY.md, "Remote peers").
