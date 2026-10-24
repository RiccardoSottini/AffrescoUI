/// @file
/// @brief The intelligence settings, the catalogue of providers and where keys are kept (Settings.hpp).

#include "intelligence/Settings.hpp"

#include "Assets.hpp"
#include "intelligence/Http.hpp"

#include <affresco/net/SignIn.hpp>
#include <affresco/net/Url.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace examples::intelligence {

using affresco::json;

namespace {

/// @brief A kind by its name in providers.json.
/// @param name The name.
/// @return The kind.
/// @throws std::runtime_error If it isn't one.
Kind kindOf(const std::string& name) {
	if(name == "anthropic") return Kind::Anthropic;
	if(name == "openai") return Kind::OpenAI;
	if(name == "openai-compatible") return Kind::OpenAICompatible;
	if(name == "claude-code") return Kind::ClaudeCode;
	throw std::runtime_error("providers.json: unknown kind '" + name + "'");
}

/// @brief A text member of an object, or a default when it is missing or not text.
/// @param object The object.
/// @param key The member.
/// @param otherwise The default.
/// @return It.
std::string text(const json& object, const char* key, const std::string& otherwise = {}) {
	const auto found = object.find(key);
	return found != object.end() && found->is_string() ? found->get<std::string>() : otherwise;
}

/// @brief Reads one provider of the catalogue.
/// @param value Its JSON.
/// @return It.
/// @throws std::runtime_error If it has no id or name, or an unknown kind.
Provider providerOf(const json& value) {
	if(!value.is_object()) throw std::runtime_error("providers.json: a provider isn't an object");
	Provider provider;
	provider.id = text(value, "id");
	provider.name = text(value, "name");
	if(provider.id.empty() || provider.name.empty())
		throw std::runtime_error("providers.json: a provider needs an id and a name");
	provider.kind = kindOf(text(value, "kind"));
	provider.baseUrl = text(value, "baseUrl");
	provider.needsKey = value.value("needsKey", false);
	provider.keyHelp = text(value, "keyHelp");
	provider.model = text(value, "model");
	provider.shown = value.value("shown", true);
	provider.fixedModels = value.value("fixedModels", false);
	for(const json& model : value.value("models", json::array()))
		if(model.is_object() && !text(model, "id").empty()) {
			const auto description = model.find("description");
			provider.models.push_back({text(model, "id"), text(model, "name", text(model, "id")),
			                           description != model.end() && description->is_object() ? *description : json()});
		}
	return provider;
}

/// @brief Writes a file readable by this user only, replacing what is there at once: a new file renamed over it.
/// @param file The file; its folder must exist.
/// @param content What it holds.
/// @throws std::runtime_error If it can't be written.
void writeAtomically(const std::filesystem::path& file, const std::string& content) {
	static std::atomic<unsigned> counter{0};
#ifdef _WIN32
	const std::filesystem::path temporary =
	    file.string() + "." + std::to_string(_getpid()) + "-" + std::to_string(counter++) + ".tmp";
	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
		if(!(out << content) || !out.flush()) throw std::runtime_error("can't write " + temporary.string());
	}
#else
	const std::filesystem::path temporary =
	    file.string() + "." + std::to_string(getpid()) + "-" + std::to_string(counter++) + ".tmp";
	const int descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
	if(descriptor < 0) throw std::runtime_error("can't create " + temporary.string());
	std::size_t written = 0;
	while(written < content.size()) {
		const ssize_t count = ::write(descriptor, content.data() + written, content.size() - written);
		if(count <= 0) break;
		written += static_cast<std::size_t>(count);
	}
	const bool synced = ::fsync(descriptor) == 0;
	::close(descriptor);
	if(written != content.size() || !synced) {
		std::filesystem::remove(temporary);
		throw std::runtime_error("can't write " + temporary.string());
	}
#endif
	std::error_code error;
	std::filesystem::rename(temporary, file, error);
	if(error) {
		std::filesystem::remove(temporary, error);
		throw std::runtime_error("can't replace " + file.string());
	}
}

} // namespace

const Provider* Catalog::find(const std::string& id) const {
	for(const Provider& provider : providers)
		if(provider.id == id) return &provider;
	return nullptr;
}

bool Provider::lists(const std::string& wanted) const {
	return std::ranges::any_of(models, [&wanted](const Model& offered) { return offered.id == wanted; });
}

std::string Provider::modelFor(const std::string& requested, const std::string& otherwise) const {
	return !requested.empty() && lists(requested) ? requested : otherwise;
}

Catalog Catalog::load(const std::filesystem::path& file) {
	std::ifstream in(file);
	if(!in) throw std::runtime_error("can't read " + file.string());
	const json document = json::parse(in, nullptr, false);
	if(!document.is_object() || !document.contains("providers") || !document["providers"].is_array())
		throw std::runtime_error(file.string() + ": expected { \"providers\": [...] }");
	Catalog catalog;
	for(const json& provider : document["providers"]) catalog.providers.push_back(providerOf(provider));
	return catalog;
}

Catalog Catalog::standard() { return load(assetsDirectory() / "shared" / "intelligence" / "providers.json"); }

std::filesystem::path Settings::file(const std::filesystem::path& dataDir) { return dataDir / "intelligence.json"; }

Settings Settings::load(const std::filesystem::path& dataDir) {
	std::ifstream in(file(dataDir));
	if(!in) return {};
	return fromJson(json::parse(in, nullptr, false));
}

void Settings::save(const std::filesystem::path& dataDir) const {
	std::error_code error;
	if(!std::filesystem::exists(dataDir, error)) std::filesystem::create_directories(dataDir, error);
#ifndef _WIN32
	::chmod(dataDir.c_str(), 0700); // the data folder is this user's: history, state and these settings, every save
#endif
	writeAtomically(file(dataDir), toJson().dump(1, '\t') + "\n");
}

json Settings::toJson() const {
	json choices = json::object();
	for(const auto& [id, choice] : providers) {
		json entry = json::object();
		if(!choice.baseUrl.empty()) entry["baseUrl"] = choice.baseUrl;
		if(!choice.model.empty()) entry["model"] = choice.model;
		choices[id] = entry;
	}
	return {{"provider", provider}, {"model", model}, {"providers", choices}};
}

Settings Settings::fromJson(const json& value) {
	Settings settings;
	if(!value.is_object()) return settings;
	settings.provider = text(value, "provider");
	settings.model = text(value, "model");
	const auto choices = value.find("providers");
	if(choices != value.end() && choices->is_object())
		for(const auto& [id, choice] : choices->items())
			if(choice.is_object()) settings.providers[id] = {text(choice, "baseUrl"), text(choice, "model")};
	return settings;
}

std::optional<Endpoint> endpointOf(const Catalog& catalog, const Settings& settings, const std::string& providerId) {
	const std::string id = providerId.empty() ? settings.provider : providerId;
	const Provider* provider = catalog.find(id);
	if(provider == nullptr) return std::nullopt;
	Endpoint endpoint;
	endpoint.provider = provider;
	const auto choice = settings.providers.find(id);
	// Only a compatible server's base URL is the person's: the API providers' keys go to the catalogue's URL alone
	const bool own = provider->kind == Kind::OpenAICompatible && choice != settings.providers.end() &&
	                 !choice->second.baseUrl.empty();
	endpoint.baseUrl = own ? choice->second.baseUrl : provider->baseUrl;
	while(endpoint.baseUrl.ends_with('/')) endpoint.baseUrl.pop_back();
	if(id == settings.provider && !settings.model.empty()) endpoint.model = settings.model;
	else if(choice != settings.providers.end() && !choice->second.model.empty()) endpoint.model = choice->second.model;
	else endpoint.model = provider->model;
	if(provider->fixedModels && !provider->lists(endpoint.model))
		endpoint.model = provider->model; // e.g. a retired one
	return endpoint;
}

std::string cleanBaseUrl(std::string url) {
	while(url.ends_with('/')) url.pop_back();
	const std::optional<affresco::net::Url> parsed = affresco::net::parseUrl(url);
	if(url.size() > 512 || !parsed || (parsed->scheme != "https" && parsed->scheme != "http"))
		throw std::runtime_error("the base URL must be an https:// URL, or http:// on this computer");
	if(parsed->scheme == "http" && !affresco::net::isLoopback(parsed->host))
		throw std::runtime_error(
		    "http:// is only for a server on this computer (127.0.0.1 or localhost): use https://");
	if(url.find_first_of("?#") != std::string::npos) throw std::runtime_error("the base URL can't have a query");
	return url;
}

std::optional<std::string> checkBaseUrl(const Endpoint& endpoint) {
	try {
		(void)cleanBaseUrl(endpoint.baseUrl);
		return std::nullopt;
	} catch(const std::exception& error) {
		return endpoint.provider->name + ": " + error.what() + " (check it in the AffrescoUI Launcher's Settings)";
	}
}

std::string keyName(const std::string& providerId) { return "intelligence/" + providerId; }

std::string readKey(affresco::SecretStore& keys, const Endpoint& endpoint) {
	if(endpoint.provider == nullptr || endpoint.provider->kind == Kind::ClaudeCode) return "";
	const std::string stored = keys.read(keyName(endpoint.provider->id)).value_or("");
	if(endpoint.provider->kind != Kind::OpenAICompatible) return stored;
	// A compatible server's key is kept with the origin it was given for, and goes to that origin only
	const json bound = json::parse(stored, nullptr, false);
	const std::optional<std::string> origin = originOf(endpoint.baseUrl);
	if(!bound.is_object() || !origin || text(bound, "origin") != *origin) return "";
	return text(bound, "key");
}

bool writeKey(affresco::SecretStore& keys, const Endpoint& endpoint, const std::string& key) {
	if(endpoint.provider == nullptr || endpoint.provider->kind == Kind::ClaudeCode) return false;
	const std::string name = keyName(endpoint.provider->id);
	if(endpoint.provider->kind != Kind::OpenAICompatible) return keys.write(name, key);
	const std::optional<std::string> origin = originOf(endpoint.baseUrl);
	return origin && keys.write(name, json{{"origin", *origin}, {"key", key}}.dump());
}

std::shared_ptr<affresco::SecretStore> systemKeys() {
	static const std::shared_ptr<affresco::SecretStore> store = []() -> std::shared_ptr<affresco::SecretStore> {
		// For tests only: AFFRESCOUI_KEYS=memory keeps every key in this process's memory, never in the keychain
		const char* chosen = std::getenv("AFFRESCOUI_KEYS"); // NOLINT(concurrency-mt-unsafe): read once, at first use
		if(chosen != nullptr && std::string_view(chosen) == "memory")
			return std::make_shared<affresco::MemorySecretStore>();
		return affresco::net::systemSecretStore("AffrescoUI");
	}();
	return store;
}

bool hiddenProvidersAllowed() {
	static const bool allowed = [] {
		const char* chosen = std::getenv("AFFRESCOUI_PROVIDERS"); // NOLINT(concurrency-mt-unsafe): read once
		return chosen != nullptr && std::string_view(chosen) == "all";
	}();
	return allowed;
}

bool persistent(const affresco::SecretStore& keys) { return keys.name() != "memory"; }

std::string kindName(Kind kind) {
	switch(kind) {
	case Kind::Anthropic:
		return "anthropic";
	case Kind::OpenAI:
		return "openai";
	case Kind::OpenAICompatible:
		return "openai-compatible";
	case Kind::ClaudeCode:
		return "claude-code";
	}
	return "openai";
}

} // namespace examples::intelligence
