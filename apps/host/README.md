# The host

`affrescoui_launcher`, the stock host. An app is a folder of files, with an `app.json`
([SCREEN_FORMAT.md](../../SCREEN_FORMAT.md), "Apps: app.json"), and the host runs it with nothing compiled for it.
It opens the app's screen, keeps its history, state and data in `<data folder>/apps/<id>`, and listens for the
person's client and agents on local sockets. It also opens a bare screen file. With nothing to open, it opens the
Launcher ([`../../examples/launcher`](../../examples/launcher/README.md)), which lists the apps and opens them.

```sh
./build/affrescoui_launcher [app folder | app.json | screen.json] [--socket DIR] [--data DIR]
AFFRESCOUI_SCREENSHOT=out.ppm ./build/affrescoui_launcher examples/flow   # the first frame, then exit
```

`host.cpp` is the whole program. The helpers it shares with the other programs are in
[`../shared`](../shared/README.md).
