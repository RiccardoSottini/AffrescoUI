# Sign in

Sign in, create an account, two-step verification, a password reset, provider sign-in with a made-up provider (Example ID), session expiry and an account page, in English, Italian and Arabic, light and dark. The `sign-in` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/sign-in/sign-in.screen.json
./build/affrescoui test examples/sign-in/sign-in.scenarios.json
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
- **Policy:** `sign-in.policy.json` shows that agents may change the look and the texts, but not the provider connection, what is persisted, or the sign-in and sign-out steps.

| File | What it is |
|---|---|
| `sign-in.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `sign-in.scenarios.json` | Its scenarios (`affrescoui test`; ctest `scenarios` plays them with the rendering check's runner) |
| `sign-in.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
