/// @file
/// @brief The intelligence settings, the catalogue of providers and where keys are kept (Settings.hpp).

#include "intelligence/Settings.hpp"

#include "Assets.hpp"

#include <affresco/net/SignIn.hpp>

#include <atomic>
#include <fstream>
#include <sstream>
#include <stdexcept>
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
	for(const json& model : value.value("models", json::array()))
		if(model.is_object() && !text(model, "id").empty())
			provider.models.push_back({text(model, "id"), text(model, "name", text(model, "id"))});
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
	if(!std::filesystem::exists(dataDir, error)) {
		std::filesystem::create_directories(dataDir, error);
#ifndef _WIN32
		::chmod(dataDir.c_str(), 0700); // the data folder is this user's: history, state and these settings
#endif
	}
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
	endpoint.baseUrl = choice != settings.providers.end() && !choice->second.baseUrl.empty() ? choice->second.baseUrl
	                                                                                         : provider->baseUrl;
	while(endpoint.baseUrl.ends_with('/')) endpoint.baseUrl.pop_back();
	if(id == settings.provider && !settings.model.empty()) endpoint.model = settings.model;
	else if(choice != settings.providers.end() && !choice->second.model.empty()) endpoint.model = choice->second.model;
	else endpoint.model = provider->model;
	return endpoint;
}

std::string keyName(const std::string& providerId) { return "intelligence/" + providerId; }

std::shared_ptr<affresco::SecretStore> systemKeys() {
	static const std::shared_ptr<affresco::SecretStore> store = affresco::net::systemSecretStore("AffrescoUI");
	return store;
}

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
