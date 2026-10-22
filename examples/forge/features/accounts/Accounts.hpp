#pragma once

/// @file
/// @brief Forge's accounts, on the host's side: where the sign-in is kept, and which origins the accounts feature
/// (accounts.json) may reach.

namespace affresco {
class Screen;
} // namespace affresco

namespace forge {

/// @brief Allows the document server's and the identity provider's origins, and keeps the sign-in in the system's
/// secret store (the macOS Keychain, the Windows Credential Manager, the Secret Service) so Forge stays signed in.
/// @param screen The screen.
/// @param remember false keeps the sign-in in memory for this run only (a screenshot, a test).
void registerAccounts(affresco::Screen& screen, bool remember);

} // namespace forge
