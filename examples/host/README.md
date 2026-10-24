# The host

`affrescoui_launcher`, the stock host: it opens a screen document, keeps its history and state, and listens for the
person's client and agents on local sockets. With no screen, it opens the Kanban template
([`../flow`](../flow/README.md)).

```sh
./build/affrescoui_launcher [screen.json] [--socket DIR] [--data DIR]
AFFRESCOUI_SCREENSHOT=out.ppm ./build/affrescoui_launcher   # the first frame, then exit
```

`host.cpp` is the whole program. It registers the code editor's `code` type ([`../shared`](../shared/README.md)), so
every template runs in it.
