# Releasing

How a release is built and signed, and how the maintainer gets the credentials the signing needs. Everything runs in
[`.github/workflows/release.yml`](.github/workflows/release.yml) when the maintainer pushes a `v*` tag; it makes a
**draft** GitHub Release, which the maintainer reads and publishes. Nothing is published anywhere else. How anyone
checks a download is in [`SECURITY.md`](SECURITY.md), "How releases are signed".

## What is signed, and how

| Download | Signed with | Checked in the workflow by |
|---|---|---|
| `AffrescoUI-<version>-macos-universal.dmg`: `AffrescoUI.app`, the Launcher | A Developer ID Application certificate, the hardened runtime and a secure timestamp; the app and the disk image are each notarized by Apple (`xcrun notarytool`, with an App Store Connect API key) and stapled (`xcrun stapler`), so they open with no warning, offline too | `codesign --verify --deep --strict`, `spctl --assess` (`execute` for the app, `open` for the image), `xcrun stapler validate`, on the build machine and again on the downloaded files |
| `affrescoui-<version>-macos-universal.tar.gz`: `affrescoui` and `affrescoui_launcher` | The same certificate; both notarized (in one submission with the app). A program outside an app or disk image can't carry a stapled ticket, so macOS asks Apple online the first time it runs (the archive's `README.txt` says so) | `codesign --verify --strict --check-notarization -R=notarized` |
| `affrescoui-<version>-windows-x86_64.zip`: `affrescoui.exe` and `affrescoui_launcher.exe` | Authenticode, through [SignPath](https://signpath.io)'s free code signing for open source; the certificate is SignPath Foundation's, so the publisher shown is "SignPath Foundation" | `signtool verify /pa /tw` and `Get-AuthenticodeSignature` (valid and timestamped), before packing and again on the downloaded zip |
| Every file of the release, `SHA256SUMS` too (Linux archive, AppImage, wheel, `.vsix`, zips, SBOM, the files above) | [Sigstore](https://www.sigstore.dev), keyless: `cosign sign-blob` with the run's GitHub OIDC identity; each file gets `<file>.sigstore.json`. No key exists to keep or lose | `cosign verify-blob` against `https://github.com/<repository>/.github/workflows/release.yml@refs/tags/<tag>` |

`SHA256SUMS`, the build-provenance attestations, the SBOM and the package recipes (winget's hash) are made from the
final files: signed, notarized and stapled. The Linux programs carry no signature of their own: Linux has no common
one, so Sigstore signs the archive and the AppImage.

### When it signs

Only a `v*` tag is signed. The signing secrets live in a GitHub **environment** named `release`, which only `v*`
tags may use and which waits for the maintainer's approval, so they exist only in a tag's run that he approved:

- **A tag:** the **Signing** job ([`tools/release/check_signing.py`](tools/release/check_signing.py)) says macOS and
  Windows are signed. Their build jobs ask for the `release` environment and wait for the maintainer's **Approve and
  deploy** (one click for both). Their first step checks that every secret of the platform is set; a missing one
  fails the run there, before anything is built, with `secret <NAME> missing in the release environment`. A tag is
  never released unsigned by accident.
- **To release a platform unsigned on purpose** (while an account is pending, say), name it in the repository variable
  `RELEASE_UNSIGNED_PLATFORMS` (`macos`, `windows`, or both): its build then doesn't ask for the environment, the
  summary and the release notes say it is unsigned. Delete the variable afterwards.
- **Anything else** (a pull request, a manual *Run workflow*, a fork): no job names the environment, so nothing waits
  for approval and no secret is read; everything is built, packed and checked as before, unsigned (the macOS app and
  disk image signed ad hoc), and the summary says why.
- **Once signing starts, every step must pass:** a signature, notarization or check that fails fails the run, and no
  draft is made.
- **Sigstore** needs no secret. It signs a tag's release files only, in the draft-release job (the only job with
  `id-token: write`), so the public transparency log holds releases and nothing else.

## The environment and its secrets

Create the environment **before the first signed tag** (a workflow that names an environment that doesn't exist
creates it, unprotected): **Settings → Environments → New environment**, named `release`
([GitHub's guide](https://docs.github.com/en/actions/managing-workflow-runs-and-deployments/managing-deployments/managing-environments-for-deployment)):

- **Deployment branches and tags:** *Selected branches and tags*, with one rule: ref type **Tag**, pattern `v*`.
  Nothing else (no branch), so no branch or pull request run can read the secrets.
- **Required reviewers:** yourself. Leave *Prevent self-review* off, since you push the tags you approve.
- **Environment secrets:** the ten below. Not as repository secrets: remove any that are set there.

| Secret | What it holds |
|---|---|
| `MACOS_CERTIFICATE_P12` | The Developer ID Application certificate with its private key, a `.p12` file, base64-encoded |
| `MACOS_CERTIFICATE_PASSWORD` | The password the `.p12` was exported with |
| `MACOS_SIGNING_IDENTITY` | The certificate's name, `Developer ID Application: <Name> (<TEAMID>)` |
| `APPLE_API_KEY_P8` | The App Store Connect API key, the `.p8` file, base64-encoded |
| `APPLE_API_KEY_ID` | The key's Key ID |
| `APPLE_API_ISSUER_ID` | The Issuer ID of the account's API keys |
| `SIGNPATH_API_TOKEN` | The API token of the SignPath user that submits signing requests |
| `SIGNPATH_ORGANIZATION_ID` | The SignPath organization's id |
| `SIGNPATH_PROJECT_SLUG` | The SignPath project's slug |
| `SIGNPATH_SIGNING_POLICY_SLUG` | The signing policy's slug (`release-signing` for a release) |

And one optional repository **variable** (not a secret): `RELEASE_UNSIGNED_PLATFORMS`, above.

The certificate is imported into a keychain the job makes for itself, with a random password, and deleted at the
end of the job; the API key is written to a file only while one submission runs ([`tools/release/macos.sh`](tools/release/macos.sh)).

## Getting the credentials

Follow the official pages linked at each step: they are the reference, and their menus change over time.

### macOS: Apple Developer Program, Developer ID, notarization

1. **Join the Apple Developer Program** ([enroll](https://developer.apple.com/programs/enroll/)), as an individual
   or an organization. It is a yearly paid membership. The name on the certificate (the one people see) is the
   enrolled person's or organization's.
2. **Create a Developer ID Application certificate** (only the account holder can):
   [Create Developer ID certificates](https://developer.apple.com/help/account/certificates/create-developer-id-certificates/).
   You make a certificate signing request on your Mac with Keychain Access
   ([Create a certificate signing request](https://developer.apple.com/help/account/certificates/create-a-certificate-signing-request)),
   upload it, download the certificate and open it, so it lands in your login keychain next to its private key.
3. **Export it as a `.p12`:** in Keychain Access, *My Certificates*, select `Developer ID Application: <Name> (<TEAMID>)`
   (with its private key) and export it as a `.p12` with a strong password. Then:
   ```sh
   base64 -i DeveloperIDApplication.p12 | pbcopy             # → MACOS_CERTIFICATE_P12
   security find-identity -v -p codesigning                  # the quoted name → MACOS_SIGNING_IDENTITY
   ```
   The password is `MACOS_CERTIFICATE_PASSWORD`. Delete the `.p12` file once the secret is saved.
4. **Create an App Store Connect API key** for notarization: in [App Store Connect](https://appstoreconnect.apple.com),
   *Users and Access → Integrations → App Store Connect API*, a team key
   ([Creating API keys for App Store Connect API](https://developer.apple.com/documentation/appstoreconnectapi/creating-api-keys-for-app-store-connect-api)).
   Give it the least access notarization accepts (*Developer* is the usual choice). The `.p8` downloads **once**:
   ```sh
   base64 -i AuthKey_<KEYID>.p8 | pbcopy                     # → APPLE_API_KEY_P8
   ```
   The Key ID is `APPLE_API_KEY_ID`, and the Issuer ID shown above the keys is `APPLE_API_ISSUER_ID`. Delete the
   `.p8` file once the secret is saved.

Background: [Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)
and [Customizing the notarization workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow).
The certificate lasts five years; a key or certificate that leaks is revoked from the same pages.

### Windows: SignPath Foundation

[SignPath Foundation](https://signpath.org) signs open-source projects for free, with its own certificate. Its
[conditions](https://signpath.org/terms) include an OSI licence (Apache-2.0 is one), programs built from the
project's own source on a CI system it trusts (GitHub Actions is one), product name and version in every signed
program (the Windows programs carry them: `cmake/AffrescoUIWindowsResources.cmake`), multi-factor authentication for
the team, a manual approval of every release, and a **code signing policy** on the project's home and download pages.

1. **Publish the code signing policy** the conditions ask for, on the README and the release page: a section titled
   "Code signing policy" that says *Free code signing provided by [SignPath.io](https://about.signpath.io), certificate
   by [SignPath Foundation](https://signpath.org)*, names the team's roles (committers and reviewers, approvers), and
   has a privacy statement (what the programs send over the network, and that they send nothing unless the person
   asks: the AI provider they configure, the connections an app declares).
2. **Apply** on [signpath.org](https://signpath.org) (*Apply*), for this repository.
3. **Once accepted**, in SignPath (their [GitHub guide](https://docs.signpath.io/trusted-build-systems/github)):
   - the organization has the trusted build system *GitHub.com*, linked to the project (it lets SignPath check that a
     request comes from this repository's workflow);
   - the project's artifact configuration (the default one) describes what the workflow uploads, a zip with the two
     programs, and checks their product name and version:
     ```xml
     <?xml version="1.0" encoding="utf-8"?>
     <artifact-configuration xmlns="http://signpath.io/artifact-configuration/v1">
       <parameters>
         <parameter name="version" required="true" />
       </parameters>
       <zip-file>
         <pe-file-set product-name="AffrescoUI" product-version="${version}">
           <include path="affrescoui_launcher.exe" />
           <include path="affrescoui.exe" />
           <for-each>
             <authenticode-sign />
           </for-each>
         </pe-file-set>
       </zip-file>
     </artifact-configuration>
     ```
     The workflow passes `version` (the release's version) with each request;
   - a CI user with an API token, allowed to submit to the signing policy: the token is `SIGNPATH_API_TOKEN`; the
     organization id, the project slug and the policy slug (`release-signing`) are the other three secrets.
4. **Approve each release:** the request waits in SignPath for an approver (the workflow waits up to 2 hours,
   `wait-for-completion-timeout-in-seconds`). Use the `release-signing` policy: SignPath's `test-signing` policy signs
   with a test certificate that Windows doesn't trust, so the workflow's check would fail the tag, as it should.

### Linux, and every file: Sigstore

Nothing to get. The release job asks GitHub for the run's OIDC token (`id-token: write`) and cosign signs with a
certificate that Sigstore's Fulcio issues for that identity; the signature is recorded in the Rekor transparency log.

## Making a release, step by step

1. **Accounts:** the three sections above.
2. **The `release` environment** with its protection rules and the ten secrets (above).
3. **Prepare the version:** the same version everywhere (`python3 tools/release/check_version.py` checks every
   place), the changelog, merged to `master`.
4. **Tag:** `git tag v<version> && git push origin v<version>`.
5. **Approve** the run's `release` deployment in the Actions tab (the macOS and Windows builds wait for it), then the
   SignPath request; wait for the run (notarization usually takes minutes).
6. **Read the draft release:** the files, `SHA256SUMS`, a `.sigstore.json` beside every file; open the `.dmg` on a
   Mac and the zip's programs on Windows, and check one of each kind as SECURITY.md shows. Then publish it.

If a tag run fails, nothing is drafted: fix the cause, delete the tag, and tag again.
