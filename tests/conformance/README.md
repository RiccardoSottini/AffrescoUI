# The conformance run

The language (`modules/lisp`) measured against the
[cross-dialect Clojure test suite](https://github.com/jank-lang/clojure-test-suite) (MPL-2.0): one test file per
function of `clojure.core` and `clojure.string`, written to run on every Clojure. The score is in
`modules/lisp/CONFORMANCE.md`, written by `run.py`; what differs on purpose is in
`modules/lisp/reference/differences.md`.

The suite is fetched to run, at one commit and by its SHA-256, and never copied here: no file of it is in this
repository, a commit or a fixture.

## What is here

| File | What it is |
|---|---|
| `runner.cpp` | `affrescoui_conformance`: runs test files, each in a fresh interpreter with low limits, and prints one JSON object per file. Linked with `affrescoui::lisp` only |
| `prepare.hpp`, `prepare.cpp` | The token pass the runner makes over a file before the language reads it: marks what is out of scope, and writes each assertion's line into it |
| `shim.lisp` | What the files ask of `clojure.test` and of the suite's portability namespace (`deftest`, `testing`, `is`, `are`, `thrown?`, `when-var-exists`...), written in the language |
| `run.py` | Runs the runner over the suite, sums the files, checks them against `expected.json` and writes `CONFORMANCE.md` |
| `expected.json` | Per file, whether it read and how many assertions passed: what the test holds the language to |
| `everyday.lisp` | Our own probes: 186 forms people write every day in Clojure, each with Clojure's value. Needs nothing fetched |

## How to run it

The everyday probes build and run with every build, as the test `cli_lisp_everyday`:

```sh
cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build -R cli_lisp_everyday --output-on-failure
```

The suite needs the option, which fetches it at configure time:

```sh
cmake -S . -B build -G Ninja -DAFFRESCOUI_CONFORMANCE=ON && cmake --build build
ctest --test-dir build -R cli_lisp_conformance --output-on-failure   # the test: no file does worse than expected.json
python3 tests/conformance/run.py --update                          # after a change that does better
python3 tests/conformance/run.py --report out/report.jsonl         # every file's whole result, to read
```

`run.py` finds the runner and the suite where a build in `build/` puts them; `--runner` and `--suite` (the suite's
`test` folder) say otherwise. `--update` rewrites `expected.json` and `modules/lisp/CONFORMANCE.md`, the same bytes
from one run to the next; commit both with the change that raised them. The whole run takes about a second.

One file alone, to look at it:

```sh
./build/tests/conformance/affrescoui_conformance --shim tests/conformance/shim.lisp \
  --suite build/_deps/clojure_test_suite-src/test build/_deps/clojure_test_suite-src/test/clojure/core_test/case.cljc
```

## How it counts

Each `is`, and each row of an `are`, is one assertion, and reports once:

- **passed**: the language gave the value Clojure gives;
- **failed**: it gave another, shown as `expected` and `actual`;
- **error**: it raised an error instead, a missing function, a wrong argument or a form the language refuses, shown as
  its message. An expected error (`thrown?`) that is "not defined" is an error too, never a pass;
- **out of scope**: it holds something the language leaves out on purpose, so it is counted without being run. The
  reasons are a fixed list, each a row of `differences.md`: `numbers`, `characters`, `regex`, `vars`, `mutation`,
  `laziness`, `host`, `metadata`, `keys`, `namespaces`. Their patterns are one table in `prepare.cpp` (`calls()`,
  `wordReason`, `collectionReason`, `callReason`), a comment per row. When unsure, a form is not marked: an assertion
  that runs and fails is honest, one marked out of scope wrongly is not;
- **not run**: its function isn't defined (`when-var-exists` skips the body), or the form around it stopped first.

A file **reads** when the language's reader takes every one of its forms. A form it doesn't take is one error, and
the rest of the file still runs. A set holding something out of scope is out of scope whole, as its marked items
would otherwise be one item twice. Each file has a budget of 5,000,000 operations, 64 MiB and a call
depth of 2,000; one that runs past it is `{"exhausted": true}`, and a crash or a hang makes the test fail.

## How to read a failure

`cli_lisp_conformance` fails when the runner crashed or hung on a file, when a file that read before no longer reads,
or when a file passes fewer assertions than `expected.json` says. It prints one line per such file. Run that file
alone (above), or `--report`, and look at its `failures`: each has the line in the suite's file, the test's name, the
`testing` texts around it and what went wrong.

- An assertion that **passed before and fails now** is a change in the language: the message says which value or
  which error.
- A file that **no longer reads** names the line and the reader's message.
- A file that **does better** passes, and the test says to run `--update`.

`cli_lisp_everyday` prints each probe that doesn't give Clojure's value, with the value it gave. It fails when fewer
probes than its `--min` hold (`CMakeLists.txt`, a ratchet: raise it when more do), or when a `differs` probe no longer
gives the value it says ours is.

## How to bump the suite's commit

1. Take the commit the suite's main branch is at, and the SHA-256 of its archive:
   `curl -sL https://github.com/jank-lang/clojure-test-suite/archive/<commit>.tar.gz | shasum -a 256`.
2. Write both in `CMakeLists.txt` (`URL`, `URL_HASH`, and the comment's date), and the commit and its date in
   `run.py` (`SUITE_COMMIT`, `SUITE_DATE`).
3. Configure with `-DAFFRESCOUI_CONFORMANCE=ON`, build, and run `python3 tests/conformance/run.py --update`. Read the
   suite's changes and the new `CONFORMANCE.md`: a file that does worse is a test that changed, or a new gap. Commit
   the four files together, and say in the change what moved.
