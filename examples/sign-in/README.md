# Sign in

Sign in, create an account, two-step verification, a password reset, provider sign-in with a made-up provider (Example ID), session expiry and an account page, in English, Italian and Arabic, light and dark. The `sign-in` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_launcher examples/sign-in/index.screen.json
./build/affrescoui test examples/sign-in/index.scenarios.json
```

## What it shows

- **Form inputs:** `secret` text areas with a show and hide button, `maxLength` on every field, `codeInput` (`accept` digits) for the one-time codes, the expressions `isEmail(text)` and `passwordStrength(text)`, and the `passwordMeter` component.
- **Components of its own:** `checkedField` (a field with its icon, label, description and `check` rule) and `strengthMeter`; the built-in `dialog` (forgot password, change password, keyboard shortcuts) and `toast`.
- **Validation:** `check` rules with their messages, shown per field once a form is sent, and given to the field as its accessible description.
- **Screens:** `screens` with `navigate`, Escape going back from the inner ones, and `focus` steps that move focus where the work goes on.
- **Strings:** a `strings` section in `en`, `it` and `ar`, `t()` with ICU plurals and selects, `"dir": "auto"` so Arabic runs right to left, and fields kept left to right where they must be (email, codes).
- **Commands and keys:** a `keys` section (Cmd+J theme, Cmd+L language, Shift+Cmd+H password, Shift+Cmd+Q sign out, Shift+Cmd+N slow network, F1 shortcuts), actions that describe themselves as commands (`title`, `description`), and the `commands` list shown with readable key names.
- **Time:** `every` for the session and resend countdowns, `after` for the fake server's delay (longer with Slow network on), `debounce` to close the toasts.
- **Net:** a `connections` entry for the fake OAuth 2 provider (no real client id or secret) with the `signIn` and `signOut` steps; scenarios play its answers as the `providerSignedIn` and `providerFailed` actions.
- **Accessibility:** roles (`button`, `link`, `switch`, `checkbox`, `radiogroup`, `alert`, `status`, `timer`, `heading`, `list`), names from labels, live regions and an announcer for loading and errors, busy and disabled buttons, keyboard use throughout.
- **Document:** `tokens` with light and dark values, `theme`, `computed`, `stateTypes`, `persist` (theme, language and "remember me" only, never a password).
- **Policy:** `index.policy.json` shows that agents may change the look and the texts, but not the provider connection, what is persisted, or the sign-in and sign-out steps.

| File | What it is |
|---|---|
| `index.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `index.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `index.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).

## How it holds together

- **Permissions:** none, on purpose: its policy file checks that without a `permissions` section a change applies, while the provider stays a capability that no change may point elsewhere, and nothing is stored beyond what it persists. Only the theme, the remembered choice and the language persist, never a password or a code. The `exampleId` connection names a made-up provider (`id.example.com`) with no secret; scenarios play its answers.
- **Threading and ownership:** it has no C++ of its own. The stock host owns the window and the screen, and runs the document on its UI thread.
- **Extension points:** a screen of the flow is an entry of `screens`; a language is a table of `strings`; `checkedField` and `strengthMeter` are its local components. Its demo accounts have no passwords.
- **Assets:** no pictures of its own; the fonts' licences are with them in `../shared/fonts`.
