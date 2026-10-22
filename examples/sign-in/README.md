# Sign in

Three screens (sign in, create an account, signed in), validation with `check`, a reset dialog, light and dark tokens. The `sign-in` template: a screen document with no C++ of its own, which the stock host runs.

```sh
./build/affrescoui_host examples/sign-in/sign-in.screen.json
./build/affrescoui test examples/sign-in/sign-in.scenarios.json
```

| File | What it is |
|---|---|
| `sign-in.policy.json` | Its approval rules' example revisions (`affrescoui policy test`) |
| `sign-in.scenarios.json` | Its scenarios (`affrescoui test`; the rendering check plays them too) |
| `sign-in.screen.json` | The screen document |

Its fonts are in [`../shared/fonts`](../shared/fonts/README.md).
