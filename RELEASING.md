# Releasing AffrescoUI

For the maintainer. A release is a tag `v<version>` that the maintainer pushes. `.github/workflows/release.yml` then
builds the downloads, drafts the GitHub Release, and publishes the MCP server to PyPI once the maintainer approves.
Agents and contributors never push tags, create releases or publish.

## What a release contains

| File | What |
|---|---|
| `affrescoui-<version>-macos-universal.tar.gz` | `affrescoui_host`, `affrescoui_studio` and `affrescoui` for macOS 13.3 or later, arm64 and x86_64 in one binary |
| `affrescoui-<version>-linux-x86_64.tar.gz` | The same for Linux x86_64 (built on Ubuntu 24.04: glibc 2.39 or later) |
| `affrescoui-<version>-windows-x86_64.zip` | The same for Windows, once Windows builds (roadmap 1.1); until then the release has no Windows file |
| `affrescoui_mcp-<version>-py3-none-any.whl`, `affrescoui_mcp-<version>.tar.gz` | The MCP server, also published to PyPI as `affrescoui-mcp` |
| `SHA256SUMS` | The checksums of all of them; each file also has a build-provenance attestation |

Each archive holds the three programs, `assets/` (the templates and their fonts, which the programs find next to
themselves), `LICENSE`, `NOTICE`, `THIRD_PARTY_NOTICES.md` and a short `README.txt`.

## Once: set up PyPI and the release environment

1. **The PyPI name.** Sign in to PyPI (with two-factor authentication), open *Your account → Publishing*, and add a
   **pending publisher**:
   - PyPI project name: `affrescoui-mcp`;
   - owner: `RiccardoSottini`, repository: `AffrescoUI`;
   - workflow name: `release.yml`;
   - environment name: `release`.

   The first publish creates the project; nobody else can take the name while the pending publisher is there. No
   token is stored anywhere: PyPI trusts the workflow run itself (trusted publishing).
2. **The environment.** On GitHub, *Settings → Environments → New environment* `release`:
   - *Required reviewers*: yourself, so every publish waits for your approval;
   - *Deployment branches and tags*: *Selected*, with the tag rule `v*`.
3. **The tags.** *Settings → Rules → Rulesets*: a tag ruleset for `v*` that only you can create, update or delete.

## Every release

1. Pick the version (semantic: `0.2.0`) and write it in the three places `tools/release/check_version.py` reads:
   `CMakeLists.txt` (`project(AffrescoUI VERSION ...)`), `mcp/pyproject.toml` (`version`) and
   `mcp/affrescoui_mcp/server.py` (`serverInfo`). Check: `python3 tools/release/check_version.py`.
2. Update `CHANGELOG.md` and the README if needed, merge to `master`, and wait for CI to be green.
3. Tag and push:
   ```sh
   git tag -a v0.2.0 -m "AffrescoUI 0.2.0"
   git push origin v0.2.0
   ```
4. Watch *Actions → Release*. It checks the tag against the version, builds and packs each system, checks each
   download on a clean machine (the tool checks every template; on Linux the host draws a frame from its own assets;
   on macOS each program has both architectures), builds the Python package, then drafts the release with the files,
   `SHA256SUMS` and the attestations.
5. The *Publish the MCP server to PyPI* job waits for you: approve it in the run (the `release` environment).
6. Read the draft release on GitHub, add the notes from `CHANGELOG.md`, and publish it.

If something fails, delete the tag (`git push origin :refs/tags/v0.2.0`, and the draft release if there is one), fix
it on `master`, and tag again. A version published to PyPI can't be published again: use the next one.

## Trying it without releasing (a dry run)

- *Actions → Release → Run workflow* (once the workflow is on `master`), or any pull request that changes
  `release.yml`, `tools/release/` or `mcp/pyproject.toml`: the same builds, packs and checks run, and the files are kept
  as the run's artifacts. The release and PyPI jobs run only for a tag.
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
`test_package`). They are templates: `tools/release/packaging.py` fills in the version and the hashes of one release.
The release job adds `affrescoui-<version>-source.tar.gz` (`git archive` of the tag), which Homebrew, vcpkg and Conan
build from; each installs the same CMake targets that `find_package(AffrescoUI)` exports, and `packaging.py --check`
keeps the Conan test in step with them.

After the release is published (the URLs in the recipes are its downloads):

```sh
gh release download v0.2.0 --repo RiccardoSottini/AffrescoUI --pattern SHA256SUMS --pattern '*-source.tar.gz' --dir dist
python3 tools/release/packaging.py --version 0.2.0 --sums dist/SHA256SUMS    # writes dist/packages/<recipe>/
```

Then the maintainer submits each one by hand, reviewed first: the formula to a tap or homebrew-core, the winget
manifests to `microsoft/winget-pkgs`, the port to a vcpkg registry, the recipe to ConanCenter or a Conan remote, and
the AppImage as a release file. No workflow submits or publishes a package. A dry run (*Run workflow*) fills the
recipes for its own files, builds and starts the AppImage, runs `winget validate` and keeps them as the `packages`
artifact; `packaging/README.md` has how to try each recipe on your machine.

## Permissions the workflow uses

Every job starts with none and asks for what it needs: `contents: read` to check out; `contents: write` only in the
job that drafts the release; `id-token: write` only there (signing the attestations) and in the PyPI job (trusted
publishing); `attestations: write` only for the attestations. Every action is pinned by its commit, and the Python
build tools by their hashes (`tools/release/requirements.txt`).
