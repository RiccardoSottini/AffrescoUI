## What and why

<!-- What this pull request changes, and why. Link the issue (Fixes #123) or the merged RFC it implements.
     One change per pull request. Describe user-visible changes here: README.md and CHANGELOG.md are the
     maintainer's. -->

## How it was verified

<!-- The exact commands you ran and their results: the number of frames the rendering check compared, the number of
     unit tests, and anything you tried by hand. For a new scenario, say what its frames show. -->

## Checklist

- [ ] Rebased on the latest `master`
- [ ] Builds with 0 warnings: `cmake -S . -B build -G Ninja -DCMAKE_COMPILE_WARNING_AS_ERROR=ON && cmake --build build`
- [ ] The rendering check passes without `--update`: `python3 tests/render/check.py`
- [ ] The unit tests pass: `ctest --test-dir build -R unit --output-on-failure`
- [ ] New logic has unit tests, a new behaviour has a rendering scenario, and a bug fix has a test that would have
      caught it
- [ ] Doxygen on every new function, formatted with `.clang-format`
- [ ] Docs updated where behaviour changed (`SCREEN_FORMAT.md`, `VERSION_CONTROL.md`, a module's `README.md`)
- [ ] A format, protocol or security-model change implements a merged RFC (`rfcs/`), and changes the schema,
      `SCREEN_FORMAT.md` and the catalog together; or this pull request has none
- [ ] The `NOTICE` file is unchanged
