# Prototype for RFC 0003: connections, sign-in and servers

What [RFC 0003](../../0003-network.md) specifies for connections and sign-in, as code that runs:

| File | What it is |
|---|---|
| `mock_identity.py` | A mock identity provider and API on `127.0.0.1`: OIDC discovery, the authorization code flow with PKCE (S256) and a loopback redirect on any port, single-use codes, the device flow with `authorization_pending` and `slow_down`, rotating refresh tokens, revocation, `/userinfo`, a protected `/api/projects` and a redirect to another origin. 8.6c needs one like it for its tests |
| `client.py` | A reference client and its checks: the host's allowlist and the TLS rule (with the tricks that make a URL look allowed), path templates whose arguments can't leave their segment, sign-in in the browser (a forged callback ignored, a code used once, a wrong verifier refused), the device flow, a refresh on 401, a redirect to another origin refused, refresh failure and sign-out, and that none of the tokens issued reaches the `auth` variable or the log |
| `proposed.screen.json` | The RFC's syntax (it doesn't load today): `connections` with operations, a WebSocket with messages, `request`, `signIn`, `signOut`, `auth.api`, mocks for previews |
| `edits.json` | What an agent sends to use the network, and two edits that are refused |

Run the checks (only the standard library; everything listens on `127.0.0.1` at ephemeral ports):

```sh
python3 rfcs/prototypes/0003-network/client.py
```

They print one line per check and end with `all checks passed` (23 checks). The mock leaves out the ID token's
signature, which needs RSA (the real client verifies it against the issuer's keys).

Count the tokens:

```sh
pip install -r tests/perf/requirements.txt
python3 rfcs/prototypes/tokens.py rfcs/prototypes/0003-network
```
