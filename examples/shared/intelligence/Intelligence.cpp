/// @file
/// @brief The settings' natives and the host's agent (Intelligence.hpp).

#include "intelligence/Intelligence.hpp"

#include "intelligence/ClaudeCode.hpp"
#include "intelligence/Providers.hpp"
#include "intelligence/ScreenQueries.hpp"

#include <affresco/net/SignIn.hpp>

#include <cctype>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace examples::intelligence {

using affresco::json;

namespace {

/// @brief What the natives share.
struct Shared {
	std::filesystem::path dataDir;             ///< Where intelligence.json is.
	Catalog catalog;                           ///< The providers.
	CallsOptions options;                      ///< See registerCalls().
	std::mutex mutex;                          ///< Guards staged.
	std::map<std::string, std::string> staged; ///< Keys typed and not yet saved, by provider. Memory only.
};

/// @brief A text argument.
/// @param arguments The arguments.
/// @param key Its name.
/// @return It, or "".
std::string argument(const json& arguments, const char* key) {
	return arguments.is_object() && arguments.contains(key) && arguments[key].is_string()
	           ? arguments[key].get<std::string>()
			   : "";
}

/// @brief A provider named in the arguments.
/// @param shared The natives' parts.
/// @param arguments The arguments.
/// @return It.
/// @throws std::runtime_error If there is no such provider.
const Provider& providerOf(const Shared& shared, const json& arguments) {
	const Provider* provider = shared.catalog.find(argument(arguments, "provider"));
	if(provider == nullptr) throw std::runtime_error("no such provider");
	return *provider;
}

/// @brief A key as pasted: without the spaces around it; refused if it holds what a key can't.
/// @param key The key.
/// @return It.
/// @throws std::runtime_error If it isn't one (the message doesn't quote it).
std::string cleanKey(std::string key) {
	while(!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
	std::size_t start = 0;
	while(start < key.size() && std::isspace(static_cast<unsigned char>(key[start]))) ++start;
	key.erase(0, start);
	if(key.size() > 4096) throw std::runtime_error("the key is too long");
	for(const char c : key)
		if(c <= ' ' || c > '~') throw std::runtime_error("the key has characters a key can't have");
	return key;
}

/// @brief A URL without the "/" it ends with.
/// @param url The URL.
/// @return It.
std::string withoutSlash(std::string url) {
	while(url.ends_with('/')) url.pop_back();
	return url;
}

/// @brief Whether a provider can run here: claude-code only if its command line is installed.
/// @param shared The natives' parts.
/// @param provider The provider.
/// @return Whether it can.
bool available(const Shared& shared, const Provider& provider) {
	if(provider.kind != Kind::ClaudeCode) return true;
	return shared.options.claudeInstalled ? shared.options.claudeInstalled() : findClaude().has_value();
}

/// @brief A provider's fallback models.
/// @param provider The provider.
/// @return `[{ id, name }]`.
json fallbackOf(const Provider& provider) {
	json list = json::array();
	for(const Model& model : provider.models) list.push_back({{"id", model.id}, {"name", model.name}});
	return list;
}

/// @brief What intelligence.providers answers.
/// @param shared The natives' parts.
/// @return It.
json providersJson(const Shared& shared) {
	const Settings settings = Settings::load(shared.dataDir);
	json list = json::array();
	for(const Provider& provider : shared.catalog.providers) {
		const std::optional<Endpoint> endpoint = endpointOf(shared.catalog, settings, provider.id);
		const bool hasKey = endpoint && !readKey(*shared.options.keys, *endpoint).empty();
		list.push_back({{"id", provider.id},
		                {"name", provider.name},
		                {"kind", kindName(provider.kind)},
		                {"needsKey", provider.needsKey},
		                {"takesKey", provider.kind != Kind::ClaudeCode},
		                {"hasKey", hasKey},
		                {"available", available(shared, provider)},
		                {"baseUrl", endpoint ? endpoint->baseUrl : provider.baseUrl},
		                {"editableUrl", provider.kind == Kind::OpenAICompatible},
		                {"model", endpoint ? endpoint->model : provider.model},
		                {"keyHelp", provider.keyHelp},
		                {"models", fallbackOf(provider)}});
	}
	const std::optional<Endpoint> current = endpointOf(shared.catalog, settings);
	return {{"current", {{"provider", current ? current->provider->id : ""}, {"model", current ? current->model : ""}}},
	        {"keyStore", shared.options.keys->name()},
	        {"persistent", persistent(*shared.options.keys)},
	        {"providers", list}};
}

/// @brief intelligence.save.
/// @param shared The natives' parts.
/// @param arguments `{ provider, model?, baseUrl?, key? }`.
/// @return As intelligence.providers.
json save(Shared& shared, const json& arguments) {
	const Provider& provider = providerOf(shared, arguments);
	Settings settings = Settings::load(shared.dataDir);
	ProviderChoice& choice = settings.providers[provider.id];
	const std::string url = argument(arguments, "baseUrl");
	if(!url.empty() && provider.kind == Kind::OpenAICompatible) {
		const std::string cleaned = cleanBaseUrl(url);
		const std::string defaultUrl = withoutSlash(provider.baseUrl);
		// A key saved for one server never goes to another: a new base URL (the saved one, else the catalogue's)
		// erases it, before anything else is saved
		if(cleaned != (choice.baseUrl.empty() ? defaultUrl : withoutSlash(choice.baseUrl))) {
			shared.options.keys->erase(keyName(provider.id));
			if(shared.options.keys->read(keyName(provider.id)).has_value())
				throw std::runtime_error("the key for the old base URL couldn't be erased from " +
				                         shared.options.keys->name());
		}
		choice.baseUrl = cleaned == defaultUrl ? "" : cleaned;
	}
	const std::string model = argument(arguments, "model");
	if(model.size() > 200) throw std::runtime_error("the model's name is too long");
	if(!model.empty()) choice.model = model;
	std::string key = argument(arguments, "key");
	{
		const std::scoped_lock lock(shared.mutex);
		const auto staged = shared.staged.find(provider.id);
		if(key.empty() && staged != shared.staged.end()) key = staged->second;
		shared.staged.erase(provider.id);
	}
	if(!key.empty() && provider.kind != Kind::ClaudeCode) {
		key = cleanKey(std::move(key));
		const std::optional<Endpoint> after = endpointOf(shared.catalog, settings, provider.id);
		if(!key.empty() && (!after || !writeKey(*shared.options.keys, *after, key)))
			throw std::runtime_error("the key couldn't be kept in " + shared.options.keys->name());
	}
	settings.provider = provider.id;
	settings.model = choice.model.empty() ? provider.model : choice.model;
	settings.save(shared.dataDir);
	if(shared.options.changed) shared.options.changed();
	return providersJson(shared);
}

/// @brief intelligence.models, on a worker thread.
/// @param shared The natives' parts.
/// @param arguments `{ provider }`.
/// @param call The call in progress.
/// @return `[{ id, name }]`.
json models(const Shared& shared, const json& arguments, affresco::CallProgress& call) {
	const Provider& provider = providerOf(shared, arguments);
	const auto fallback = [&provider] { return fallbackOf(provider); };
	if(provider.kind == Kind::ClaudeCode) return fallback();
	std::optional<Endpoint> endpoint = endpointOf(shared.catalog, Settings::load(shared.dataDir), provider.id);
	if(!endpoint || checkBaseUrl(*endpoint)) return fallback();
	endpoint->key = readKey(*shared.options.keys, *endpoint);
	if(provider.needsKey && endpoint->key.empty()) return fallback();
	const std::optional<std::string> origin = originOf(endpoint->baseUrl);
	if(!origin) return fallback();
	try {
		HttpSession http(shared.options.network, *origin);
		const Outcome outcome = http.fetch(modelsRequest(*endpoint), [&call] { return call.cancelled(); });
		const auto* response = std::get_if<affresco::network::Response>(&outcome);
		if(response == nullptr) return fallback();
		json list = json::array();
		for(const Model& model : parseModels(provider.kind, response->body))
			list.push_back({{"id", model.id}, {"name", model.name}});
		return list.empty() ? fallback() : list;
	} catch(const std::exception&) {
		return fallback();
	}
}

/// @brief intelligence.test, on a worker thread: one tiny request to the provider in use.
/// @param shared The natives' parts.
/// @param call The call in progress.
/// @return `{ ok, message }`.
json test(const Shared& shared, affresco::CallProgress& call) {
	std::optional<Endpoint> endpoint = endpointOf(shared.catalog, Settings::load(shared.dataDir));
	if(!endpoint) return {{"ok", false}, {"message", "No provider is chosen yet."}};
	const Provider& provider = *endpoint->provider;
	if(provider.kind == Kind::ClaudeCode) {
		const std::optional<std::filesystem::path> program = findClaude();
		if(!shared.options.claude && !program)
			return {{"ok", false}, {"message", "Claude Code isn't installed (the claude command isn't on the PATH)."}};
		const auto stopped = [&call] { return call.cancelled(); };
		const ClaudeAnswer answer =
		    shared.options.claude
		        ? shared.options.claude(endpoint->model, "Answer with the word OK.", "Say OK.", stopped)
		        : runClaude(*program, endpoint->model, "Answer with the word OK.", "Say OK.", std::chrono::seconds(90),
		                    stopped);
		return {{"ok", answer.ok},
		        {"message", answer.ok ? "Connected: Claude Code answered with " + endpoint->model + "." : answer.text}};
	}
	if(const std::optional<std::string> refused = checkBaseUrl(*endpoint))
		return {{"ok", false}, {"message", *refused}};
	endpoint->key = readKey(*shared.options.keys, *endpoint);
	if(provider.needsKey && endpoint->key.empty())
		return {{"ok", false}, {"message", provider.name + " needs an API key."}};
	if(endpoint->model.empty()) return {{"ok", false}, {"message", "Choose a model first."}};
	const std::optional<std::string> origin = originOf(endpoint->baseUrl);
	if(!origin) return {{"ok", false}, {"message", "The base URL isn't an http(s) URL."}};
	try {
		HttpSession http(shared.options.network, *origin);
		Conversation conversation{"Answer with the word OK.", json::array()};
		addUser(provider.kind, conversation, "Say OK.");
		affresco::network::Request request = chatRequest(*endpoint, conversation, {});
		request.timeout = std::chrono::milliseconds(60000);
		const Outcome outcome = http.fetch(std::move(request), [&call] { return call.cancelled(); });
		if(const auto* failure = std::get_if<affresco::network::Failure>(&outcome))
			return {{"ok", false}, {"message", describeFailure(*failure, provider.name, endpoint->key)}};
		(void)parseTurn(provider.kind, std::get<affresco::network::Response>(outcome).body);
		return {{"ok", true}, {"message", "Connected: " + endpoint->model + " answered."}};
	} catch(const std::exception& error) {
		return {{"ok", false}, {"message", redact(error.what(), endpoint->key)}};
	}
}

} // namespace

void registerCalls(affresco::Screen& screen, const std::filesystem::path& dataDir, CallsOptions options) {
	auto shared = std::make_shared<Shared>();
	shared->dataDir = dataDir;
	shared->catalog = Catalog::standard();
	if(!options.keys) options.keys = systemKeys();
	shared->options = std::move(options);

	screen.onCall("intelligence.providers", [shared](const std::string&) { return providersJson(*shared).dump(); });
	screen.onCall("intelligence.stageKey", [shared](const std::string& argumentsJson) {
		const json arguments = json::parse(argumentsJson, nullptr, false);
		const Provider& provider = providerOf(*shared, arguments);
		std::string key = argument(arguments, "key");
		if(key.size() > 4096) throw std::runtime_error("the key is too long");
		const std::scoped_lock lock(shared->mutex);
		if(key.empty()) shared->staged.erase(provider.id);
		else shared->staged[provider.id] = std::move(key);
		return std::string("true");
	});
	screen.onCall("intelligence.save", [shared](const std::string& argumentsJson) {
		return save(*shared, json::parse(argumentsJson, nullptr, false)).dump();
	});
	screen.onCall("intelligence.forget", [shared](const std::string& argumentsJson) {
		const Provider& provider = providerOf(*shared, json::parse(argumentsJson, nullptr, false));
		shared->options.keys->erase(keyName(provider.id));
		{
			const std::scoped_lock lock(shared->mutex);
			shared->staged.erase(provider.id);
		}
		if(shared->options.changed) shared->options.changed();
		return providersJson(*shared).dump();
	});
	screen.onCall("intelligence.openKeyHelp", [shared](const std::string& argumentsJson) {
		// Only the catalogue's own page of a provider: a screen can't make it open a URL of its choosing
		const Provider& provider = providerOf(*shared, json::parse(argumentsJson, nullptr, false));
		if(provider.keyHelp.starts_with("https://") && affresco::net::isSignInPage(provider.keyHelp))
			affresco::net::openInBrowser(provider.keyHelp);
		return std::string("true");
	});
	screen.onAsyncCall("intelligence.models", [shared](const std::string& argumentsJson, affresco::CallProgress& call) {
		return models(*shared, json::parse(argumentsJson, nullptr, false), call).dump();
	});
	screen.onAsyncCall("intelligence.test", [shared](const std::string&, affresco::CallProgress& call) {
		return test(*shared, call).dump();
	});
}

HostAssistant::HostAssistant(affresco::Executor& executor, std::filesystem::path dataDir)
    : executor_(executor), dataDir_(std::move(dataDir)) {}

bool HostAssistant::attach(affresco::Screen& screen) {
	if(agent_) return true;
	try {
		AgentOptions options;
		options.dataDir = dataDir_;
		options.catalog = Catalog::standard();
		if(!endpointOf(options.catalog, Settings::load(dataDir_))) return false;
		options.guide = readFormatGuide();
		options.keys = systemKeys();
		agent_ = std::make_unique<ProviderAgent>(executor_, std::move(options));
	} catch(const std::exception& error) {
		std::cerr << "affrescoui_host: the assistant's agent can't start: " << error.what() << '\n';
		return false;
	}
	screen.connect(*agent_, "agent");
	return true;
}

void HostAssistant::enableSettings(affresco::Screen& screen) { registerCalls(screen, dataDir_); }

void askOnStart(affresco::Executor& executor, affresco::Screen& screen, affresco::Window& window, std::string text) {
	executor.post([&screen, &window, text = std::move(text)] {
		screen.runAction("assistant.start", window.root(), json{{"text", text}}.dump());
	});
}

} // namespace examples::intelligence
