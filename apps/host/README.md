# The host

`affrescoui_launcher`, the stock host. An app is a folder of files, with an `app.json`
([SCREEN_FORMAT.md](../../SCREEN_FORMAT.md), "Apps: app.json"), and the host runs it with nothing compiled for it.
It opens the app's screen, keeps its history, state and data in `<data folder>/apps/<id>`, and listens for the
person's client and agents on local sockets. It also opens a bare screen file. With nothing to open, it opens the
Launcher ([`../../examples/launcher`](../../examples/launcher/README.md)), which lists the apps and opens them.

```sh
./build/affrescoui_launcher [app folder | app.json | screen.json] [--socket DIR] [--data DIR] [--grant KEY ...]
./build/affrescoui_launcher --permissions examples/tempo                  # forget what the person allowed Tempo
AFFRESCOUI_SCREENSHOT=out.ppm ./build/affrescoui_launcher examples/flow   # the first frame, then exit
```

The app may use nothing its `app.json` declares (origins, folders, programs, sound) until the person allows it. The
host asks in a window of its own ([`../../examples/shared/host`](../../examples/shared/host/permissions.screen.json))
the first time, for this app only, and remembers the answer in `<data folder>/apps/<id>/grants.json`. `--grant KEY`
(`network:https://api.example.com`, `folder:workspace:write`, `program:git`, `audio`) allows one for a single run; a
screenshot asks nothing and allows only those.

`host.cpp` is the program, and `Permissions.cpp` what the person allows. The helpers it shares with the other
programs are in [`../shared`](../shared/README.md).
