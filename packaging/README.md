# Packages

A recipe for each package manager, one folder each. They are templates: `tools/release/packaging.py` fills in one
release's version and hashes and writes the filled copies to `dist/packages/`. Nothing here is published or submitted
anywhere, by a workflow or by hand: the filled copies are the release asset `packages-<version>.zip` on the GitHub
Release, for you to use locally.

| Folder | What it makes | Built from |
|---|---|---|
| `homebrew/` | A formula: the library (static, with its CMake package), the host (which opens the Launcher) and `affrescoui` | the source tarball |
| `winget/` | Manifests (1.6.0) for a portable install of `affrescoui` and `affrescoui_host` | the Windows zip |
| `appimage/` | `AffrescoUI-<version>-x86_64.AppImage`: the host (the default: the Launcher) and the CLI (`affrescoui`) | the Linux archive |
| `vcpkg/` | A port (`portfile.cmake`, `vcpkg.json`, `usage`), static libraries only | the source tarball |
| `conan/` | A Conan 2 recipe and a `test_package` that finds every target with `find_package(AffrescoUI)` | the source tarball |

The library recipes (Homebrew, vcpkg, Conan) install what `cmake --install` installs: the targets `affrescoui::*`
of `cmake/AffrescoUIInstall.cmake`, and the GLFW, nlohmann_json, FreeType (and on Linux curl) builds they link, which
CMake downloads and checks by hash while building (`cmake/dependencies.json`), so the build needs the network.

## Tokens

| Token | Value |
|---|---|
| `@VERSION@` | the version, e.g. `0.2.0` |
| `@RELEASE_DATE@` | the date of `SOURCE_DATE_EPOCH`, or today |
| `@SOURCE_SHA256@`, `@SOURCE_SHA512@` | `affrescoui-<version>-source.tar.gz`; the SHA-512 is computed from the file next to `SHA256SUMS` |
| `@WINDOWS_SHA256@` | `affrescoui-<version>-windows-x86_64.zip` |

```sh
python3 tools/release/packaging.py --version 0.2.0 --sums dist/SHA256SUMS [--out dist/packages]
python3 tools/release/packaging.py --check    # the Conan test checks the exported targets; every token is known
```

A recipe whose files aren't in `SHA256SUMS` (the Windows zip, before Windows builds) is skipped, with a note.

## Trying each one

From the filled copies in `dist/packages/`:

- **Homebrew**: `brew tap-new local/affrescoui`, copy `homebrew/affrescoui.rb` into the tap's `Formula/`, then
  `brew install --build-from-source local/affrescoui/affrescoui`, `brew test affrescoui` and `brew style affrescoui`.
- **winget** (Windows): `winget validate --manifest winget`; to install, `winget settings --enable LocalManifestFiles`
  and `winget install --manifest winget`.
- **AppImage** (Linux x86_64): `sh appimage/build.sh <unpacked linux archive> <appimagetool> <runtime-x86_64> [out]`,
  with the appimagetool and runtime pinned in `cmake/dependencies.json`; then
  `./AffrescoUI-<version>-x86_64.AppImage affrescoui --version`.
- **vcpkg**: `vcpkg install affrescoui --overlay-ports=vcpkg` (the port's name comes from `vcpkg.json`).
- **Conan**: `conan create conan` builds the package and its `test_package`.

The release workflow's dry run (*Run workflow*) fills them for its own files, builds and starts the AppImage and runs
`winget validate`.
