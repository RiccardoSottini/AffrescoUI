# Sign-in: parity matrix

What a first-class identity product's account flows do, and whether this example does it. The brand is the invented
"Example ID"; nothing here copies a real company's names, logos, colours, wording or layout, and every credential is
fake and obviously so (`@example.com`, `555-01xx` numbers, the secret `EXAMPLEEXAMPLE23`).

Legend: **yes** shown and tested by a scenario; **part** shown, with a gap noted; **no** not yet.

| # | Flow | Feature | State |
|---|------|---------|-------|
| 1 | Sign in | Identifier first (email, then password) | yes |
| 1 | Sign in | Show password | yes |
| 1 | Sign in | Caps-lock warning | yes |
| 1 | Sign in | Remember this device | yes |
| 1 | Sign in | Passkey | yes |
| 1 | Sign in | Email code with a 6-digit input | yes |
| 1 | Sign in | Social buttons (made-up providers) | yes |
| 2 | Create account | Strength meter | yes |
| 2 | Create account | Rules checklist | yes |
| 2 | Create account | Terms | yes |
| 2 | Create account | Email verification | yes |
| 2 | Create account | Username availability with a spinner | yes |
| 2 | Create account | Invitations | yes |
| 3 | Two-step | QR from a fake secret | yes |
| 3 | Two-step | Backup codes | yes |
| 3 | Two-step | SMS fallback | yes |
| 3 | Two-step | Trusted devices | yes |
| 4 | Recovery | Forgot password | yes |
| 4 | Recovery | Reset with token expiry | yes |
| 4 | Recovery | Locked account | yes |
| 4 | Recovery | Session expired | yes |
| 4 | Recovery | Step-up (confirm the password again) | yes |
| 5 | Account | Profile | yes |
| 5 | Account | Emails | yes |
| 5 | Account | Password change | yes |
| 5 | Account | Passkeys | yes |
| 5 | Account | Sessions | yes |
| 5 | Account | Connected accounts | yes |
| 5 | Account | Security log | yes |
| 5 | Account | Delete by typing | yes |
| 6 | Throughout | Inline accessible errors | yes |
| 6 | Throughout | Loading and disabled states | yes |
| 6 | Throughout | Notices that fit their message and leave with their step | yes |
| 7 | Add-on | Assistant chat, themed with the app's tokens, suggestions about this app | yes |
| 6 | Throughout | Success screens | part |
| 6 | Throughout | Responsive card | part |

Start: 8 yes, 5 part, 21 no (34 features). Now: 34 yes, 2 part, 0 no (36 features).

## Metrics

| | Start | Now |
|---|---|---|
| Document tokens (`index.screen.json`, about 4.7 bytes a token) | 17,818 | 47,028 |
| C++ lines in the example | 0 | 0 |
| Host registrations | 0 | 0 |
| Rendering-check frames | 8 | 34 |
| App scenarios | 44 | 77 |
