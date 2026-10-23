# Releasing AffrescoUI

For the maintainer. A release is a tag `v<version>` that the maintainer pushes. `.github/workflows/release.yml` then
builds every download and drafts the GitHub Release. A release is only that tag plus the maintainer reviewing and
publishing the draft, all on GitHub: **every artefact is an asset of that release, and nothing is published, deployed
or submitted anywhere else** (no PyPI, VS Code Marketplace, Open VSX, GitHub Pages, CDN, Homebrew, winget, vcpkg or
Conan). Agents and contributors never push tags, create releases or publish.

## What a release contains

| File | What |
|---|---|
| `affrescoui-<version>-macos-universal.tar.gz` | `affrescoui_host`, `affrescoui_studio` and `affrescoui` for macOS 13.3 or later, arm64 and x86_64 in one binary |
| `affrescoui-<version>-linux-x86_64.tar.gz` | The same for Linux x86_64 (built on Ubuntu 24.04: glibc 2.39 or later) |
| `affrescoui-<version>-windows-x86_64.zip` | The same for Windows, once Windows builds (roadmap 1.1); until then the release has no Windows file |
| `affrescoui_mcp-<version>-py3-none-any.whl`, `affrescoui_mcp-<version>.tar.gz` | The MCP server (install: `pipx install ./affrescoui_mcp-<version>-py3-none-any.whl`) |
| `AffrescoUI-<version>-x86_64.AppImage` | The Linux archive in one file |
| `affrescoui-vscode-<version>.vsix` | The VS Code extension (install: *Extensions: Install from VSIX...*) |
| `affrescoui-web-<version>.zip` | The browser build (WebAssembly host and Studio), plain files for any static server |
| `affrescoui-site-<version>.zip` | The documentation site, plain files |
| `packages-<version>.zip` | The recipes for Homebrew, winget, vcpkg, Conan and the AppImage, filled in for this release, for people to use themselves (`packaging/README.md`) |
| `affrescoui-<version>-source.tar.gz`, `affrescoui-<version>.spdx.json` | The source (`git archive` of the tag) and the SBOM |
| `SHA256SUMS` | The checksums of all of them; each file also has a build-provenance attestation |

Each archive holds the three programs, `assets/` (the templates and their fonts, which the programs find next to
themselves), `LICENSE`, `NOTICE`, `THIRD_PARTY_NOTICES.md` and a short `README.txt`.

## Once: protect the tags

*Settings → Rules → Rulesets*: a tag ruleset for `v*` that only you can create, update or delete.

## Every release

1. Pick the version (semantic: `0.2.0`) and write it in the three places `tools/release/check_version.py` reads:
   `CMakeLists.txt` (`project(AffrescoUI VERSION ...)`), `mcp/pyproject.toml` (`version`) and `mcp/affrescoui_mcp.py`
   (`serverInfo`). Check: `python3 tools/release/check_version.py`.
2. Update `CHANGELOG.md` and the README if needed, merge to `master`, and wait for CI to be green.
3. Tag and push:
   ```sh
   git tag -a v0.2.0 -m "AffrescoUI 0.2.0"
   git push origin v0.2.0
   ```
4. Watch *Actions → Release*. It checks the tag against the version, builds and packs each system, checks each
   download on a clean machine (the tool checks every template; on Linux the host draws a frame from its own assets;
   on macOS each program has both architectures), builds the Python package, the VS Code extension, the web build,
   the docs site, the AppImage and the recipes, then drafts the release with all the files, `SHA256SUMS` and the
   attestations.
5. Read the draft release on GitHub, add the notes from `CHANGELOG.md`, and publish it.

If something fails, delete the tag (`git push origin :refs/tags/v0.2.0`, and the draft release if there is one), fix
it on `master`, and tag again.

## Trying it without releasing (a dry run)

- *Actions → Release → Run workflow* (once the workflow is on `master`), or any pull request that changes
  `release.yml`, `tools/release/` or `mcp/pyproject.toml`: the same builds, packs and checks run, and the files are kept
  as the run's artifacts. The release job runs only for a tag, and nothing else uploads anywhere.
- On your machine:
  ```sh
  cmake -S . -B build -G Ninja && cmake --build build
  python3 tools/release/package.py --build build --platform macos-arm64 --out dist
  pip install --require-hashes -r tools/release/requirements.txt    # in a virtual environment
  python3 tools/release/build_mcp.py --out dist
  ```

## Checking a download

```sh
sha256sum -c SHA256SUMS --ignore-missing                               # shasum -a 256 -c on macOS
gh attestation verify affrescoui-0.2.0-linux-x86_64.tar.gz --repo RiccardoSottini/AffrescoUI
```

The macOS programs are not signed or notarized (that needs an Apple Developer ID). macOS quarantines what a browser
downloads; `xattr -dr com.apple.quarantine affrescoui-0.2.0-macos-universal` lets it run, as `README.txt` says.

## Packages

`packaging/` has a recipe for each package manager: Homebrew (a formula that builds from source), winget (the Windows
zip, as a portable install), AppImage (the Linux archive in one file), vcpkg (a port) and Conan 2 (a recipe with a
`test_package`). They are templates: `tools/release/packaging.py` fills in the version and the hashes of one release,
and the release job zips the filled copies as `packages-<version>.zip` next to the built AppImage. They are for people
to use themselves, from the release asset (`packaging/README.md` has how to try each one); the maintainer submits
them nowhere: no tap, `homebrew-core`, `microsoft/winget-pkgs`, vcpkg registry, ConanCenter or Conan remote. The
release job also adds `affrescoui-<version>-source.tar.gz`, which Homebrew, vcpkg and Conan build from; each installs
the same CMake targets that `find_package(AffrescoUI)` exports, and `packaging.py --check` keeps the Conan test in
step with them. Every run (also a dry run) fills the recipes, builds and starts the AppImage and runs
`winget validate`.

## Permissions the workflow uses

Every job starts with none and asks for what it needs: `contents: read` to check out; `contents: write` only in the
job that drafts the release; `id-token: write` only there (signing the attestations); `attestations: write` only for the attestations. Every action is pinned by its commit, and the Python
build tools by their hashes (`tools/release/requirements.txt`).
