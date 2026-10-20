# Prototype for RFC 0002: services and data models

What [RFC 0002](../../0002-services.md) specifies for its `jsonrpc` provider, as code that runs:

| File | What it is |
|---|---|
| `service.py` | A tiny JSON-RPC 2.0 service over stdio with LSP's `Content-Length` framing: a request from the service to the client during `initialize`, `files/read`, a streamed `search/find` with `$/progress` that honours `$/cancelRequest`, and a slow method for timeouts |
| `client.py` | A reference client and its checks: the framing with its limits (8 KiB of headers, 8 MiB a message), requests matched to answers by id, errors, streams and progress delivered on one "main" thread, cancellation, a timeout that cancels, a late answer dropped, and a request from the service answered by a handler. The service is started from an argument vector, without a shell |
| `proposed.screen.json` | The RFC's syntax (it doesn't load today): `services`, `invoke`, `subscribe`, `cancel`, `respond`, `services.cpp.state` and a `repeat` over `models.files.tree` |
| `edits.json` | What an agent sends to use services, today and with the RFC |
| `model_read.py` | What an agent reads for a large list: 10,000 rows of state against a model's summary |

Run the checks (only the standard library):

```sh
python3 rfcs/prototypes/0002-services/client.py
```

They print one line per check and end with `all checks passed` (18 checks). They are the behaviour 8.5a's C++
provider is meant to have, and the first unit tests it should port.

Count the tokens:

```sh
pip install -r tests/perf/requirements.txt
python3 rfcs/prototypes/tokens.py rfcs/prototypes/0002-services
python3 rfcs/prototypes/0002-services/model_read.py
```
