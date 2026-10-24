#pragma once

/// @file
/// @brief The examples' intelligence, for a host: the natives of the settings UI (the `intelligence.*` calls the
/// AffrescoUI Launcher's Settings use, examples/shared/addons/intelligence) and the agent every app's assistant
/// talks to (ProviderAgent), attached when a provider is set up. Application layer: nothing here is in the library.

#include "intelligence/Agent.hpp"
#include "intelligence/Http.hpp"
#include "intelligence/Settings.hpp"

#include <affresco/affresco.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace examples::intelligence {

/// @brief What the settings' natives use; the defaults are the system's.
struct CallsOptions {
	std::shared_ptr<affresco::SecretStore> keys; ///< Where keys are kept; null: systemKeys().
	NetworkFactory network = systemNetwork();    ///< Makes the HTTP client of a models list or a test.
	std::function<bool()> claudeInstalled;       ///< Whether claude-code can run; empty: findClaude().
	ClaudeRunner claude;                         ///< Runs claude-code for a test; empty: the installed one.
	std::function<void()> changed;               ///< Called on the main thread after the settings changed.
	bool hidden = false; ///< Offer the providers providers.json hides too (for tests of the other providers).
};

/// @brief Registers the settings' natives on a screen. Only the AffrescoUI Launcher's host does: a call step any
/// other screen (or a revision) adds can't change the provider, its base URL or its key.
///
/// - `intelligence.providers` → `{ "current": { provider, model }, "keyStore", "persistent", "providers": [{ id,
///   name, kind, needsKey, takesKey, hasKey, available, baseUrl, editableUrl, model, keyHelp, models }] }`; `hasKey`
///   is a boolean, the key never leaves the keychain; `persistent` is false when the store keeps keys in memory only
///   (no keychain); `models` is the fallback list.
/// - `intelligence.models` `{ provider }` (slow, on a worker thread) → `[{ id, name }]`: the provider's live list with
///   its saved base URL and key, else the catalogue's fallback.
/// - `intelligence.stageKey` `{ provider, key }` → `true`: keeps a key typed in the form in this process's memory
///   only (the field isn't bound to the state), for the next save of that provider; "" forgets it.
/// - `intelligence.save` `{ provider, model?, baseUrl?, key? }` → as `intelligence.providers`: makes it the provider
///   in use; a key given (or staged) goes to the keychain and is forgotten; a base URL that changes erases the key
///   saved for the old one, and a compatible server's key is kept with its origin and sent to that origin only
///   (a key never goes to a server it wasn't given for).
/// - `intelligence.forget` `{ provider }` → as `intelligence.providers`: erases its key.
/// - `intelligence.openKeyHelp` `{ provider }` → `true`: opens the catalogue's page for getting that provider's key.
/// - `intelligence.test` (slow) → `{ ok, message }`: one tiny request to the provider in use, with its model.
/// @param screen The screen.
/// @param dataDir The data folder of intelligence.json.
/// @param options What they use.
void registerCalls(affresco::Screen& screen, const std::filesystem::path& dataDir, CallsOptions options = {});

/// @brief The agent of a host: made and connected to its screen (author "agent") once a provider is set up.
class HostAssistant {
public:
	/// @brief Nothing is attached yet.
	/// @param executor The main thread (the Application); outlives this.
	/// @param dataDir The data folder of intelligence.json.
	HostAssistant(affresco::Executor& executor, std::filesystem::path dataDir);

	/// @brief Connects the agent to the screen if intelligence.json names a provider (once: later calls do nothing
	/// once it is connected). Declare the HostAssistant before the screen: the channel must outlive it.
	/// @param screen The screen.
	/// @return Whether the agent is connected.
	bool attach(affresco::Screen& screen);

	/// @brief Registers the settings' natives (registerCalls()). The agent isn't attached to this screen, then or
	/// when a provider is saved: the screen that changes the provider has no agent.
	/// @param screen The screen (the Launcher).
	void enableSettings(affresco::Screen& screen);

private:
	affresco::Executor& executor_;         ///< The main thread.
	std::filesystem::path dataDir_;        ///< See the constructor.
	std::unique_ptr<ProviderAgent> agent_; ///< The agent, once connected.
};

/// @brief Opens the assistant and sends a request as the person's first message once the window is up (the host's
/// --ask): runs the add-on's `assistant.start` with `{ "text": … }`.
/// @param executor The main thread.
/// @param screen The screen; outlives the posted task's run (the Application's loop).
/// @param window Its window.
/// @param text The request.
void askOnStart(affresco::Executor& executor, affresco::Screen& screen, affresco::Window& window, std::string text);

} // namespace examples::intelligence
