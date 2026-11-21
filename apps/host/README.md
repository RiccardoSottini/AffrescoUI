# The host

`affrescoui_launcher`, the stock host. An app is a folder of files, with an `app.json`
([SCREEN_FORMAT.md](../../SCREEN_FORMAT.md), "Apps: app.json"), and the host runs it with nothing compiled for it.
It opens the app's screen, keeps its history, state and data in `<data folder>/apps/<id>`, and listens for the
person's client and agents on local sockets. It also opens a bare screen file. With nothing to open, it opens the
Launcher ([`../../examples/launcher`](../../examples/launcher/README.md)), which lists the apps and opens them.

```sh
./build/affrescoui_launcher [app folder | app.json | screen.json] [--socket DIR] [--data DIR] [--grant KEY ...] [--nrepl [PORT]]
./build/affrescoui_launcher --permissions examples/tempo/lisp                # forget what the person allowed Tempo
AFFRESCOUI_SCREENSHOT=out.ppm ./build/affrescoui_launcher examples/flow/lisp # the first frame, then exit
```

Ctrl+Shift+D (Cmd+Shift+D on macOS), or *Review changes* on a card in the app's assistant, opens the host's window on a
change, file by file, with preview, accept, reject and undo
([`ChangesWindow.cpp`](ChangesWindow.cpp), [VERSION_CONTROL.md](../../VERSION_CONTROL.md), "The changes, file by file").
An app's `{"call": "changes.open", "with": {"source": address, "line": n}}` opens it on its Source page instead: the
text that drew an element, read-only, its line marked, and the commit that last changed it.

Before an applied change is undone, the window shows what the undo would do. Its screen asks the host through three
calls, and the host decides only through version control, as the person (the channel `user`):

| Call | What it does |
|---|---|
| `changes.undoPlan {commit, drop?, keep?}` | The plan, worked out without changing anything (`VersionControl::undoPlan`, VERSION_CONTROL.md, "History"): which later changes were built on it, how many stay as they are, and what is left to choose |
| `changes.undo {commit, drop?, keep?, head?}` | Undoes it as one new commit by the person. `drop` takes out the later changes built on it, `keep` leaves them (that part is not undone), both `"all"` or a list of paths; `head` is the main head the plan was read on, so a choice made on changes nobody saw is refused. Without them, a plain undo that keeps everything after it |
| `changes.askUndo {commit, text}` | Runs the app's own action `assistant.start` with `{"text"}` (1 to 32 KiB), so its assistant asks the agents in the person's name; their proposal waits for review like any other. An app without that action answers "this app has no assistant" |

The window takes only `drop`, `keep` and `head` from its screen (never `then`, a patch applied with the undo), and a
malformed choice is an error it shows. `changes.list` also says `canAsk`: whether the app's document has the action
`assistant.start`. With a plan whose outcome is "clean", the window shows the Undo button as before, with a quiet line
for the later changes that stay, live data changed since that stays, and each warning. With "choose", it lists the
later changes built on it (number, message, author and the parts they changed) and offers *Undo, and take these out
too* (`drop: "all"`), *Undo the rest, leave these* (`keep: "all"`, shown off when that plan is blocked) and, with
`canAsk`, *Ask the assistant to keep them*. With "blocked", the reason, and the request to the assistant.

The app may use nothing its `app.json` declares (origins, folders, programs, sound, the assistant) until the person
allows it. The host asks in a window of its own
([`../../examples/shared/host`](../../examples/shared/host/permissions.screen.json), the `.screen.lisp` program for an app opened
from its Lisp version: `HostScreens.hpp`) the first time, for this app only,
and remembers the answer in `<data folder>/apps/<id>/grants.json`. `--grant KEY` (`network:https://api.example.com`,
`folder:workspace:write`, `program:git`, `audio`, `assistant`) allows one for a single run; a screenshot asks nothing
and allows only those.

`--nrepl [PORT]` lets an nREPL client (CIDER, Calva, Conjure) evaluate in an app written as a program, as the person:
the host listens on 127.0.0.1 (the port given, or one the system picks), prints the port, and writes it to `.nrepl-port`
in the app's folder, removed when the host ends. Each connection waits until the person says yes in the window's
question ([LISP.md](../../LISP.md), "Connecting an editor"; [SECURITY.md](../../SECURITY.md), "Editors (nREPL)"). The
Launcher takes no editor: `--nrepl` without an app exits with 2.

The Launcher is granted the examples to read (`examples`), the person's apps folder to read and write (`apps`, `--apps
DIR`), the host's program to open each app, the open and save dialogs, and the `packages` service
([`PackagesProvider.hpp`](../../modules/screen/include/affresco/PackagesProvider.hpp)) on both folders, installing into
`apps` and reading the apps' histories and the trusted publishers in the data folder. With it the Launcher exports an
app the person changed as one `.affresco.json` file, and installs one someone gave them as a new app, once they have
reviewed it; its changes then wait in the new app as a proposal only the person accepts. No other screen gets the
service, and a screenshot of the Launcher gets none of these.

`host.cpp` is the program, and `Permissions.cpp` what the person allows. The helpers it shares with the other
programs are in [`../shared`](../shared/README.md).
