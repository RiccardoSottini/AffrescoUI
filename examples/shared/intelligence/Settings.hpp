#pragma once

/// @file
/// @brief The intelligence settings every example app shares: which provider and model the assistant uses
/// (`<data dir>/intelligence.json`, never a key), the catalogue of providers (providers.json) and where each
/// provider's key is kept (the system's keychain).

#include <affresco/core/Json.hpp>
#include <affresco/core/SecretStore.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace examples::intelligence {

/// @brief How a provider is spoken to.
enum class Kind {
	Anthropic,        ///< The Messages API, with an x-api-key.
	OpenAI,           ///< Chat Completions, with a Bearer key.
	OpenAICompatible, ///< Chat Completions at a base URL the person gives, a key optional.
	ClaudeCode,       ///< The person's own Claude Code command line, run with no tools.
};

/// @brief A model a provider offers.
struct Model {
	std::string id;   ///< What the API is sent, e.g. "claude-sonnet-5-5".
	std::string name; ///< For people, e.g. "Claude Sonnet 5.5".
};

/// @brief One provider of the catalogue (providers.json).
struct Provider {
	std::string id;             ///< E.g. "anthropic".
	std::string name;           ///< For people, e.g. "Anthropic (Claude)".
	Kind kind = Kind::OpenAI;   ///< How it is spoken to.
	std::string baseUrl;        ///< Its default base URL ("" for claude-code).
	bool needsKey = false;      ///< Whether it can't work without a key.
	std::string keyHelp;        ///< Where to get a key (or the program), for people.
	std::string model;          ///< The model chosen when the person hasn't chosen one.
	std::vector<Model> models;  ///< The fallback list, when the live one can't be read.
};

/// @brief The catalogue of providers.
struct Catalog {
	std::vector<Provider> providers; ///< In the order they are shown.

	/// @brief Finds a provider.
	/// @param id Its id.
	/// @return It, or nullptr.
	[[nodiscard]] const Provider* find(const std::string& id) const;
	/// @brief Reads a catalogue file.
	/// @param file providers.json.
	/// @return The catalogue.
	/// @throws std::runtime_error If it can't be read or a provider is malformed.
	[[nodiscard]] static Catalog load(const std::filesystem::path& file);
	/// @brief The examples' own catalogue (examples/shared/intelligence/providers.json, or the release's copy).
	/// @return It.
	/// @throws std::runtime_error If it can't be read.
	[[nodiscard]] static Catalog standard();
};

/// @brief What the person chose for one provider.
struct ProviderChoice {
	std::string baseUrl; ///< Their base URL, or "" for the provider's default.
	std::string model;   ///< Their model, or "" for the provider's default.
};

/// @brief The settings in `<data dir>/intelligence.json`: never a key.
struct Settings {
	std::string provider;                            ///< The provider in use, or "" for none.
	std::string model;                               ///< Its model, or "" for its default.
	std::map<std::string, ProviderChoice> providers; ///< What was chosen per provider.

	/// @brief The settings file in a data folder.
	/// @param dataDir The folder (Screen::defaultDataDirectory() or --data).
	/// @return `<dataDir>/intelligence.json`.
	[[nodiscard]] static std::filesystem::path file(const std::filesystem::path& dataDir);
	/// @brief Reads the settings; a missing or malformed file gives none.
	/// @param dataDir The data folder.
	/// @return The settings.
	[[nodiscard]] static Settings load(const std::filesystem::path& dataDir);
	/// @brief Writes the settings atomically (a new file renamed over the old one), readable by this user only.
	/// @param dataDir The data folder, created if needed.
	/// @throws std::runtime_error If it can't be written.
	void save(const std::filesystem::path& dataDir) const;

	/// @brief The settings as JSON.
	/// @return `{ provider, model, providers: { id: { baseUrl?, model? } } }`.
	[[nodiscard]] affresco::json toJson() const;
	/// @brief Reads settings from JSON, ignoring what is malformed (and any field but those of toJson()).
	/// @param value The JSON.
	/// @return The settings.
	[[nodiscard]] static Settings fromJson(const affresco::json& value);
};

/// @brief A provider as it is configured: the catalogue's entry with the person's choices.
struct Endpoint {
	const Provider* provider = nullptr; ///< The catalogue's entry.
	std::string baseUrl;                ///< The base URL in use, without a trailing "/".
	std::string model;                  ///< The model in use.
	std::string key;                    ///< The key, read from the keychain at the moment it is needed; may be "".
};

/// @brief The provider the settings name, with its base URL and model (no key).
/// @param catalog The catalogue.
/// @param settings The settings.
/// @param providerId A provider to use instead of the settings' one, or "".
/// @return It, or nullopt if no (known) provider is chosen.
[[nodiscard]] std::optional<Endpoint> endpointOf(const Catalog& catalog, const Settings& settings,
                                                 const std::string& providerId = {});

/// @brief The keychain entry of a provider's key.
/// @param providerId The provider.
/// @return E.g. "intelligence/anthropic".
[[nodiscard]] std::string keyName(const std::string& providerId);

/// @brief The examples' secret store: the system's keychain, under the service "AffrescoUI".
/// @return It (one per process).
[[nodiscard]] std::shared_ptr<affresco::SecretStore> systemKeys();

/// @brief A kind's name in providers.json.
/// @param kind The kind.
/// @return E.g. "openai-compatible".
[[nodiscard]] std::string toString(Kind kind);

} // namespace examples::intelligence
